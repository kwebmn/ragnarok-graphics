#pragma once
#include <cstdint>

// 3.0.22-beta.4 (idea 3, core/tk_mission): the Taekwon mission's monster told in the game's chat - where it lives, or
// that it lives nowhere on this server - once a monster. The game's thread.
namespace uagfx::win::tkmissionwin {

void packet(uint16_t id, const uint8_t* p);  // the packet tap's, each of the game's packets
void frame(unsigned codePage);              // each frame: a new mission's line, once the reference is loaded

}  // namespace uagfx::win::tkmissionwin
