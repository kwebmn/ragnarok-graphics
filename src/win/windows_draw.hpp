#pragma once

namespace uagfx::win {

// 3.0.11-beta.53 (the owner: «когда открываешь инвентарь мигает старый на время»): the game's windows the new
// interface replaces are hidden just before the game draws its windows - the game mode's frame (FUN_00559750) calls
// the windows' manager's draw (FUN_004cd720) at 0x559972 after its input opened them (Alt+E makes «Речі» there), while
// the parts' own frames come at the scene's end: a window opened was drawn for that one frame. Through the driver, the
// parts hide theirs first (each part's beforeGameDraw), then the game draws. Once; false: not this build's code.
bool installWindowsDraw();

}  // namespace uagfx::win
