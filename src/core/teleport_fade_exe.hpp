#pragma once
#include <cstdint>

namespace uagfx::teleport_fade_exe {

// 3.0.13-beta.7 (the owner's pick 18 of «50 идей»: «Короче затемнение при телепорте внутри карты»): the map change
// 0x0091 (FUN_0057e9b0) to the map the player is on - a teleport, a fly wing - calls FUN_004f5790(0): a loop that
// stops the game for 255 ms while the screen darkens, before the new place is drawn (its fade in, FUN_004f5710, does
// not stop the game and stays). The call with its argument, 9 bytes, made nothing: `push 0; mov ecx, esi; call
// 0x4f5790` - ECX loaded again at once after it (0x57EC1E), the callee's own stack cleaning gone with the push.
constexpr uint32_t kSite = 0x57EC15;
constexpr uint8_t kGame[9] = {0x6A, 0x00, 0x8B, 0xCE, 0xE8, 0x72, 0x6B, 0xF7, 0xFF};
constexpr uint8_t kQuick[9] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};

}  // namespace uagfx::teleport_fade_exe
