#pragma once

namespace uagfx::win {

// 0.89: the cell of the ground under the mouse (uaRO.exe 0x564A20, each frame) found by walking the mouse's ray cell
// by cell (core/ground_pick.hpp) instead of testing every cell of every piece of the map it passes over. On each map
// both answers are compared first, and then now and then (pick::Trust); after three disagreements the game's own is
// used from then on. 3.0.2-beta.13: a map's first picks (the game still setting it up) find no cell, neither asked.
// installPickHook(false) ([debug] fast_pick = 0) leaves the game's own; false, with a log line, when the exe is
// another build.
bool installPickHook(bool on);
// 3.0.2-beta.13, a map's loading measured: the picks so far, those checked against the game's own, and the game's own
// time on them (ms)
void pickCounts(unsigned& picks, unsigned& checked, double& theirsMs);

}  // namespace uagfx::win
