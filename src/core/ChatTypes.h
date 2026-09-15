#pragma once

#include "ChatConstants.h"

#include <string>
#include <vector>

namespace chitchat {

enum class Sender : std::uint32_t {
	You = 0,   // right-aligned, "owner" of the phone
	Them = 1   // left-aligned, the other person
};

struct Message {
	int id = 0; // 1-based
	Sender sender = Sender::You;
	std::string text; // UTF-8, no NUL
};

struct StyleSettings {
	float corner_radius = kDefaultRadius;
	float padding_x = kDefaultPadding;
	float padding_y = kDefaultPadding;
	float spacing = kDefaultSpacing;
	float max_width_pct = kDefaultMaxWidthPct;
	float margin_x = kDefaultMarginX;
	float margin_y = kDefaultMarginY;
	float font_size = kDefaultFontSize;
	int font_index = 0;
	bool bold = false;
	float letter_spacing = kDefaultLetterSpacing;
	float line_height = kDefaultLineHeight;
	char custom_font[kMaxFontNameBytes] = {};

	float fade_in_sec = kDefaultFadeSec;
	float appear_offset_px = kDefaultOffsetPx;

	float you_bubble_top_r = 0.0f, you_bubble_top_g = 0.478f, you_bubble_top_b = 1.0f, you_bubble_top_a = 1.0f;
	float you_bubble_bot_r = 0.0f, you_bubble_bot_g = 0.478f, you_bubble_bot_b = 1.0f, you_bubble_bot_a = 1.0f;
	float you_text_r = 1.0f, you_text_g = 1.0f, you_text_b = 1.0f, you_text_a = 1.0f;
	float you_stroke = kDefaultStroke;
	float you_stroke_r = 0.0f, you_stroke_g = 0.0f, you_stroke_b = 0.0f, you_stroke_a = 1.0f;

	float them_bubble_top_r = 0.898f, them_bubble_top_g = 0.898f, them_bubble_top_b = 0.918f, them_bubble_top_a = 1.0f;
	float them_bubble_bot_r = 0.898f, them_bubble_bot_g = 0.898f, them_bubble_bot_b = 0.918f, them_bubble_bot_a = 1.0f;
	float them_text_r = 0.0f, them_text_g = 0.0f, them_text_b = 0.0f, them_text_a = 1.0f;
	float them_stroke = kDefaultStroke;
	float them_stroke_r = 0.0f, them_stroke_g = 0.0f, them_stroke_b = 0.0f, them_stroke_a = 1.0f;
};

inline float SenderStrokeWidth(const StyleSettings& style, Sender sender) {
	const float w = (sender == Sender::Them) ? style.them_stroke : style.you_stroke;
	return w > 0.0f ? w : 0.0f;
}

inline float BubbleInnerPadX(const StyleSettings& style, Sender sender) {
	const float pad = style.padding_x > 0.0f ? style.padding_x : 0.0f;
	return pad + SenderStrokeWidth(style, sender) * 0.5f;
}

inline float BubbleInnerPadY(const StyleSettings& style, Sender sender) {
	const float pad = style.padding_y > 0.0f ? style.padding_y : 0.0f;
	return pad + SenderStrokeWidth(style, sender) * 0.5f;
}

struct MeasuredBubble {
	int id = 0;
	Sender sender = Sender::You;
	std::string text;
	float width = 0.0f;
	float height = 0.0f;
};

struct DrawBubble {
	int id = 0;
	Sender sender = Sender::You;
	std::string text;
	float x = 0.0f; // left
	float y = 0.0f; // top
	float width = 0.0f;
	float height = 0.0f;
	float opacity = 1.0f;
};

struct AppearEvent {
	int id = 0;          // message id that first became satisfied
	double time_sec = 0; // composition time
};

} // namespace chitchat
