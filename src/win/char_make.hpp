#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::charmakewin {

// 3.0.11-beta.153 (task 3 of the remaining windows, the login's part three): [interface] char_make - the game's
// character make (UI window 4: in the manager's windows' list only - g_windowMgr + 0x38, its class 0x697C3C, SendMsg
// FUN_0045c5f0: its name field + 0x60, the stats + 0xC6 (STR AGI VIT INT DEX LUK, a byte each), the hair's style
// + 0x9A, its colour + 0xAA) in the new look (core/char_make): the face (win/head_picture) with the hair's ‹ ›, the
// name typed in the game's own field, the three pairs' points moved by the game's own buttons (0x8b-0x90), its hair
// buttons (0xa0 / 0xa1, 0xd5), «Створити» 0x76, «Скасувати» 0x77; the game's window off the screen. On the game's
// thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
void beforeGameDraw();
enum class PadCommand : uint8_t { Up, Down, Left, Right, Make, Cancel };
void padCommand(PadCommand c);
// 3.0.18-beta.228 (the keyboard concept, stage 5): ↑ ↓ the rows (the name among them: core charmake::keyRow), ← → the
// row's change (on the name: its caret, the game's); Enter and Esc stay the game's («Створити», «Скасувати»)
bool key(UINT m, WPARAM wp, LPARAM lp);
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::charmakewin
