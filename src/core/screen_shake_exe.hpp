#pragma once
#include <cstdint>

namespace uagfx::screen_shake_exe {

// 3.0.13-beta.8 (the owner's pick 2 of «50 идей»: «Выключатель тряски экрана от Earthquake и подобных скиллов»): the
// game mode's message 0x87 (Earthquake's effect 0xAF at its ticks 0, 35, 70 and 28 more senders) starts the camera's
// shake by FUN_004fa830 (thiscall, 5 arguments: its start time, strength and length into the camera's shake state) -
// its first bytes `push ebp; mov ebp, esp` made `ret 0x14`: the shake never starts, all else as it was
constexpr uint32_t kShakeStart = 0x4FA830;
constexpr uint8_t kGame[3] = {0x55, 0x8B, 0xEC};
constexpr uint8_t kNone[3] = {0xC2, 0x14, 0x00};

}  // namespace uagfx::screen_shake_exe
