#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::loginshelfwin {

// 3.0.11-beta.176 (the login concept's «Полиця»): the login screen's shelf of accounts (core/login_shelf) - drawn by
// win/login_card in place of its card while this Windows user's shelf (win/accounts_store) has accounts and the
// classic card was not asked for («Інший акаунт», «Додати акаунт», a refused password: the card for one login, back
// with its «‹ Полиця»). A card's click enters; its menu (its toggles, its label typed in, its place, off the shelf),
// the arrows, the keys (← → Enter; Esc closes the menu or the label). On the game's thread.
enum class Act : uint8_t { None, Login, Add, Other, Edit, Create, Exit };
struct Asked {
    Act act = Act::None;
    int index = -1;  // Login, Edit: the account's place on the shelf
};
void classic(bool on);  // the card for one login (on), the shelf again (off)
bool classicChosen();
void frame(double k, int targetWidth, int targetHeight, unsigned codePage);
void draw(FrameRecord& record, gpu::Renderer& renderer);
Asked mouse(UINT m, float tx, float ty, bool& took);  // a press at the target's (tx, ty); took: it was over the shelf
Asked key(UINT m, WPARAM wp, bool& took);
bool box(int& x, int& y, int& w, int& h);  // the shelf's picture in the target's pixels

}  // namespace uagfx::win::loginshelfwin
