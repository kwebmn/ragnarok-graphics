#pragma once

namespace uagfx::win::groundthin {

// 3.0.19-beta.42 (item 5 of the owner's «делаем все по порядку»: the speed; core/ground_thin): in the 3D view, while
// the renderer process draws its own ground in place of the game's, the game's ground cells past the reflections'
// casters' reach worked out one frame in 8 (uaRO.exe FUN_004E5E20 sent here: the others go back without a face) - the
// game's thread spared its projections, the driver its records of them. On the game's thread.
bool install();
void setOn(bool on);  // [view3d] fast_ground
void frame();         // the log's tally, each 30 s while any was left out

}  // namespace uagfx::win::groundthin
