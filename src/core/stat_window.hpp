#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::statwin {

// 3.0.11-beta.42 (stage 8 of the new interface, its windows; the owner: «делай окна»): «Характеристики» (Alt+A)
// redrawn in style A (the concept's v17_windows): the six stats' table - base, bonus, their sum and the next point's
// price, a «+» where the free points pay for it - beside the battle's numbers, and a line under them telling what the
// stat pointed at gives by the game's (pre-renewal) formulas. The numbers are the game's session's, as its own status
// window shows them (FUN_00471d80); a «+» asks as its own does (the game mode's message 0x20). The parts that need no
// Windows; the words by GDI (win/stat_window).

constexpr int kStats = 6;
extern const char* const kNames[kStats];  // STR AGI VIT INT DEX LUK

struct Data {
    int base[kStats] = {}, bonus[kStats] = {}, need[kStats] = {};
    int free = 0;  // the status points
    int atk = 0, atk2 = 0, matkMin = 0, matkMax = 0, hit = 0, crit = 0;
    int def = 0, def2 = 0, mdef = 0, mdef2 = 0, flee = 0, flee2 = 0, aspd = 0;
};
// As the game's window shows its «+» (FUN_004722a0, message 0x23): a price, the points for it, under 99
bool canRaise(const Data& d, int stat);

struct Texts {
    std::string title, hotkey;    // «Характеристики», «Alt+A»
    std::string free;             // «Вільні очки: {1}»
    std::string columns[5];       // «Стат», «База», «Бонус», «Разом», «Ціна»
    std::string battle, cast;     // «Бій», «Час касту»
    std::string maxed;            // «макс.»
    std::string cost, shortBy, top;  // «{1} {2} → {3} коштує {4}, у вас {5}», «бракує {1}», «{1} {2} — максимум»
    std::string effects[kStats];  // what a point gives: «ATK +{1}, вантаж +30» ...
    std::string hint;             // nothing pointed at
    char decimal = ',';
};

struct Row {
    std::string name, value;
};
// The battle's numbers: ATK «28 + 17», MATK «334 ~ 550», HIT, FLEE «132 + 1», CRIT, DEF «6 + 32», MDEF, ASPD and the
// cast's time «−56 %» (by DEX: a 150th a point)
std::vector<Row> battleRows(const Data& d, const Texts& t);
// The line about a stat: its next point's price against the free points and what it gives; the hint for -1
std::string explain(const Data& d, int stat, const Texts& t);

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;

struct Show {
    int hoverRow = -1, hoverPlus = -1, selected = -1;
    bool hoverClose = false;
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, freeChip, table, header, battle, line;
    std::vector<panel::Box> rows, pluses;  // the six stats' rows and «+»
};
Layout layout(int x, int y, float scale, const Data& d, const Texts& t, const Measure& m);

enum class Part : uint8_t { None, Close, Plus, Row, Title, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);

void paint(panel::Picture& out, const Layout& L, const Data& d, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d, const Show& sh, const Texts& t, const Measure& m);

}  // namespace uagfx::statwin
