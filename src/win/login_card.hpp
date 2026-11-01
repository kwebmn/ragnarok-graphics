#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::loginwin {

// 3.0.11-beta.151 (task 3 of the remaining windows, the login's part): [interface] login - the game's login window (UI
// window 3 at g_windowMgr + 0x88, its class 0x697A70, SendMsg FUN_00457280: its account's field + 0x64, its
// password's + 0x60, «Запам'ятати» + 0x68) in the new look (core/login_card) at the screen's middle: the game's window
// moved off the screen (its fields keep the keys - typing, Tab, Enter as before), their words shown as they have them
// (the password as dots, its length only), a click on a field gives it the keys (the manager's SetFocus 0x4CE510),
// «Увійти», «Вийти» and the box press its own buttons, the registration opens uaro.kiev.ua's page. On the game's
// thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::loginwin
