#pragma once

#include <cstddef>
#include <cstdint>

namespace chitchat {

inline constexpr char kEffectName[] = "Chit Chat";
inline constexpr char kMatchName[] = "ChitChat";
inline constexpr char kCategory[] = "Text";

inline constexpr std::uint32_t kArbMagic = 0x54484343u; // 'CCHT' little-endian
inline constexpr std::uint32_t kArbVersion = 1;

inline constexpr int kMaxMessages = 64;
inline constexpr int kMaxTextBytes = 1024; // UTF-8, including trailing NUL
inline constexpr int kMaxTranscriptBytes = kMaxMessages * (kMaxTextBytes + 16);

inline constexpr int kMaxAppearId = kMaxMessages;

// Fail render rather than allocating huge CPU/GPU bitmaps (8K cap).
inline constexpr int kMaxRenderWidth = 8192;
inline constexpr int kMaxRenderHeight = 8192;
inline constexpr int kMaxAppearSearchIters = 64;

inline constexpr float kDefaultFontSize = 18.0f;
inline constexpr float kDefaultRadius = 18.0f;
inline constexpr float kDefaultPadding = 14.0f;
inline constexpr float kDefaultSpacing = 10.0f;
inline constexpr float kDefaultFadeSec = 0.25f;
inline constexpr float kDefaultOffsetPx = 24.0f;
inline constexpr float kDefaultMaxWidthPct = 70.0f;
inline constexpr float kDefaultMarginX = 16.0f;
inline constexpr float kDefaultMarginY = 16.0f;

} // namespace chitchat
