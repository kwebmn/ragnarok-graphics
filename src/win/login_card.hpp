#pragma once
#include <windows.h>

#include <string>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::loginwin {

// 3.0.11-beta.151 (task 3 of the remaining windows, the login's part): [interface] login - the game's login window (UI
// window 3 at g_windowMgr + 0x88, its class 0x697A70, SendMsg FUN_00457280: its account's field + 0x60, its
// password's + 0x64 - the one drawn as '*', told by that mask since 3.0.11-beta.170 (R-4TNJP8: .151 had them the wrong
// way round), «Запам'ятати» + 0x68) in the new look (core/login_card) at the screen's middle: the game's window
// moved off the screen (its fields keep the keys - typing, Tab, Enter as before), their words shown as they have them
// (the password as dots, its length only), a click on a field gives it the keys (the manager's SetFocus 0x4CE510),
// «Увійти», «Вийти» and the box press its own buttons, the registration opens uaro.kiev.ua's page. On the game's
// thread.
// 3.0.11-beta.173: `status` - [interface] login_status, the server's status line at the screen's foot
void frame(HWND game, bool on, bool status, unsigned codePage, double k, int targetWidth, int targetHeight);
// 3.0.11-beta.172: the card holds the game's login now (on, the window there, its fields known by their mask)
bool active();
// 3.0.11-beta.172: the login typed into the game's field the last time the card saw it (UTF-8)
const std::string& account();
// 3.0.11-beta.173: the game's «Failed to Connect to Server.» for the login (win/msg_box answered it unseen)
void connectFailed();
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);

}  // namespace uagfx::win::loginwin
