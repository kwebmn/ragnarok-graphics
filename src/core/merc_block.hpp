#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/merc_card.hpp"
#include "core/window_panel.hpp"

namespace uagfx::mercblock {

// 3.0.11-beta.216 (the owner: «в интерфейсе нету управления наемниками»): the hired mercenary's block on the screen -
// under the party's panel, or the character block - its kind and level, its name, HP and SP bars, the contract's time
// left (warm in its last five minutes) and its loyalty; a click opens or closes its card (the game's Ctrl+R window, the
// new «Найманець» card: its skills, dismiss). The server leads it alone (uAthena takes no orders for it). The parts
// that need no Windows; the words by GDI (win/merc_card).
struct Info {
    merccard::Kind kind = merccard::Kind::Swordsman;
    std::string name;  // UTF-8
    int level = 0, hp = 0, maxHp = 0, sp = 0, maxSp = 0, loyalty = 0;
    int secondsLeft = 0;  // the contract's
};
// «ще 24 хв» / «ще 1 год 05 хв» (a started minute counts); empty once ended
std::string leftText(int seconds, const std::string& minutes, const std::string& hours);
bool ending(int seconds);  // its last five minutes

struct Texts {
    std::string kinds[3], level;  // level: «Рв. {1}»
    std::string left;             // the time left, as leftText made it
    std::string loyalty;          // «Вірність»
};
struct Show {
    bool hover = false;
    bool open = false;  // its card shows: edged gold
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, icon, kind, level, name, hpText, hp, sp, left, loyalty;
};
// `width` the character block's (0: its own); its top left at (left, top)
Layout layout(int left, int top, int width, float scale);
bool hit(const Layout& L, float x, float y);

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
std::vector<Words> words(const Layout& L, const Info& i, const Texts& t);

}  // namespace uagfx::mercblock
