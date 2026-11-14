#pragma once
#include <cstdint>

#include "core/config.hpp"
#include "core/window_panel.hpp"

namespace uagfx::cursorart {

// 3.0.11-beta.198 (the tester: «курсор миші стандартний не дуже підходить в загальний стиль»; the owner: «да, сделай
// возможность курсор менять»): the cursor in the interface's style, [interface] cursor - drawn in place of the game's
// (win/cursor_look), a shape for each of its actions: smooth shapes in a 24-unit box, their edge 1.4 units, a soft
// shadow under them; «Темний» dark glass in the interface's gold, «Світлий» white in a dark edge. No Windows here.
// 3.0.19-beta.10: TargetHot - the 3D view's crosshair over a monster (red, a little bolder)
enum class Shape : uint8_t { Arrow, Hand, Grab, Talk, Attack, Rotate, Door, Invalid, Target, TargetHot, kCount };
// The game's cursor actions (data\sprite\cursors.act; the mode's + 0x38, FUN_004f5a50 sets it) as the shapes: 0 the
// arrow, 1 talk, 2 a window's button, 3 a gem falling onto the arrow, 4 the camera turned, 5 and 6 attack, 7 and 12 a
// warp, 8 not there, 9 pick up, 10 and 11 a target, 13 the arrow
Shape shapeOf(int action);
// 3.0.19-beta.10: the 3D view's crosshair (win/view3d holds the game's cursor at the screen's middle) by the game's own
// action there: attack -> the red target; talk, a window's button, a warp, not there, pick up -> the game's shapes;
// the rest (the arrow, the gem, the camera turned, a target) -> the target
Shape lookShapeOf(int action);
struct Art {
    panel::Picture picture;    // premultiplied
    float hotX = 0, hotY = 0;  // the game's cursor's point in the picture
};
// The shape in the look, its 24-unit box `size` pixels wide; the game's look: nothing
Art make(Shape shape, CursorLook look, float size);

}  // namespace uagfx::cursorart
