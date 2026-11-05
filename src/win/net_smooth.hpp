#pragma once
#include <cstdint>

namespace uagfx::win::netsmoothwin {

// 3.0.11-beta.123 (the network plan's step 3): [interface] smooth_fixes - the server's position fix (0x0088) of a unit
// 1-3 cells from where the game shows it: the packet's cell made the unit's own (the game stops it where it is, no
// jump) and the unit drawn gliding to the server's cell over 160-240 ms (core/net_smooth) - its world place (+4) set
// along the way before each frame is drawn (Device7::BeginScene); the camera follows the character's. A unit that
// walks on meanwhile (the game moved it) is left to the game. On the game's thread.
void setOn(bool on);
// The packet's unit, cell and the packet itself before the game reads it: how far the unit stood (cells; -1 not
// known) - the packet's cell changed to where it stands when it glides
int fix(uint32_t id, uint8_t* packet);
// 3.0.11-beta.124 (step 4): a move of another unit (0x0086) before the game takes it - where it is shown now kept: a
// jump of 0.4-4 cells when the game walks it on (late: from where it should be by now; or from another cell) drawn away
// over 140-280 ms, an offset from the game's own place fading out (the character's own not: the camera follows it)
void moveSeen(uint32_t id);
// 3.0.11-beta.251 (R-YB4S2K): another unit's walk (0x0086, the packet) and its death or leaving (0x0080) - kept to
// give the walk to the game again when its shown hit's flinch left the unit standing (core/net_smooth stranded)
void walkSeen(uint32_t id, const uint8_t* packet);
void gone(uint32_t id);
void beginScene();

}  // namespace uagfx::win::netsmoothwin
