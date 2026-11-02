#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/shelf_merge.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::loginshelfwin {

// 3.0.11-beta.176 (the login concept's «Полиця»): the login screen's shelf of accounts (core/login_shelf) - drawn by
// win/login_card in place of its card while this Windows user's shelf (win/accounts_store) has accounts and the
// classic card was not asked for («Інший акаунт», «Додати акаунт», a refused password: the card for one login, back
// with its «‹ Полиця»). A card's click enters; its menu (its toggles, its label typed in, its place, off the shelf),
// the arrows, the keys (← → Enter; Esc closes the menu or the label). 3.0.11-beta.181: signed in to the site - its
// other logins on the shelf too (their passwords asked once), «Персонажі» (a row a login, a click enters with that
// character), a new game login, an existing one linked, a game password changed. On the game's thread.
// 3.0.11-beta.189 (the owner: «стрелочки, скрол чтобы мотать было интереснее ... поиск», «чтобы мышкой скролить»):
// the row glides; the wheel, the arrows and the bar move it (the focus stays in sight), the keys and the pad move the
// focus (the row follows; Home, End, Page Up and Down too); with more accounts than in sight the keys typed go into
// the search on the title row (the login, the label, the characters' names; Esc empties it).
enum class Act : uint8_t {
    None, Login, Add, Other, Edit, Create, Exit,
    NewLogin, LinkLogin, GamePassword, CharLogin, CharNew  // 3.0.11-beta.181: the site account's
};
struct Asked {
    Act act = Act::None;
    int index = -1;    // Login, Edit, GamePassword, CharLogin, CharNew: the row's place (row())
    std::string name;  // CharLogin: the character's (UTF-8)
};
// 3.0.11-beta.181: the shelf's rows now - this computer's accounts and the site account's other logins
// (core/shelf_merge); null: no such row
const shelfmerge::Row* row(int index);
void classic(bool on);  // the card for one login (on), the shelf again (off)
bool classicChosen();
void frame(double k, int targetWidth, int targetHeight, unsigned codePage);
void draw(FrameRecord& record, gpu::Renderer& renderer);
// a press or the wheel (its wp) at the target's (tx, ty); took: it was over the shelf
Asked mouse(UINT m, WPARAM wp, float tx, float ty, bool& took);
Asked key(UINT m, WPARAM wp, bool& took);
bool box(int& x, int& y, int& w, int& h);  // the shelf's picture in the target's pixels
// 3.0.11-beta.179: the pad - ◀ ▶ the card, A plays it (a menu open: its row), X another account, Y the card's menu
// (↑ ↓ its rows), B closes the menu (false: no menu - the caller's B)
enum class Pad : uint8_t { Left, Right, Up, Down, Select, Other, Menu, Back };
Asked pad(Pad p, bool& took);
bool menuOpen();

}  // namespace uagfx::win::loginshelfwin
