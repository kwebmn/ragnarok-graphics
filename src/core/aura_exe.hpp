#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::aura_exe {

// 3.0.22-beta.31 (R-JBG5MM: an access violation at uaRO.exe+0x1b9878 reading 0 - a level 99 knight's fly wing on
// cmd_fild06, 40 s after «Змінити акаунт»): a character's start (FUN_005518f0) counts the characters made since the
// way into the world (the byte 0x773602, set to 0 by the character select, FUN_005a04a0) and at the third asks the
// world's player - the game mode (GetGameMode 0x4f6ad0 on 0x74b578), its world at +0xc4, its player at +0x3c - to
// show its icons and the 99's aura again (FUN_005b8e70). uaRO.exe's check that the character is the player's own is
// gone (two nops at 0x552908), so any character counts; a same-map teleport (0x0091, FUN_0055b1d0) clears the world's
// player and makes it anew - when that is the third, the world has no player yet, and the aura's call on it faults.
// The call at kCall goes to a guard: no player - nothing; else the game's own function as before.
constexpr uint32_t kAt = 0x552915;    // mov ecx, 0x74b578
constexpr uint32_t kCall = 0x552928;  // call 0x5b8e70
constexpr uint32_t kCallee = 0x5B8E70;
constexpr size_t kOriginalSize = 24;
extern const uint8_t kOriginal[kOriginalSize];  // this build's bytes at kAt, the call last

constexpr size_t kGuardSize = 10;
struct Guard {
    uint8_t code[kGuardSize];
};
// The guard's code where it lands (`at`): test ecx, ecx; jz ret; jmp the game's function (it returns to the game's
// caller); ret
Guard guard(uint32_t at);
// The game's call at kCall, to the guard at `at`
void callTo(uint32_t at, uint8_t out[5]);

}  // namespace uagfx::aura_exe
