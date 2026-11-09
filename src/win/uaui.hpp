#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::uaui {

// 3.0.18-beta.7 (the owner: «давай сделаем проверку RmlUi»; Claude Doc «Концепция: единая библиотека интерфейса
// uaRO Graphics», stage 1): RmlUi 6.3 (third_party/rmlui, MIT) in the driver - its geometry as the game's own
// vertices through the bridge (core/uaui_mesh), its words from a GDI glyph atlas in IBM Plex Sans (win/ui_font), its
// clock and log ours. [interface] uaui shows a test card of the character (name, levels, HP, SP) at the top in the
// middle: it fades in, lights under the cursor and is dragged by the mouse. Every 10 s the log tells its cost (update,
// render, the draws) - the numbers the stage's decision is taken by. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
// The mouse's buttons over the card (and the end of a drag wherever it is): true taken
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::uaui
