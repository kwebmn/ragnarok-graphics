#pragma once
#include <cstdint>
#include <string>

namespace uagfx::win::upkeepwatch {

// 3.0.16-beta.13 (the owner's picks 19, 20 of «20 идей»): broken gear and the pet's and the homunculus's fullness
// (core/upkeep) as the server tells them; a piece breaking or one of them getting hungry a short chime (the arrows')
// when its switch is on - [interface] broken_gear, pet_hunger. On the game's thread.
void packet(uint16_t id, const uint8_t* packet);
void setOn(bool brokenGear, bool petHunger);
// For the character block: the broken pieces («Knife» or their count, "" none), the fullness (< 0 none out or off)
std::string broken(unsigned codePage);
int pet();
int homun();

}  // namespace uagfx::win::upkeepwatch
