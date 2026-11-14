#pragma once
#include <windows.h>

#include "core/config.hpp"

namespace uagfx::win::view3dwin {

// 3.0.19-beta.6 (the owner: «давай попробуем, сделай побольше всего ... чтобы сразу получить качественную картинку»;
// core/view3d, plan docs/superpowers/plans/2026-11-14-view3d.md): the 3D view. Ctrl+Shift+V: the game's own camera ->
// behind the character -> from its eyes -> back. The game's camera build (uaRO.exe 0x4faf00) sent here: the game's yaw
// ours, its build, then the eye and the view matrix (+0x94) and its inverse (+0xc4) ours, the renderer's projection
// (0x6EE3D8: f0, f1, the culling's slopes) for the wider field of view - the game draws its world, its sprites and its
// clicks through it. The mouse turns the view: its move from the screen's middle, the game's cursor held there (Alt
// held: a free cursor for the windows); W A S D walk (the walk mode on while in it). On the game's thread.

void install();                          // the camera's hook, once (the game's world not needed yet)
bool key(UINT m, WPARAM wp, LPARAM lp);  // Ctrl+Shift+V; true: ours (the game sees neither the key nor its letter)
// each frame: the cursor held in the window while looking, the log; 3.0.19-beta.10: [view3d]'s fields of view and
// mouse speed taken (for the camera's next build)
void frame(const Config& config);
bool active();                           // the 3D view on
bool looking();                          // and the mouse turns it (not Alt): the crosshair in the cursor's place

}  // namespace uagfx::win::view3dwin
