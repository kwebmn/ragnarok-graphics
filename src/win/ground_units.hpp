#pragma once
#include <cstdint>

#include <vector>

#include "core/ground_timers.hpp"
#include "core/ground_units.hpp"

namespace uagfx::win::groundunitswin {

// 3.0.11-beta.143 (the owner's concept «маг, прист и хант на геймпаде»: walls counted on the hotbar): the character's
// own skill units (core/ground_units) from the game's packets as it reads them (win/packet_tap) - 0x011f a unit seen,
// 0x0120 one gone; a map change (0x0091, 0x0092) clears them. The character is the session's own account. On the
// game's thread.
void packet(uint16_t id, const uint8_t* packet);
const groundunits::Tracker& tracker();
// 3.0.16-beta.21: [interface] ground_timers - the own Safety Wall's, Pneuma's, Sanctuary's and traps' time rings
// (core/ground_timers): a ground skill the character asked for (0x0113, win/ground_marks skillCast), the rings now
void asked(uint16_t skill, int level);
std::vector<groundtimers::Ring> rings();

}  // namespace uagfx::win::groundunitswin
