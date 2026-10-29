#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/ui_icons.hpp"
#include "core/window_panel.hpp"

namespace uagfx::charblock {

// 3.0.11-beta.36 (stage 7 of the new interface, the HUD; the owner: «кольца или полоски на выбор. приступай»): the
// character's block at the screen's top left in place of the game's Basic Info window, style A (the concept's
// v17_char) - full (the portrait, the name and class, the levels, the weight, the zeny, the free points, HP and SP
// bars with their numbers, the experience with a tick each 10 %, the Basic Info window's buttons), compact (bars or
// rings, as chosen) and folded to a ring icon; a click on the portrait goes round them. Low HP: its bar (its ring)
// red, «!», a short warning under it. The parts that need no Windows; the words go over it by GDI (win/char_block).

enum class Look : uint8_t { Bars, Rings };       // the compact block's: [interface] character = bars | rings
enum class View : uint8_t { Full, Compact, Icon };
View nextView(View v);  // a click on the portrait: full, compact, the icon, full again

struct Data {
    std::string name, job;
    int baseLevel = 0, jobLevel = 0;
    int hp = 0, hpMost = 0, sp = 0, spMost = 0;
    float baseExp = -1, jobExp = -1;  // the level's part done (0..1); < 0 not known
    int weight = -1;                  // per cent of the most; < 0 not known
    long long zeny = -1;              // < 0 not known
    int statusPoints = 0;
    const panel::Picture* portrait = nullptr;  // the head, at the portrait's side; none: the name's first letter
};
bool lowHp(const Data& d);  // under a quarter of the most, as the game's own gauges turn red
const char* jobName(int job);  // the server's English (conf/msg_athena.conf 550-620); "" not known
std::string grouped(long long n);  // "1 482 300", the thousands apart by no-break spaces
std::string percent(float part, char decimal);  // "46,3 %"

// The Basic Info window's buttons (its own commands - FUN_0046c980 opens or closes each window): stats, equip,
// items, skills, friends, options
constexpr int kButtons = 6;
struct Button {
    int command;
    uiicons::Icon icon;
};
extern const Button kButton[kButtons];

struct Layout {
    float s = 1;
    View view = View::Full;
    Look look = Look::Bars;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box block;                // what takes the mouse: the glass, the rings' disc
    panel::Box portrait;             // its circle's box
    panel::Box name, levels, info;   // full: the name and class line, the levels, the weight / zeny / points line
    panel::Box hpLabel, spLabel, hpBar, spBar, hpText, spText;  // full and compact bars
    panel::Box expBar, baseText, jobText;                        // full
    panel::Box badge;                // the level on the rings (rings, the icon)
    float cx = 0, cy = 0, hpR = 0, hpW = 0, spR = 0, spW = 0;  // the rings: their middle, outer radii and widths
    panel::Box pointsDot, weightDot;  // the rings': free points (gold), heavy (orange)
    panel::Box buttons[kButtons];     // full
    panel::Box under;                 // under the block: the low HP warning, then a tip
};
// Its top left at (left, top) in the target, its sizes at `scale`
Layout layout(View view, Look look, int left, int top, float scale);

enum class Part : uint8_t { None, Portrait, Button, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;  // the button
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

struct Texts {
    std::string weight;            // «Вага»
    std::string points;            // «очко|очки|очок»: the free points' word by their count (one, a few, many)
    std::string lowHp;             // «Мало HP · {1} %»
    std::string buttons[kButtons];  // «Характеристики · Alt+A» ...
    char decimal = ',';
};
struct Show {
    int hoverButton = -1;
    bool hoverBlock = false;  // the rings and the icon: their numbers in a tip
    bool warning = false;     // the low HP warning (its first 2 seconds)
};
void paint(panel::Picture& out, const Layout& L, const Data& d, const Texts& t, const Show& show);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d, const Texts& t, const Show& show);

}  // namespace uagfx::charblock
