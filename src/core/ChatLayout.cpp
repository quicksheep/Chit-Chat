#include "ChatLayout.h"

#include <algorithm>
#include <cmath>

namespace chitchat {

float Smoothstep(float t) {
	t = std::max(0.0f, std::min(1.0f, t));
	return t * t * (3.0f - 2.0f * t);
}

std::vector<DrawBubble> LayoutBubbles(const LayoutInput& in) {
	std::vector<DrawBubble> out;
	const float w = in.layer_w;
	const float h = in.layer_h;
	if (w <= 1.0f || h <= 1.0f || in.bubbles.empty()) {
		return out;
	}

	const float fade = std::max(0.0f, in.style.fade_in_sec);
	const float offset = std::max(0.0f, in.style.appear_offset_px);
	const float spacing = std::max(0.0f, in.style.spacing);
	const float margin_x = std::max(0.0f, in.style.margin_x);
	const float margin_y = std::max(0.0f, in.style.margin_y);

	const int n = static_cast<int>(in.bubbles.size());
	if (static_cast<int>(in.age_sec.size()) != n) {
		return out;
	}

	// Resting stack from the bottom (newest last in the input vector).
	std::vector<float> rest_y(static_cast<std::size_t>(n), 0.0f);
	float cursor = h - margin_y;
	for (int i = n - 1; i >= 0; --i) {
		const float bh = std::max(1.0f, in.bubbles[static_cast<std::size_t>(i)].height);
		cursor -= bh;
		rest_y[static_cast<std::size_t>(i)] = cursor;
		cursor -= spacing;
	}

	// Newest appear-time among still-fading bubbles (smallest age).
	int incoming_first = n;
	double min_age = 1.0e30;
	if (fade > 0.0f) {
		for (int i = 0; i < n; ++i) {
			const double age = in.age_sec[static_cast<std::size_t>(i)];
			if (age >= 0.0 && age < static_cast<double>(fade)) {
				if (age < min_age - 1.0e-6) {
					min_age = age;
					incoming_first = i;
				}
			}
		}
	}

	// Incoming group: consecutive bubbles that share this appear age (skipped IDs).
	int incoming_last = incoming_first - 1;
	float incoming_block = 0.0f;
	float progress = 1.0f;
	if (incoming_first < n) {
		incoming_last = incoming_first;
		while (incoming_last + 1 < n) {
			const double a0 = in.age_sec[static_cast<std::size_t>(incoming_first)];
			const double a1 = in.age_sec[static_cast<std::size_t>(incoming_last + 1)];
			if (std::abs(a1 - a0) > 1.0e-4) {
				break;
			}
			++incoming_last;
		}
		for (int i = incoming_first; i <= incoming_last; ++i) {
			incoming_block += std::max(1.0f, in.bubbles[static_cast<std::size_t>(i)].height);
			if (i < incoming_last) {
				incoming_block += spacing;
			}
		}
		// Spacing above the incoming group that older bubbles must yield.
		incoming_block += spacing;
		if (fade <= 0.0f) {
			progress = 1.0f;
		} else {
			progress = Smoothstep(static_cast<float>(min_age / static_cast<double>(fade)));
		}
	}

	out.reserve(static_cast<std::size_t>(n));
	for (int i = 0; i < n; ++i) {
		const MeasuredBubble& m = in.bubbles[static_cast<std::size_t>(i)];
		DrawBubble d;
		d.id = m.id;
		d.sender = m.sender;
		d.text = m.text;
		d.width = m.width;
		d.height = m.height;

		float y = rest_y[static_cast<std::size_t>(i)];
		float opacity = 1.0f;

		const bool incoming = (i >= incoming_first && i <= incoming_last);
		if (incoming) {
			y += offset * (1.0f - progress);
			opacity = progress;
		} else if (incoming_first < n && i < incoming_first) {
			y += incoming_block * (1.0f - progress);
		}

		if (d.sender == Sender::Them) {
			d.x = margin_x;
		} else {
			d.x = w - margin_x - d.width;
		}

		d.y = y;
		d.opacity = std::max(0.0f, std::min(1.0f, opacity));
		out.push_back(std::move(d));
	}

	return out;
}

} // namespace chitchat
