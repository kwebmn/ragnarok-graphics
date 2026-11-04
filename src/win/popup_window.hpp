#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::popupwin {

// 3.0.11-beta.211 (the tester: «меню взаємодії з іншим гравцем - в старому стилі»): [interface] popups - the game's
// popup menu (UI window 0x12 at g_windowMgr + 0xC0, class 0x699C68: a player's in the world, a chat room member's, a
// guild member's) as a card (core/popup_card) at its place: its items (+ 0x70: each twice, the separator - the string
// at 0x6B99B0 - once) and, picked, what its own click does (FUN_00487ab0): its owner (the window by the id at + 0x60,
// FUN_004d1cb0) gets message 0x30 with the item's number - none and the id 0x89: the game mode's message 0x1A (the
// world's player menu) - and the menu closes (DeleteWindow 0x12). The game's menu hidden while the card shows. On the
// game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: closed with no pick
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::popupwin
