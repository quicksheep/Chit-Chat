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
	float padding = kDefaultPadding;
	float spacing = kDefaultSpacing;
	float max_width_pct = kDefaultMaxWidthPct;
	float margin_x = kDefaultMarginX;
	float margin_y = kDefaultMarginY;
	float font_size = kDefaultFontSize;
	int font_index = 0;
	bool bold = false;

	float fade_in_sec = kDefaultFadeSec;
	float appear_offset_px = kDefaultOffsetPx;

	float you_bubble_r = 0.0f, you_bubble_g = 0.478f, you_bubble_b = 1.0f, you_bubble_a = 1.0f;
	float you_text_r = 1.0f, you_text_g = 1.0f, you_text_b = 1.0f, you_text_a = 1.0f;

	float them_bubble_r = 0.898f, them_bubble_g = 0.898f, them_bubble_b = 0.918f, them_bubble_a = 1.0f;
	float them_text_r = 0.0f, them_text_g = 0.0f, them_text_b = 0.0f, them_text_a = 1.0f;
};

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
