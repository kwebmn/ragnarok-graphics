#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/item_window.hpp"
#include "core/window_panel.hpp"

namespace uagfx::tradewin {

// 3.0.11-beta.116 (the windows plan's stage 7, «Обмін»; the concept «Концепция окон и инвентаря», stage 4): the trade
// redrawn in style A - two sides, «Ви» left and the partner right, ten cells each (the game's ten slots a side), the
// zeny offered under each (yours typed in its field), «Готово» marks, the buttons «Готово», «Обмін», «Скасувати»; the
// partner's things or zeny changed after your «Готово» in red with a warning line (the scam the game does not show).
// The parts that need no Windows; the words by GDI (win/trade_window).

constexpr int kSlots = 10, kColumns = 5;

struct Thing {
    int id = 0, count = 0, refine = 0;  // count 0: an empty slot
    bool identified = true;
    std::string name;  // UTF-8
    const panel::Picture* icon = nullptr;
};
struct Side {
    Thing slots[kSlots];
    long long zeny = 0;
    bool ready = false;  // «Готово» pressed (the game's lock)
};

// The partner's slots and zeny changed while you were ready - red from then on (the slots by their place: the game
// fills the first empty one and takes none back during a trade)
struct Late {
    bool slots[kSlots] = {};
    bool zeny = false;
    bool any() const;
};
Late watchLate(const Side& before, const Side& now, bool meReady, Late was);
std::string tipFirst(const Thing& x);  // 3.0.18-beta.95: a pointed thing's tip, its first line
bool canReady(const Side& me);                                  // not pressed yet
bool canTrade(const Side& me, const Side& them, bool traded);  // both ready, «Обмін» not pressed yet

struct Texts {
    std::string title, partner;  // «Обмін», the partner's name
    std::string you, zeny;       // «Ви», «Зені»
    std::string ready, trade, cancel;  // the buttons and the marks: «Готово», «Обмін», «Скасувати»
    std::string late;   // «Змінено після вашого «Готово»»
    std::string hint;   // «Перетягніть речі з «Речі» або двічі клацніть»
    std::string info;   // the tip's: «ПКМ — картка»
};

struct Show {
    int hover = -1;        // the cell pointed at: 0-9 yours, 10-19 the partner's
    int hoverButton = -1;  // 0 «Готово», 1 «Обмін», 2 «Скасувати»
    bool hoverClose = false;
    bool tip = true;
    bool typing = false;   // the zeny field has the keys
    std::string typed;     // its digits
    bool caretOn = true;
    bool traded = false;   // «Обмін» pressed
    bool dropping = false;  // a bag thing dragged: your side lit (a drop there offers it)
    Late late;
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass, title, close, mineHead, theirHead, mineZeny, theirZeny, zenyField, warn, hint;
    panel::Box cells[2 * kSlots];  // yours, then the partner's
    panel::Box buttons[3];
};
Layout layout(int x, int y, float scale);

enum class Part : uint8_t { None, Close, Title, Cell, Zeny, Button, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a cell's (0-19), a button's (0-2)
};
Hit hitTest(const Layout& L, float x, float y);

// 3.0.18-beta.28 (uaUI's trade): the parts paint() draws, placed - the window's pixels; w 0: not there
struct CellView {
    panel::Box box, count;  // the cell, the count's plate
    bool thing = false, hot = false, late = false;
    const panel::Picture* icon = nullptr;
    float iconX = 0, iconY = 0, iconOpacity = 1;
};
struct ButtonView {
    bool on = false, main = false, hot = false;  // pressable, the next step (gold), pointed at
};
struct View {
    panel::Box ready[2], dropping, divider, caret, tip;
    CellView cells[2 * kSlots];
    ButtonView buttons[3];
    bool late = false;  // the warning's plate
};
View view(const Layout& L, const Side& me, const Side& them, const Show& sh, const Texts& t,
          const itemwin::Measure& m);

}  // namespace uagfx::tradewin
