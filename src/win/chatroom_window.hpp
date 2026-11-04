#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::chatroomwin {

// 3.0.11-beta.207 (the tester's third list: «alt c - створити чат рум - в старому стилі»): [interface] chat_rooms -
// the game's chat room making window (Alt+C: UI window 0x1B at g_windowMgr + 0xFC, class 0x699968, SendMsg
// FUN_00482220) as a card (core/chatroom_card): its title and password edits (+ 0x70, + 0x74) shown as they are (the
// password by its length only), a click giving one the keys (the manager's SetFocus); the places the game's list
// (+ 0xC0, chosen + 0x94: its message 0x27 with the combo + 0x90 at 0), open or private its radios (+ 0x78, + 0x7C:
// message 6 / 0x95 from one, chosen + 0x8C), «Створити» its OK (6 / 0x76: the game checks and sends), «Скасувати» its
// 6 / 0x77; the game's window moved off the screen (its fields keep the keys). On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its cancel
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::chatroomwin
