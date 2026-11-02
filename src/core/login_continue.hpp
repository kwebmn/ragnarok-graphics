#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::logincontinue {

// 3.0.11-beta.177 (the login concept's «Продовжити»): the last account's character big at the left, its name, its
// job, level, map and login, «Грати» (Enter), «Інший персонаж» (the character select shown), «Змінити акаунт» (the
// shelf's other logins, ▾), «Створити акаунт». The parts that need no Windows; the words by GDI (win/login_continue).
struct Account {
    std::string name, job, map, login;  // UTF-8; name "" - no character yet (the login big)
    int level = 0;
    bool refused = false;
    const panel::Picture* art = nullptr;  // the class's picture; null - a plain ground
};
struct Texts {
    std::string heading, play, enterKey, loginWord;  // «Продовжити гру», «Грати», «Enter», «логін {1}»
    std::string otherChar, change, create;           // the links
    std::string noChar, refused;                     // «Ще не грали», «Пароль не підходить»
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the panel in the target's pixels
    panel::Box art, heading, name, line, play, otherChar, change, create;
};
Layout layout(int targetW, int targetH, float scale, const Texts& t = Texts{});
enum class Part : uint8_t { None, Play, OtherChar, Change, Create };
Part hitTest(const Layout& L, float x, float y);
struct Show {
    Part hover = Part::None;
    bool changeOpen = false;  // the other logins' list shown
};
void paint(panel::Picture& out, const Layout& L, const Account& a, const Show& sh);
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 14;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Account& a, const Show& sh, const Texts& t);

}  // namespace uagfx::logincontinue
