#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::charselect {

// 3.0.11-beta.152 (task 3 of the remaining windows, the login's part two): the game's character select (UI window 5)
// in style A at the screen's middle - the page's three places as cards (the face, the name, the class and levels, HP
// and SP, the six stats; an empty place to make one), the chosen one outlined in gold; «Увійти», «Створити»,
// «Видалити», «Назад» press its own buttons; ‹ › its pages. The parts that need no Windows; the words by GDI
// (win/char_select).
struct Slot {
    bool used = false;
    std::string name, job;  // UTF-8
    int level = 0, jobLevel = 0, hp = 0, maxHp = 0, sp = 0, maxSp = 0;
    int stats[6] = {0, 0, 0, 0, 0, 0};          // STR AGI VIT INT DEX LUK
    const panel::Picture* face = nullptr;  // its head at faceSide(scale), premultiplied
    std::string ago, map;  // 3.0.11-beta.273: when last played here («2 год тому»), its map then ("" not known)
};
int faceSide(float scale);

struct Texts {
    std::string title, enter, make, remove, back;  // «Вибір персонажа», «Увійти», «Створити», «Видалити», «Назад»
    std::string empty, makeHint;                   // «Вільне місце», «Створіть тут нового персонажа»
    std::string stats[6];
};
struct Show {
    int selected = 0;  // the place chosen on the page (0-2)
    int hover = -1;    // the place under the mouse
    int hoverButton = -1;
    int page = 0, pages = 1;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, page, pageLeft, pageRight;
    std::vector<panel::Box> slots;    // three
    std::vector<panel::Box> buttons;  // Button's order
};
Layout layout(int targetW, int targetH, float scale);
enum class Part : uint8_t { None, Slot, Button, PageLeft, PageRight };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);

enum class Button : uint8_t { Enter, Make, Remove, Back };
int gameButton(Button b);    // the game's window's own: 0x76 enter, 0x89 make, 0x91 delete, 0x77 back
int pageButton(bool right);  // 0x96 the page after (the game: its page + 1), 0x97 the one before
bool allowed(const Slot& s, Button b);

void paint(panel::Picture& out, const Layout& L, const std::vector<Slot>& slots, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Slot>& slots, const Show& sh, const Texts& t);

}  // namespace uagfx::charselect
