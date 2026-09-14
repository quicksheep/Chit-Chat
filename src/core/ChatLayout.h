#pragma once

#include "ChatTypes.h"

#include <vector>

namespace chitchat {

struct LayoutInput {
	float layer_w = 0.0f;
	float layer_h = 0.0f;
	float downsample = 1.0f; // AE downsample scale already baked into pixel sizes
	StyleSettings style;
	std::vector<MeasuredBubble> bubbles; // already filtered to id <= appear_id, sorted by id
	// Parallel array: seconds since each bubble first became visible. Negative = not yet.
	std::vector<double> age_sec;
};

// Stack from the bottom. Newest message is lowest.
// Incoming bubbles (age < fade_in) start offset downward and fade 0->1.
// Older bubbles are pushed up by the incoming group's occupied height * progress.
// If several IDs appear at the same time (skipped IDs), they animate as one group.
std::vector<DrawBubble> LayoutBubbles(const LayoutInput& in);

float Smoothstep(float t);

} // namespace chitchat
