#pragma once

#include "ChatTypes.h"

#include <string>
#include <vector>

namespace chitchat {

// Transcript syntax (UTF-8):
//   me: Hello
//   them: Hi there
// Extra lines after a message stay in that bubble until the next me:/them: line.
// # comments and leading blank lines are ignored.
// Optional explicit id:  3 | them | See you soon
bool ParseTranscript(const std::string& transcript, std::vector<Message>* out, std::string* error);

std::string FormatTranscript(const std::vector<Message>& messages);

// Rejects overlong encodings, NULs, and bytes that are not valid UTF-8.
bool IsValidUtf8(const std::string& s, std::size_t max_bytes);

// Collapse CR, reject NUL. Does not allow unbounded growth.
std::string SanitizeTranscript(const std::string& raw, std::size_t max_bytes);

} // namespace chitchat
