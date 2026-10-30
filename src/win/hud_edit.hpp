#pragma once
#include <windows.h>

#include "core/hud_layout.hpp"
#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::hudedit {

// 3.0.11-beta.75 (the owner: «некоторые интерфейсы хотелось бы чтобы можно было бы двигать и настраивать»):
// «Розміщення інтерфейсу» - Ctrl+Shift+L (or the console's row) shows each part of the new interface in a gold frame
// with its name: dragged with the mouse it moves (snapping to the screen's edges and middle and to the other parts),
// the wheel sizes it 60-160 %, a right click puts it back, «Скинути все» puts them all back, «Готово» or Esc ends it.
// The world is dimmed and takes no clicks meanwhile. Kept in uaro-graphics\hud-layout.txt (core/hud_layout).

// Each part, each frame: its size (multiplies its scale), then its picture's top left moved where the player put it
// (kept on the screen) - its box known to the editor. k: the target's pixels a game pixel
float sizeOf(hudlayout::Part part);
void place(hudlayout::Part part, float k, int targetW, int targetH, int w, int h, int& x, int& y);
// Changes when the part's place or size does (a part that lays out only when it paints paints again)
int stampOf(hudlayout::Part part);

bool on();
void open(HWND game, const char* by);
void frame(HWND game, double k, int targetW, int targetH);  // after the parts' own
void draw(FrameRecord& record, gpu::Renderer& renderer);    // over them
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);  // all of the mouse's while on
bool key(UINT m, WPARAM wp, LPARAM lp);                                // Ctrl+Shift+L, Esc

}  // namespace uagfx::win::hudedit
