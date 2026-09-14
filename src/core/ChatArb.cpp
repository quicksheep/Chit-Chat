#include "ChatArb.h"

#include "MessageParse.h"

#include <algorithm>
#include <cstring>

namespace chitchat {

bool ValidateArb(const ArbBlock& block) {
	if (block.magic != kArbMagic || block.version != kArbVersion) {
		return false;
	}
	if (block.count > static_cast<std::uint32_t>(kMaxMessages)) {
		return false;
	}
	bool used[kMaxAppearId + 1] = {};
	for (std::uint32_t i = 0; i < block.count; ++i) {
		const ArbMessage& m = block.msgs[i];
		if (m.id < 1 || m.id > kMaxAppearId) {
			return false;
		}
		if (used[m.id]) {
			return false;
		}
		used[m.id] = true;
		if (m.sender > 1) {
			return false;
		}
		if (m.text_bytes >= static_cast<std::uint32_t>(kMaxTextBytes)) {
			return false;
		}
		if (m.text[kMaxTextBytes - 1] != '\0' && m.text_bytes >= static_cast<std::uint32_t>(kMaxTextBytes)) {
			return false;
		}
		std::string s(m.text, m.text + m.text_bytes);
		if (!IsValidUtf8(s, kMaxTextBytes - 1) || s.empty()) {
			return false;
		}
	}
	return true;
}

bool ArbToMessages(const ArbBlock& block, std::vector<Message>* out) {
	if (!out || !ValidateArb(block)) {
		return false;
	}
	out->clear();
	out->reserve(block.count);
	for (std::uint32_t i = 0; i < block.count; ++i) {
		const ArbMessage& m = block.msgs[i];
		Message msg;
		msg.id = m.id;
		msg.sender = (m.sender == 1) ? Sender::Them : Sender::You;
		msg.text.assign(m.text, m.text + m.text_bytes);
		out->push_back(std::move(msg));
	}
	std::sort(out->begin(), out->end(), [](const Message& a, const Message& b) { return a.id < b.id; });
	return true;
}

bool MessagesToArb(const std::vector<Message>& messages, ArbBlock* out) {
	if (!out) {
		return false;
	}
	ClearArb(out);
	if (messages.size() > static_cast<std::size_t>(kMaxMessages)) {
		return false;
	}
	out->count = static_cast<std::uint32_t>(messages.size());
	for (std::uint32_t i = 0; i < out->count; ++i) {
		const Message& src = messages[i];
		if (src.id < 1 || src.id > kMaxAppearId || src.text.empty() || src.text.size() >= static_cast<std::size_t>(kMaxTextBytes)) {
			ClearArb(out);
			return false;
		}
		if (!IsValidUtf8(src.text, kMaxTextBytes - 1)) {
			ClearArb(out);
			return false;
		}
		ArbMessage& dst = out->msgs[i];
		dst.id = src.id;
		dst.sender = (src.sender == Sender::Them) ? 1u : 0u;
		dst.text_bytes = static_cast<std::uint32_t>(src.text.size());
		std::memcpy(dst.text, src.text.data(), dst.text_bytes);
		dst.text[dst.text_bytes] = '\0';
	}
	return ValidateArb(*out);
}

} // namespace chitchat
