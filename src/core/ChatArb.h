#pragma once

#include "ChatConstants.h"
#include "ChatTypes.h"

#include <cstring>
#include <string>
#include <vector>

namespace chitchat {

#pragma pack(push, 1)
struct ArbMessage {
	std::int32_t id;
	std::uint32_t sender; // 0 you, 1 them
	std::uint32_t text_bytes; // not including NUL, < kMaxTextBytes
	std::uint32_t reserved;
	char text[kMaxTextBytes];
};

struct ArbBlock {
	std::uint32_t magic;
	std::uint32_t version;
	std::uint32_t count;
	std::uint32_t reserved;
	ArbMessage msgs[kMaxMessages];
};
#pragma pack(pop)

inline void ClearArb(ArbBlock* b) {
	if (!b) {
		return;
	}
	std::memset(b, 0, sizeof(*b));
	b->magic = kArbMagic;
	b->version = kArbVersion;
}

inline void DefaultArb(ArbBlock* b) {
	ClearArb(b);
	b->count = 3;
	b->msgs[0].id = 1;
	b->msgs[0].sender = 0;
	{
		const char* t = "Hey — are you free later?";
		b->msgs[0].text_bytes = static_cast<std::uint32_t>(std::strlen(t));
		std::memcpy(b->msgs[0].text, t, b->msgs[0].text_bytes);
	}
	b->msgs[1].id = 2;
	b->msgs[1].sender = 1;
	{
		const char* t = "Yeah, what's going on?";
		b->msgs[1].text_bytes = static_cast<std::uint32_t>(std::strlen(t));
		std::memcpy(b->msgs[1].text, t, b->msgs[1].text_bytes);
	}
	b->msgs[2].id = 3;
	b->msgs[2].sender = 0;
	{
		const char* t = "Thought we could grab coffee.";
		b->msgs[2].text_bytes = static_cast<std::uint32_t>(std::strlen(t));
		std::memcpy(b->msgs[2].text, t, b->msgs[2].text_bytes);
	}
}

bool ArbToMessages(const ArbBlock& block, std::vector<Message>* out);
bool MessagesToArb(const std::vector<Message>& messages, ArbBlock* out);
bool ValidateArb(const ArbBlock& block);

} // namespace chitchat
