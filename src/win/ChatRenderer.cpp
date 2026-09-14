#include "ChatRenderer.h"

#include "ChatConstants.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace chitchat {
namespace {

const wchar_t* kFontNames[] = {
	L"Segoe UI",
	L"Arial",
	L"Calibri",
	L"Roboto",
	L"Georgia",
	L"Courier New"
};

D2D1_COLOR_F Color(float r, float g, float b, float a) {
	D2D1_COLOR_F c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;
	return c;
}

HRESULT MakeRoundRectGeometry(ID2D1Factory* factory, const D2D1_RECT_F& rect, float radius, ID2D1RoundedRectangleGeometry** geo) {
	const float w = rect.right - rect.left;
	const float h = rect.bottom - rect.top;
	float r = std::max(0.0f, radius);
	r = std::min(r, w * 0.5f);
	r = std::min(r, h * 0.5f);
	const D2D1_ROUNDED_RECT rr = { rect, r, r };
	return factory->CreateRoundedRectangleGeometry(rr, geo);
}

void EnsureComOnThisThread() {
	thread_local bool initialized = false;
	if (initialized) {
		return;
	}
	const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE || hr == S_FALSE) {
		initialized = true;
	}
}

} // namespace

bool Utf8ToWide(const std::string& utf8, std::wstring* wide) {
	if (!wide) {
		return false;
	}
	wide->clear();
	if (utf8.empty()) {
		return true;
	}
	if (utf8.size() > static_cast<std::size_t>(kMaxTextBytes * 4)) {
		return false;
	}
	const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	if (needed <= 0) {
		return false;
	}
	wide->assign(static_cast<std::size_t>(needed), L'\0');
	const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), &(*wide)[0], needed);
	if (written != needed) {
		wide->clear();
		return false;
	}
	return true;
}

struct ChatRenderer::Impl {
	ComPtr<ID2D1Factory> d2d;
	ComPtr<IDWriteFactory> dwrite;
	ComPtr<IWICImagingFactory> wic;
};

ChatRenderer::ChatRenderer() = default;

ChatRenderer::~ChatRenderer() {
	Release();
}

void ChatRenderer::Release() {
	ready_ = false;
	delete impl_;
	impl_ = nullptr;
}

bool ChatRenderer::Init() {
	EnsureComOnThisThread();
	if (ready_ && impl_) {
		return true;
	}
	Release();
	impl_ = new (std::nothrow) Impl();
	if (!impl_) {
		return false;
	}
	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, impl_->d2d.GetAddressOf());
	if (FAILED(hr)) {
		Release();
		return false;
	}
	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
							 reinterpret_cast<IUnknown**>(impl_->dwrite.GetAddressOf()));
	if (FAILED(hr)) {
		Release();
		return false;
	}
	hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->wic));
	if (FAILED(hr)) {
		Release();
		return false;
	}
	ready_ = true;
	return true;
}

const wchar_t* ChatRenderer::FontFamily(int font_index) const {
	const int n = static_cast<int>(sizeof(kFontNames) / sizeof(kFontNames[0]));
	if (font_index < 0 || font_index >= n) {
		return kFontNames[0];
	}
	return kFontNames[font_index];
}

bool ChatRenderer::Measure(const StyleSettings& style, const std::string& utf8_text, float max_bubble_width, MeasuredBubble* out) {
	if (!out || !Init()) {
		return false;
	}
	std::wstring wide;
	if (!Utf8ToWide(utf8_text, &wide)) {
		return false;
	}

	const float pad = std::max(0.0f, style.padding);
	const float max_w = std::max(pad * 2.0f + 8.0f, max_bubble_width);
	const float inner = std::max(8.0f, max_w - pad * 2.0f);
	const float font_size = std::max(4.0f, style.font_size);
	const DWRITE_FONT_WEIGHT weight = style.bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_REGULAR;

	ComPtr<IDWriteTextFormat> format;
	HRESULT hr = impl_->dwrite->CreateTextFormat(
		FontFamily(style.font_index),
		nullptr,
		weight,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		font_size,
		L"en-us",
		&format);
	if (FAILED(hr)) {
		hr = impl_->dwrite->CreateTextFormat(L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
											 DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-us", &format);
	}
	if (FAILED(hr)) {
		return false;
	}
	format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
	format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

	ComPtr<IDWriteTextLayout> layout;
	hr = impl_->dwrite->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), format.Get(), inner, 8192.0f, &layout);
	if (FAILED(hr)) {
		return false;
	}

	DWRITE_TEXT_METRICS metrics{};
	hr = layout->GetMetrics(&metrics);
	if (FAILED(hr)) {
		return false;
	}

	const float text_w = std::max(1.0f, metrics.widthIncludingTrailingWhitespace);
	const float text_h = std::max(font_size, metrics.height);
	out->width = std::min(max_w, text_w + pad * 2.0f);
	out->height = text_h + pad * 2.0f;
	out->text = utf8_text;
	return true;
}

bool ChatRenderer::Render(const StyleSettings& style, const std::vector<DrawBubble>& bubbles, int width, int height, BitmapBGRA* out) {
	if (!out || width <= 0 || height <= 0 || width > kMaxRenderWidth || height > kMaxRenderHeight) {
		return false;
	}
	const std::uint64_t pixel_bytes = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4ull;
	if (pixel_bytes > (256ull * 1024ull * 1024ull)) {
		return false;
	}
	if (!Init()) {
		return false;
	}

	const UINT32 w = static_cast<UINT32>(width);
	const UINT32 h = static_cast<UINT32>(height);
	ComPtr<IWICBitmap> bitmap;
	HRESULT hr = impl_->wic->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bitmap);
	if (FAILED(hr)) {
		return false;
	}

	// Software/WARP target: never a swap chain, never a hardware D3D device per frame.
	const D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_SOFTWARE,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
		96.0f,
		96.0f);

	ComPtr<ID2D1RenderTarget> rt;
	hr = impl_->d2d->CreateWicBitmapRenderTarget(bitmap.Get(), rtp, &rt);
	if (FAILED(hr)) {
		return false;
	}

	rt->BeginDraw();
	rt->Clear(D2D1::ColorF(0, 0, 0, 0));
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

	const float pad = std::max(0.0f, style.padding);
	const float font_size = std::max(4.0f, style.font_size);
	const DWRITE_FONT_WEIGHT weight = style.bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_REGULAR;

	ComPtr<IDWriteTextFormat> format;
	hr = impl_->dwrite->CreateTextFormat(FontFamily(style.font_index), nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
										 DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-us", &format);
	if (FAILED(hr)) {
		hr = impl_->dwrite->CreateTextFormat(L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
											 DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-us", &format);
	}
	if (FAILED(hr)) {
		rt->EndDraw();
		return false;
	}
	format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
	format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

	for (const auto& b : bubbles) {
		if (b.opacity <= 0.001f) {
			continue;
		}
		if (b.width < 1.0f || b.height < 1.0f) {
			continue;
		}

		const D2D1_RECT_F rect = D2D1::RectF(b.x, b.y, b.x + b.width, b.y + b.height);
		ComPtr<ID2D1RoundedRectangleGeometry> geo;
		if (FAILED(MakeRoundRectGeometry(impl_->d2d.Get(), rect, style.corner_radius, &geo))) {
			continue;
		}

		const bool them = (b.sender == Sender::Them);
		const float br = them ? style.them_bubble_r : style.you_bubble_r;
		const float bg = them ? style.them_bubble_g : style.you_bubble_g;
		const float bb = them ? style.them_bubble_b : style.you_bubble_b;
		const float ba = (them ? style.them_bubble_a : style.you_bubble_a) * b.opacity;
		const float tr = them ? style.them_text_r : style.you_text_r;
		const float tg = them ? style.them_text_g : style.you_text_g;
		const float tb = them ? style.them_text_b : style.you_text_b;
		const float ta = (them ? style.them_text_a : style.you_text_a) * b.opacity;

		ComPtr<ID2D1SolidColorBrush> fill;
		if (FAILED(rt->CreateSolidColorBrush(Color(br, bg, bb, ba), &fill))) {
			continue;
		}
		rt->FillGeometry(geo.Get(), fill.Get());

		std::wstring wide;
		if (!Utf8ToWide(b.text, &wide) || wide.empty()) {
			continue;
		}

		ComPtr<ID2D1SolidColorBrush> text_brush;
		if (FAILED(rt->CreateSolidColorBrush(Color(tr, tg, tb, ta), &text_brush))) {
			continue;
		}

		const D2D1_RECT_F text_rect = D2D1::RectF(b.x + pad, b.y + pad, b.x + b.width - pad, b.y + b.height - pad);
		rt->DrawText(wide.c_str(), static_cast<UINT32>(wide.size()), format.Get(), text_rect, text_brush.Get(),
					 D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
	}

	hr = rt->EndDraw();
	if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET) {
		return false;
	}

	const UINT stride = w * 4u;
	std::vector<std::uint8_t> pixels;
	try {
		pixels.resize(static_cast<std::size_t>(stride) * static_cast<std::size_t>(h), 0);
	} catch (...) {
		return false;
	}
	WICRect rc;
	rc.X = 0;
	rc.Y = 0;
	rc.Width = static_cast<INT>(w);
	rc.Height = static_cast<INT>(h);
	hr = bitmap->CopyPixels(&rc, stride, static_cast<UINT>(pixels.size()), pixels.data());
	if (FAILED(hr)) {
		return false;
	}

	out->width = width;
	out->height = height;
	out->stride = static_cast<int>(stride);
	out->pixels.swap(pixels);
	return true;
}

} // namespace chitchat
