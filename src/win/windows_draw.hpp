#pragma once
#include <cstddef>

namespace uagfx::win {

// 3.0.11-beta.53 (the owner: «когда открываешь инвентарь мигает старый на время»): the game's windows the new
// interface replaces are hidden just before the game draws its windows - the game mode's frame (FUN_00559750) calls
// the windows' manager's draw (FUN_004cd720) at 0x559972 after its input opened them (Alt+E makes «Речі» there), while
// the parts' own frames come at the scene's end: a window opened was drawn for that one frame. Through the driver, the
// parts hide theirs first (each part's beforeGameDraw), then the game draws. Once; false: not this build's code.
bool installWindowsDraw();
// 3.0.11-beta.185/.190: the faces in the renderer's interface list ([0x6EE3D8] + 0xEC, drawn in that order after the
// world with its depth off; a face with no vertices skipped) - before its windows (the names over the heads), at its
// scene's begin (all of them: its windows, its cursor; the depth-sorted screen list +0x11C comes after). SIZE_MAX:
// not known
size_t interfaceFaces();

}  // namespace uagfx::win
