#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::tipcard {

// 3.0.11-beta.204 (the tester's third list: «Tip of the Day» left in the game's old look): the game's tip window (UI
// window 0x26 at g_windowMgr + 0x13C, made on the first map of a start when its «Display at startup» is set) as a
// card in style A at the screen's middle - the tip's words, its number of all, «Назад», «Далі», «Закрити» and the
// game's own start box, each pressing the game's (its SendMsg's message 6). The parts that need no Windows; the words
// by GDI (win/tip_window).

// tipOfTheday.txt's lines of one tip (between its '#' lines) as paragraphs: the first one's «*» off, each line's
// spaces at its ends, the empty ones out
std::vector<std::string> paragraphs(const std::vector<std::string>& lines);
std::string counter(int number, int count);  // «3 / 42»; "" with no tips

struct Card {
    std::vector<std::string> paragraphs;  // UTF-8
    int number = 0, count = 0;            // the tip's (from 1) and the tips'
    bool startup = true;                  // the game shows its tips at the start (its box)
    int rows = 3;                         // the words' rows at the card's width (measured by the window part)
};
struct Texts {
    std::string title, back, next, close, startup;  // «Порада дня», «Назад», «Далі», «Закрити», the box's
    std::string backKey, nextKey, closeKey;         // the keys or the controller's buttons beside them ("" none)
};
enum class Part : uint8_t { None, Back, Next, Close, Startup };
struct Show {
    Part hover = Part::None;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, counter, text, tick, tickLabel, back, next, close;
};
// At the screen's middle across, a third down
Layout layout(const Card& c, int targetW, int targetH, float scale, const Texts& t);
float textWidth(float scale);  // the words' room across (their rows measured against it)
// The words' rows at `width`, broken at spaces as GDI breaks them (a word wider than the width stays whole), their
// own line breaks kept; `measure` a piece's width
int rows(const std::string& text, float width, const std::function<float(const std::string&)>& measure);
Part partAt(const Layout& L, float x, float y);  // the picture's pixels
int gameButton(Part p);  // the game's window's own: 0xB4 back, 0x98 next, 0x87 close, 0x93 the box; 0 none

void paint(panel::Picture& out, const Layout& L, const Card& c, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
    bool wrap = false;  // broken into rows at its box's width (the tip)
};
std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t);

}  // namespace uagfx::tipcard
