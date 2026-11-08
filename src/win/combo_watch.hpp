#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::win::combowatch {

// 3.0.16-beta.13 (the owner's pick 5 of «20 идей»): the character's own combo window (core/combo) - 0x01d2 to the own
// account, the own skills' damage (0x01de, 0x0114). On the game's thread.
void packet(uint16_t id, const uint8_t* packet);
std::vector<uint16_t> next();  // the skills that chain now (none: no window)
float left();                  // its share left, 1 .. 0
// 3.0.16-beta.13 (pick 15): Steel Body on the character - its own cast (0x011a: <skill>.W 2 <level>.W 4 <source>.L
// 10) of 268 lasts 30 s a level (uAthena skill_cast_db); the game shows no icon for it
bool steelBody();

}  // namespace uagfx::win::combowatch
