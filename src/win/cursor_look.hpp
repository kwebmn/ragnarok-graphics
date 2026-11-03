#pragma once
#include <windows.h>

#include "core/config.hpp"
#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::cursorlook {

// 3.0.11-beta.198 (the tester: «курсор миші стандартний не дуже підходить в загальний стиль»; the owner: «да, сделай
// возможность курсор менять»): [interface] cursor - the game's cursor (its faces of the colour kCursorColour at its
// point: FUN_004f60e0) out of the frame, the interface's (core/cursor_art) drawn at its point last, over everything,
// the shape of the game's cursor action now (the mode's + 0x38); none where the game drew none (a map loading, the
// controller playing). On the game's thread.
void frame(CursorLook look, double k);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::cursorlook
