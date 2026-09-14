#pragma once

#include <string>

namespace chitchat {

// Modal transcript editor. parent may be null (uses the foreground window).
// Returns true if the user confirmed. transcript is UTF-8.
bool EditTranscriptDialog(void* parent_hwnd, std::string* transcript, std::string* error);

} // namespace chitchat
