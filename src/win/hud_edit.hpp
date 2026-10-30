#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"
#include "core/hud_layout.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::hudedit {

// 3.0.11-beta.75 (the owner: «некоторые интерфейсы хотелось бы чтобы можно было бы двигать и настраивать»): the new
// interface's parts placed and sized by the player, kept in uaro-graphics\hud-layout.txt (core/hud_layout).
// 3.0.11-beta.77 (the owner: «хотел чтобы без быстрых кнопок можно было двигать, чтобы сразу все мышкой», «некоторые
// окна налазят одно на другое и мешают»): straight with the mouse - a press on a part held till the pointer moves
// (then the part is dragged, snapping to the screen and the others, let go clear of them) or is let go (then it is the
// part's click after all); a grip at the corner of the part under the pointer sizes it 60-160 % (twice: as it was).
// [interface] lock_layout keeps them where they are. Ctrl+Shift+L (or the console's row) still shows them all framed.

// Each part, each frame: its size (multiplies its scale), then its picture's top left moved where the player put it
// (kept on the screen) - its box known here. k: the target's pixels a game pixel
float sizeOf(hudlayout::Part part);
void place(hudlayout::Part part, float k, int targetW, int targetH, int w, int h, int& x, int& y);
// Changes when the part's place or size does (a part that lays out only when it paints paints again)
int stampOf(hudlayout::Part part);
bool movedOf(hudlayout::Part part);  // 3.0.11-beta.79: put elsewhere by the player
void setLocked(bool locked);  // [interface] lock_layout, each frame

bool on();
void open(HWND game, const char* by);
void frame(HWND game, double k, int targetW, int targetH);  // after the parts' own
void draw(FrameRecord& record, gpu::Renderer& renderer);    // over them
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);  // all of the mouse's while framed
// 3.0.11-beta.77: before the parts' mouse - Take: the message is the drag's (held, moved, let go); PressFirst: the
// press held is the parts' after all - give them heldPress() first, then this message
enum class Gate : uint8_t { Pass, Take, PressFirst };
Gate gate(HWND game, UINT m, WPARAM wp, LPARAM lp);
void heldPress(UINT& m, WPARAM& wp, LPARAM& lp);
bool key(UINT m, WPARAM wp, LPARAM lp);  // Ctrl+Shift+L, Esc

}  // namespace uagfx::win::hudedit
