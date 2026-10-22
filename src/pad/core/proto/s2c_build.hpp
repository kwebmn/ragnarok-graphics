#pragma once
#include <cstdint>

#include "pad/core/bytes.hpp"

namespace uapad {

// Server -> client packets the pad makes up itself. They only ever go to the client, never to the server.
constexpr uint8_t kEmotionGasp = 0;  // "!"

Bytes buildEmotion(uint32_t actor, uint8_t type);  // 0x00c0: an emotion bubble over `actor`
// 0x0229: `actor`'s state (opt1, opt2, option) as the client should draw it.
Bytes buildUnitState(uint32_t actor, uint16_t opt1, uint16_t opt2, uint32_t option);

}  // namespace uapad
