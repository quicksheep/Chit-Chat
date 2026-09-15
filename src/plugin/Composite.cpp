#include "Composite.h"

#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "SPBasic.h"

#include <algorithm>
#include <cstring>

namespace chitchat {
namespace {

// D2D/WIC overlay is premultiplied BGRA. AE worlds are straight alpha.
// Treating premul as straight makes fades look grey and leaves a dark fringe
// that reads as a phantom stroke.
void StraightFromPremul(const std::uint8_t* src, float* b, float* g, float* r, float* a) {
	const float oa = src[3] / 255.0f;
	*a = oa;
	if (oa <= 0.0005f) {
		*b = *g = *r = 0.0f;
		return;
	}
	*b = (std::min)(1.0f, (src[0] / 255.0f) / oa);
	*g = (std::min)(1.0f, (src[1] / 255.0f) / oa);
	*r = (std::min)(1.0f, (src[2] / 255.0f) / oa);
}

void StraightOver(float sb, float sg, float sr, float sa, float* db, float* dg, float* dr, float* da) {
	const float ia = 1.0f - sa;
	const float outa = sa + (*da) * ia;
	if (outa <= 0.0005f) {
		*db = *dg = *dr = *da = 0.0f;
		return;
	}
	*db = (sb * sa + (*db) * (*da) * ia) / outa;
	*dg = (sg * sa + (*dg) * (*da) * ia) / outa;
	*dr = (sr * sa + (*dr) * (*da) * ia) / outa;
	*da = outa;
}

template <typename Pix, typename Chan>
void OverRow(Pix* dst, const std::uint8_t* src, int count, Chan max_chan) {
	const float scale = static_cast<float>(max_chan);
	for (int x = 0; x < count; ++x) {
		float sb, sg, sr, sa;
		StraightFromPremul(src, &sb, &sg, &sr, &sa);
		if (sa <= 0.0005f) {
			src += 4;
			++dst;
			continue;
		}
		float db = dst->blue / scale;
		float dg = dst->green / scale;
		float dr = dst->red / scale;
		float da = dst->alpha / scale;
		StraightOver(sb, sg, sr, sa, &db, &dg, &dr, &da);
		dst->blue = static_cast<Chan>((std::max)(0.0f, (std::min)(scale, db * scale + 0.5f)));
		dst->green = static_cast<Chan>((std::max)(0.0f, (std::min)(scale, dg * scale + 0.5f)));
		dst->red = static_cast<Chan>((std::max)(0.0f, (std::min)(scale, dr * scale + 0.5f)));
		dst->alpha = static_cast<Chan>((std::max)(0.0f, (std::min)(scale, da * scale + 0.5f)));
		src += 4;
		++dst;
	}
}

void OverRow32(PF_PixelFloat* dst, const std::uint8_t* src, int count) {
	for (int x = 0; x < count; ++x) {
		float sb, sg, sr, sa;
		StraightFromPremul(src, &sb, &sg, &sr, &sa);
		if (sa <= 0.0005f) {
			src += 4;
			++dst;
			continue;
		}
		StraightOver(sb, sg, sr, sa, &dst->blue, &dst->green, &dst->red, &dst->alpha);
		src += 4;
		++dst;
	}
}

PF_Err GetWorldFormat(PF_InData* in_data, PF_EffectWorld* dst, PF_PixelFormat* format) {
	if (!in_data || !in_data->pica_basicP || !dst || !format) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	const void* suiteP = nullptr;
	if (in_data->pica_basicP->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, &suiteP) != kSPNoError || !suiteP) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	auto* wsP = static_cast<const PF_WorldSuite2*>(suiteP);
	PF_Err err = wsP->PF_GetPixelFormat(dst, format);
	in_data->pica_basicP->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);
	return err;
}

int BytesPerPixel(PF_PixelFormat format) {
	switch (format) {
	case PF_PixelFormat_ARGB32:
		return static_cast<int>(sizeof(PF_Pixel));
	case PF_PixelFormat_ARGB64:
		return static_cast<int>(sizeof(PF_Pixel16));
	case PF_PixelFormat_ARGB128:
		return static_cast<int>(sizeof(PF_PixelFloat));
	default:
		return 0;
	}
}

} // namespace

PF_Err ClearWorld(PF_InData* in_data, PF_EffectWorld* dst) {
	if (!dst || !dst->data) {
		return PF_Err_NONE;
	}
	if (dst->width <= 0 || dst->height <= 0 || dst->rowbytes <= 0) {
		return PF_Err_NONE;
	}

	PF_PixelFormat format = PF_PixelFormat_ARGB32;
	PF_Err err = GetWorldFormat(in_data, dst, &format);
	if (err) {
		return err;
	}
	const int bpp = BytesPerPixel(format);
	if (bpp <= 0) {
		return PF_Err_UNRECOGNIZED_PARAM_TYPE;
	}

	const int w = dst->width;
	const int h = dst->height;
	if (dst->rowbytes < w * bpp) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}

	for (int y = 0; y < h; ++y) {
		if (in_data && (y & 15) == 0) {
			ERR(PF_ABORT(in_data));
			if (err) {
				return err;
			}
		}
		char* row = reinterpret_cast<char*>(dst->data) + static_cast<std::size_t>(y) * static_cast<std::size_t>(dst->rowbytes);
		std::memset(row, 0, static_cast<std::size_t>(w) * static_cast<std::size_t>(bpp));
	}
	return PF_Err_NONE;
}

PF_Err CompositeOver(PF_InData* in_data, PF_EffectWorld* dst, const BitmapBGRA& overlay) {
	if (!dst || !dst->data || overlay.pixels.empty()) {
		return PF_Err_NONE;
	}

	PF_PixelFormat format = PF_PixelFormat_ARGB32;
	PF_Err err = GetWorldFormat(in_data, dst, &format);
	if (err) {
		return err;
	}

	if (overlay.width <= 0 || overlay.height <= 0 || overlay.stride < overlay.width * 4) {
		return PF_Err_NONE;
	}
	if (overlay.pixels.size() < static_cast<std::size_t>(overlay.height) * static_cast<std::size_t>(overlay.stride)) {
		return PF_Err_NONE;
	}
	if (dst->width <= 0 || dst->height <= 0 || dst->rowbytes <= 0) {
		return PF_Err_NONE;
	}

	const int w = (std::min)(dst->width, overlay.width);
	const int h = (std::min)(dst->height, overlay.height);
	if (w <= 0 || h <= 0) {
		return PF_Err_NONE;
	}

	int dst_bpp = 0;
	switch (format) {
	case PF_PixelFormat_ARGB32:
		dst_bpp = static_cast<int>(sizeof(PF_Pixel));
		break;
	case PF_PixelFormat_ARGB64:
		dst_bpp = static_cast<int>(sizeof(PF_Pixel16));
		break;
	case PF_PixelFormat_ARGB128:
		dst_bpp = static_cast<int>(sizeof(PF_PixelFloat));
		break;
	default:
		return PF_Err_UNRECOGNIZED_PARAM_TYPE;
	}
	if (dst->rowbytes < w * dst_bpp) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}

	for (int y = 0; y < h; ++y) {
		if ((y & 15) == 0) {
			ERR(PF_ABORT(in_data));
			if (err) {
				return err;
			}
		}
		const std::uint8_t* src = overlay.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(overlay.stride);
		char* row = reinterpret_cast<char*>(dst->data) + static_cast<std::size_t>(y) * static_cast<std::size_t>(dst->rowbytes);
		switch (format) {
		case PF_PixelFormat_ARGB32:
			OverRow(reinterpret_cast<PF_Pixel*>(row), src, w, static_cast<A_u_char>(PF_MAX_CHAN8));
			break;
		case PF_PixelFormat_ARGB64:
			OverRow(reinterpret_cast<PF_Pixel16*>(row), src, w, static_cast<A_u_short>(PF_MAX_CHAN16));
			break;
		case PF_PixelFormat_ARGB128:
			OverRow32(reinterpret_cast<PF_PixelFloat*>(row), src, w);
			break;
		default:
			return PF_Err_UNRECOGNIZED_PARAM_TYPE;
		}
	}
	return PF_Err_NONE;
}

} // namespace chitchat
