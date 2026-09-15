#pragma once

#include "ChatTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace chitchat {

struct BitmapBGRA {
	int width = 0;
	int height = 0;
	int stride = 0; // bytes
	std::vector<std::uint8_t> pixels; // premultiplied B8G8R8A8
};

class ChatRenderer {
public:
	ChatRenderer();
	~ChatRenderer();

	ChatRenderer(const ChatRenderer&) = delete;
	ChatRenderer& operator=(const ChatRenderer&) = delete;

	bool Init();
	void Release();

	const wchar_t* FontFamily(int font_index) const;

	bool Measure(const StyleSettings& style,
				 Sender sender,
				 const std::string& utf8_text,
				 float max_bubble_width,
				 MeasuredBubble* out);

	bool Render(const StyleSettings& style,
				const std::vector<DrawBubble>& bubbles,
				int width,
				int height,
				BitmapBGRA* out);

private:
	bool ready_ = false;
	struct Impl;
	Impl* impl_ = nullptr;
};

bool Utf8ToWide(const std::string& utf8, std::wstring* wide);

} // namespace chitchat
