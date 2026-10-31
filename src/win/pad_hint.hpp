#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::padhintwin {

// 3.0.11-beta.92 (the concept «Контроллер в интерфейсе uaRO», stage 6): the controller's hint line - an NPC's window,
// a support skill asking whom (uapad::runtime::hints) - in the interface's own style at the bottom middle over the
// skill bar (core/pad_hint), while [interface] pad_hints is on. On the game's thread.
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);

}  // namespace uagfx::win::padhintwin
