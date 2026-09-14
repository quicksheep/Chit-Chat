#include "Composite.h"

#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "SPBasic.h"

#include <algorithm>
#include <cstring>

namespace chitchat {
namespace {

template <typename Pix, typename Chan>
void OverRow(Pix* dst, const std::uint8_t* src, int count, Chan max_chan) {
	for (int x = 0; x < count; ++x) {
		const float oa = src[3] / 255.0f;
		if (oa <= 0.0005f) {
			src += 4;
			++dst;
			continue;
		}
		const float ia = 1.0f - oa;
		const float ob = src[0] / 255.0f;
		const float og = src[1] / 255.0f;
		const float or_ = src[2] / 255.0f;
		const float db = dst->blue / static_cast<float>(max_chan);
		const float dg = dst->green / static_cast<float>(max_chan);
		const float dr = dst->red / static_cast<float>(max_chan);
		const float da = dst->alpha / static_cast<float>(max_chan);
		const float outb = ob + db * ia;
		const float outg = og + dg * ia;
		const float outr = or_ + dr * ia;
		const float outa = oa + da * ia;
		dst->blue = static_cast<Chan>((std::max)(0.0f, (std::min)(static_cast<float>(max_chan), outb * max_chan + 0.5f)));
		dst->green = static_cast<Chan>((std::max)(0.0f, (std::min)(static_cast<float>(max_chan), outg * max_chan + 0.5f)));
		dst->red = static_cast<Chan>((std::max)(0.0f, (std::min)(static_cast<float>(max_chan), outr * max_chan + 0.5f)));
		dst->alpha = static_cast<Chan>((std::max)(0.0f, (std::min)(static_cast<float>(max_chan), outa * max_chan + 0.5f)));
		src += 4;
		++dst;
	}
}

void OverRow32(PF_PixelFloat* dst, const std::uint8_t* src, int count) {
	for (int x = 0; x < count; ++x) {
		const float oa = src[3] / 255.0f;
		if (oa <= 0.0005f) {
			src += 4;
			++dst;
			continue;
		}
		const float ia = 1.0f - oa;
		dst->blue = src[0] / 255.0f + dst->blue * ia;
		dst->green = src[1] / 255.0f + dst->green * ia;
		dst->red = src[2] / 255.0f + dst->red * ia;
		dst->alpha = oa + dst->alpha * ia;
		src += 4;
		++dst;
	}
}

} // namespace

PF_Err CompositeOver(PF_InData* in_data, PF_EffectWorld* dst, const BitmapBGRA& overlay) {
	if (!dst || !dst->data || overlay.pixels.empty()) {
		return PF_Err_NONE;
	}

	PF_PixelFormat format = PF_PixelFormat_ARGB32;
	if (!in_data->pica_basicP) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	const void* suiteP = nullptr;
	if (in_data->pica_basicP->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, &suiteP) != kSPNoError || !suiteP) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	auto* wsP = static_cast<const PF_WorldSuite2*>(suiteP);
	PF_Err err = wsP->PF_GetPixelFormat(dst, &format);
	in_data->pica_basicP->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);
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
