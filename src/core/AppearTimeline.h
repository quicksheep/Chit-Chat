#pragma once

#include "ChatTypes.h"

#include <vector>

namespace chitchat {

struct Sample {
	double time_sec = 0.0;
	int appear_id = 0; // hold-interpolated integer
};

// First time (seconds) at which appear_id >= message_id.
// samples must be sorted by time and cover [0, current].
// Returns negative if the message has not appeared yet.
double AppearTimeSec(int message_id, const std::vector<Sample>& samples);

} // namespace chitchat
