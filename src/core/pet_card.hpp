#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::petcard {

// 3.0.11-beta.159 (task 9 of the remaining windows: the pet, Alt+J): the game's pet window (UI window 0x58,
// FUN_00464c50) as a card in style A - the pet's name and level, its fullness (0-100) and closeness (0-1000) as bars
// with the game's five words each, its four commands the window's own menu choices (message 0x27: 0 feed - the game
// asks first, 1 perform, 2 the accessory off, 3 back into the egg), ✕ its close (6 / 0x87). The parts that need no
// Windows; the words by GDI (win/pet_card).
constexpr int kMenuMsg = 0x27, kCloseButton = 0x87;
struct Info {
    std::string name;  // UTF-8
    int level = 0, hunger = 0, intimacy = 0;
    bool accessory = false;
};
int hungerState(int hunger);      // 0 very hungry ... 4 stuffed (the game's 10 / 25 / 75 / 90)
int intimacyState(int intimacy);  // 0 awkward ... 4 loyal (100 / 250 / 750 / 900)
enum class Command : uint8_t { Feed, Perform, Unequip, Egg };
int gameChoice(Command c);
bool allowed(const Info& i, Command c);

struct Texts {
    std::string title, level, hunger, intimacy;  // level: «Рівень {1}»
    std::string hungerStates[5], intimacyStates[5];
    std::string commands[4];
};
struct Show {
    int hover = -1;  // a button
    bool hoverClose = false;
    int focus = -1;  // the controller's button
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, name, level, hungerLabel, hungerBar, hungerState, intimacyLabel, intimacyBar,
        intimacyState;
    panel::Box buttons[4];
};
Layout layout(int targetW, int targetH, float scale);
enum class Part : uint8_t { None, Close, Button };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);


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

}  // namespace uagfx::petcard
