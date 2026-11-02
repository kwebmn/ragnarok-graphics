#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::homuncard {

// 3.0.11-beta.160 (task 9 of the remaining windows: the homunculus, Alt+R): the game's homunculus window (UI window
// 0x71, FUN_00467210 / FUN_00466be0) as a card in style A - its name and level, HP, SP and experience as bars, its
// fullness (0-100) and closeness (0-1000, the game's eight words), its eight stats (ATK MATK HIT CRIT DEF MDEF FLEE
// ASPD), the window's own buttons: feed (0xe0: the game asks first), its skills' window (0xdf), delete (0xe1: the
// game's red question first), ✕ (0x87). The parts that need no Windows; the words by GDI (win/homun_card).
struct Info {
    std::string name;  // UTF-8
    int level = 0, hp = 0, maxHp = 0, sp = 0, maxSp = 0;
    long long exp = 0, maxExp = 0;
    int hunger = 0, intimacy = 0;
    int stats[7] = {0, 0, 0, 0, 0, 0, 0};  // ATK MATK HIT CRIT DEF MDEF FLEE
    int aspd = 0;                          // the game's delay (shown as (2000 - it) / 10)
};
int intimacyState(int intimacy);  // 0 hostile ... 6 loyal, 7 unknown (the game's 3/10/100/250/750/910/1000)
int aspd(int delay);
enum class Button : uint8_t { Feed, Skills, Delete, Close };
int gameButton(Button b);

struct Texts {
    std::string title, level, hunger, intimacy;  // level: «Рівень {1}»
    std::string intimacyStates[8];
    std::string feed, skills, remove;
};
struct Show {
    int hover = -1;  // a Button
    int focus = -1;  // the controller's (Feed or Skills)
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, name, level, hpLabel, hp, spLabel, sp, expLabel, exp, hungerLabel, hunger,
        intimacyLabel, intimacy, remove;
    panel::Box stats[8];
    panel::Box buttons[2];  // feed, skills
};
Layout layout(int targetW, int targetH, float scale);
enum class Part : uint8_t { None, Button };
struct Hit {
    Part part = Part::None;
    Button button = Button::Close;
};
Hit hitTest(const Layout& L, float x, float y);

void paint(panel::Picture& out, const Layout& L, const Info& i, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Info& i, const Show& sh, const Texts& t);

}  // namespace uagfx::homuncard
