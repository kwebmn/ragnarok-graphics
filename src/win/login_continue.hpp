#pragma once
#include <windows.h>

#include <cstdint>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::logincontinuewin {

// 3.0.11-beta.177 (the login concept's «Продовжити»): the account used last (or the one picked in «Змінити акаунт»)
// with its character big (core/login_continue) - drawn by win/login_card in place of its card with the view
// «Продовжити» and accounts on the shelf. «Грати» (Enter) enters with that character, «Інший персонаж» stops at the
// character select, «Змінити акаунт» lists the shelf (and «+ Інший логін»: the card), «Створити акаунт» the site's
// page. On the game's thread.
enum class Act : uint8_t { None, Play, OtherChar, Other, Create };
struct Asked {
    Act act = Act::None;
    int index = -1;  // Play, OtherChar: the account's place on the shelf
};
void frame(double k, int targetWidth, int targetHeight, unsigned codePage);
void draw(FrameRecord& record, gpu::Renderer& renderer);
Asked mouse(UINT m, float tx, float ty, bool& took);  // a press at the target's (tx, ty); took: it was the panel's
Asked key(UINT m, WPARAM wp, bool& took);
bool box(int& x, int& y, int& w, int& h);  // the panel in the target's pixels
// 3.0.11-beta.179: the pad's Y - the shelf's next account shown (round)
void nextAccount();
int shown();  // the account shown: its place on the shelf, -1 none

}  // namespace uagfx::win::logincontinuewin
