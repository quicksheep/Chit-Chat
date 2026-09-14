#include "AppearTimeline.h"

namespace chitchat {

double AppearTimeSec(int message_id, const std::vector<Sample>& samples) {
	if (message_id <= 0 || samples.empty()) {
		return -1.0;
	}
	for (const auto& s : samples) {
		if (s.appear_id >= message_id) {
			return s.time_sec;
		}
	}
	return -1.0;
}

} // namespace chitchat
