#pragma once
#include <cstdint>

namespace uagfx::win::skilldelaywin {

// 3.0.11-beta.276 (the owner's feature 7): the character's own skills as the server tells them done - 0x01de and
// 0x0114 (a skill's damage: <skill>.W at 2, <source>.L at 4, <level>.W at 28 / 26), 0x011a (no damage: <skill>.W 2,
// <level>.W 4, <source>.L 10), 0x0117 (on the ground: <skill>.W 2, <source>.L 4, <level>.W 8) - its delay after it
// begun (core/skill_delay, the character's DEX)
void packet(uint16_t id, const uint8_t* packet);
float left();  // the share of the delay left now (0 none) - the hotbar's sweep

}  // namespace uagfx::win::skilldelaywin
