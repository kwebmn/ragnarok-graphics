#pragma once
#include <cstdint>

namespace uagfx::swear_filter_exe {

// 3.0.13-beta.9 (the owner's pick 25 of «50 идей»: «Отключаемый фильтр брани (manner.txt)»): the game keeps
// manner.txt's words (the filter object 0x74B780) and asks two of its checks (thiscall, the text) whether a text holds
// one anywhere - any part of a word: «pute» in «computer», «anus» in «Janus» - and then refuses to send it; both made
// `xor eax, eax; ret 4`: no word found
constexpr uint32_t kChecks[2] = {0x4FD9A0, 0x4FDAD0};
constexpr uint8_t kGame[5] = {0x55, 0x8B, 0xEC, 0x51, 0x53};    // push ebp; mov ebp, esp; push ecx; push ebx
constexpr uint8_t kNever[5] = {0x33, 0xC0, 0xC2, 0x04, 0x00};   // xor eax, eax; ret 4

}  // namespace uagfx::swear_filter_exe
