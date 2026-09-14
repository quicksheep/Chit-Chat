#include "MessageParse.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace chitchat {
namespace {

std::string Trim(const std::string& s) {
	std::size_t a = 0;
	while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) {
		++a;
	}
	std::size_t b = s.size();
	while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
		--b;
	}
	return s.substr(a, b - a);
}

std::string ToLowerAscii(std::string s) {
	for (char& c : s) {
		if (c >= 'A' && c <= 'Z') {
			c = static_cast<char>(c - 'A' + 'a');
		}
	}
	return s;
}

bool ParseSenderToken(const std::string& token, Sender* out) {
	const std::string t = ToLowerAscii(Trim(token));
	if (t == "me" || t == "you" || t == "right" || t == "self" || t == "owner") {
		*out = Sender::You;
		return true;
	}
	if (t == "them" || t == "other" || t == "left" || t == "friend" || t == "they") {
		*out = Sender::Them;
		return true;
	}
	return false;
}

bool ParsePositiveInt(const std::string& token, int* out) {
	const std::string t = Trim(token);
	if (t.empty() || t.size() > 8) {
		return false;
	}
	int v = 0;
	for (char c : t) {
		if (c < '0' || c > '9') {
			return false;
		}
		v = v * 10 + (c - '0');
		if (v > kMaxAppearId) {
			return false;
		}
	}
	if (v <= 0) {
		return false;
	}
	*out = v;
	return true;
}

std::string UnescapeNewlines(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (std::size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '\\' && i + 1 < s.size()) {
			if (s[i + 1] == 'n') {
				out.push_back('\n');
				++i;
				continue;
			}
			if (s[i + 1] == '\\') {
				out.push_back('\\');
				++i;
				continue;
			}
		}
		out.push_back(s[i]);
	}
	return out;
}

} // namespace

bool IsValidUtf8(const std::string& s, std::size_t max_bytes) {
	if (s.size() > max_bytes) {
		return false;
	}
	const auto* p = reinterpret_cast<const unsigned char*>(s.data());
	const auto* end = p + s.size();
	while (p < end) {
		if (*p == 0) {
			return false;
		}
		if (*p <= 0x7F) {
			++p;
			continue;
		}
		int extra = 0;
		unsigned int cp = 0;
		if ((*p & 0xE0) == 0xC0) {
			extra = 1;
			cp = *p & 0x1F;
			if (cp < 0x02) {
				return false; // overlong
			}
		} else if ((*p & 0xF0) == 0xE0) {
			extra = 2;
			cp = *p & 0x0F;
		} else if ((*p & 0xF8) == 0xF0) {
			extra = 3;
			cp = *p & 0x07;
			if (cp > 0x04) {
				return false;
			}
		} else {
			return false;
		}
		++p;
		for (int i = 0; i < extra; ++i) {
			if (p >= end || (*p & 0xC0) != 0x80) {
				return false;
			}
			cp = (cp << 6) | (*p & 0x3F);
			++p;
		}
		if (extra == 2 && cp < 0x800) {
			return false;
		}
		if (extra == 3 && cp < 0x10000) {
			return false;
		}
		if (cp >= 0xD800 && cp <= 0xDFFF) {
			return false;
		}
		if (cp > 0x10FFFF) {
			return false;
		}
	}
	return true;
}

std::string SanitizeTranscript(const std::string& raw, std::size_t max_bytes) {
	std::string out;
	out.reserve(std::min(raw.size(), max_bytes));
	for (unsigned char c : raw) {
		if (out.size() >= max_bytes) {
			break;
		}
		if (c == 0) {
			continue;
		}
		if (c == '\r') {
			continue;
		}
		out.push_back(static_cast<char>(c));
	}
	return out;
}

bool TryParseMessageHeader(const std::string& trimmed, int auto_id, Message* msg, std::string* error) {
	msg->sender = Sender::You;
	const std::size_t pipe1 = trimmed.find('|');
	const std::size_t colon = trimmed.find(':');

	if (pipe1 != std::string::npos) {
		const std::size_t pipe2 = trimmed.find('|', pipe1 + 1);
		if (pipe2 == std::string::npos) {
			if (error) {
				*error = "Pipe syntax must be: id | me|them | text";
			}
			return false;
		}
		if (!ParsePositiveInt(trimmed.substr(0, pipe1), &msg->id)) {
			if (error) {
				*error = "Message IDs must be integers from 1 to 64.";
			}
			return false;
		}
		if (!ParseSenderToken(trimmed.substr(pipe1 + 1, pipe2 - pipe1 - 1), &msg->sender)) {
			if (error) {
				*error = "Sender must be me/you/right or them/other/left.";
			}
			return false;
		}
		msg->text = UnescapeNewlines(Trim(trimmed.substr(pipe2 + 1)));
		return true;
	}

	if (colon != std::string::npos && colon > 0 && colon < 12 &&
		ParseSenderToken(trimmed.substr(0, colon), &msg->sender)) {
		msg->id = auto_id;
		msg->text = UnescapeNewlines(Trim(trimmed.substr(colon + 1)));
		return true;
	}

	return false;
}

bool CommitMessage(Message* msg, std::vector<Message>* out, bool used[], std::string* error) {
	while (!msg->text.empty() && msg->text.back() == '\n') {
		msg->text.pop_back();
	}
	if (msg->text.empty()) {
		if (error) {
			*error = "Message text cannot be empty.";
		}
		return false;
	}
	if (msg->text.size() >= static_cast<std::size_t>(kMaxTextBytes)) {
		if (error) {
			*error = "A message exceeds the 1023-character limit.";
		}
		return false;
	}
	if (msg->id < 1 || msg->id > kMaxAppearId) {
		if (error) {
			*error = "Message IDs must be between 1 and 64.";
		}
		return false;
	}
	if (used[msg->id]) {
		if (error) {
			*error = "Duplicate message ID.";
		}
		return false;
	}
	if (static_cast<int>(out->size()) >= kMaxMessages) {
		if (error) {
			*error = "Too many messages (max 64).";
		}
		return false;
	}
	used[msg->id] = true;
	out->push_back(std::move(*msg));
	return true;
}

bool ParseTranscript(const std::string& transcript, std::vector<Message>* out, std::string* error) {
	if (!out) {
		return false;
	}
	out->clear();
	if (!IsValidUtf8(transcript, kMaxTranscriptBytes)) {
		if (error) {
			*error = "Transcript is not valid UTF-8 or exceeds the size limit.";
		}
		return false;
	}

	std::istringstream ss(transcript);
	std::string line;
	int auto_id = 1;
	bool used[kMaxAppearId + 1] = {};
	Message current;
	bool have = false;

	while (std::getline(ss, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		const std::string trimmed = Trim(line);
		Message header;
		if (!trimmed.empty() && trimmed[0] != '#' && TryParseMessageHeader(trimmed, auto_id, &header, error)) {
			if (have && !CommitMessage(&current, out, used, error)) {
				out->clear();
				return false;
			}
			if (error) {
				error->clear();
			}
			current = std::move(header);
			have = true;
			auto_id = std::max(auto_id, current.id) + 1;
			continue;
		}

		if (have) {
			current.text.push_back('\n');
			current.text += line;
			if (error) {
				error->clear();
			}
			continue;
		}

		if (trimmed.empty() || trimmed[0] == '#') {
			if (error) {
				error->clear();
			}
			continue;
		}

		if (error) {
			if (error->empty()) {
				*error = "Each message must start with \"me:\" or \"them:\" (or \"1 | them | text\").";
			}
		}
		out->clear();
		return false;
	}

	if (have && !CommitMessage(&current, out, used, error)) {
		out->clear();
		return false;
	}

	std::sort(out->begin(), out->end(), [](const Message& a, const Message& b) { return a.id < b.id; });
	return true;
}

std::string FormatTranscript(const std::vector<Message>& messages) {
	std::ostringstream ss;
	bool sequential = true;
	for (std::size_t i = 0; i < messages.size(); ++i) {
		if (messages[i].id != static_cast<int>(i + 1)) {
			sequential = false;
			break;
		}
	}
	for (const auto& m : messages) {
		if (sequential) {
			ss << (m.sender == Sender::Them ? "them: " : "me: ");
		} else {
			ss << m.id << " | " << (m.sender == Sender::Them ? "them" : "me") << " | ";
		}
		ss << m.text << "\n";
	}
	return ss.str();
}

} // namespace chitchat
