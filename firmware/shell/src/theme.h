// Shell colours (dark theme). Colours are not lengths, so they are not size tokens.
#pragma once
#include <cstdint>

namespace summit::shell::theme {
constexpr uint32_t kBg = 0x0F1317;
constexpr uint32_t kStripBg = 0x1A2027;
constexpr uint32_t kChipBg = 0x262E37;
constexpr uint32_t kText = 0xE6EDF3;
constexpr uint32_t kMuted = 0x9AA7B4;
constexpr uint32_t kAccent = 0x4EA1FF;
constexpr uint32_t kError = 0xFF6B6B;
}  // namespace summit::shell::theme
