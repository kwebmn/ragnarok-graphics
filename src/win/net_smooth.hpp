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
void beginScene();

}  // namespace uagfx::win::netsmoothwin
