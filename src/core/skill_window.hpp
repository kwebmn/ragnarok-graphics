#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::skillwin {

// 3.0.11-beta.96 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's «Навыки»):
// «Навички» (Alt+S) redrawn in style A - tabs by the jobs the character passed through (the server's skill tree), a
// row a skill: its icon, its name, what it is (on an enemy, SP, range; passive; what it needs), its level's dots out
// of its most and «learned/most», a «+» where a free point raises it (the server says it may), the arrows of the level
// it is used at; the free points under them and what to do - the mouse's drag to the skill bar, the controller's
// buttons. The skills, their levels and the raise are the game's session's and its own messages' (win/skill_window).
// The parts that need no Windows; the words by GDI.
constexpr int kRows = 8;  // rows shown at once; more scroll

struct Skill {
    int id = 0;
    std::string name, aegis;
    std::string needs;  // «Bash 5, Provoke 5» - what an unlearned one needs; "" none known
    int type = 0;       // the game's: 0 passive, 1 an enemy, 2 the ground, 4 itself, 16 a friend, 32 a trap
    int level = 0, most = 0, use = 0;  // learned, its most (0 not known), the level it is used at
    int sp = 0, range = 0;
    bool up = false;  // the server lets a point raise it
    const panel::Picture* icon = nullptr;  // at iconSide(), premultiplied
};
struct Tab {
    std::string name;  // the job's
    std::vector<Skill> skills;
};
struct Data {
    std::vector<Tab> tabs;
    int points = 0;  // the free skill points
};

struct Texts {
    std::string title, hotkey;  // «Навички», «Alt+S»
    std::string points, drag;   // «Вільних очок: {1}», «Перетягніть навичку на панель клавіш»
    std::string passive, enemy, ground, self, ally, trap;  // what a skill is used on
    std::string sp, range, needs, unlearned;               // «SP {1}», «дальність {1}», «Потрібно: {1}», «Не вивчено»
    std::string use, raise, assign, close;                 // the controller's: «Застосувати», «+1 рівень» ...
};

bool canRaise(const Skill& k, int points);  // a free point and the server's leave, below its most
bool usable(const Skill& k);                // learned and not passive
std::string info(const Skill& k, const Texts& t);
int keepInSight(int focus, int scroll, int count);  // the scroll that shows the focused row
int iconSide(float scale);

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;

struct Show {
    int tab = 0, scroll = 0;
    int hoverTab = -1, hoverRow = -1, hoverPlus = -1, hoverLess = -1, hoverMore = -1;
    int focus = -1;  // the controller's row (its index in the tab)
    bool hoverClose = false;
    bool pad = false, sony = false;  // played with the controller: its buttons under the rows
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    int tab = 0, first = 0;  // the tab shown, its first row's index
    panel::Box glass, title, close, tabsRow, list, footer, scroll;
    std::vector<panel::Box> tabs;
    // a row each shown, its parts (w 0: none)
    std::vector<panel::Box> rows, icons, pluses, less, more, dots;
    std::vector<panel::Box> padGlyphs, padWords;  // the controller's buttons in the footer (A, Y, X, B) and words
};
Layout layout(int x, int y, float scale, const Data& d, const Show& sh, const Texts& t, const Measure& m);

// 3.0.11-beta.106 (R-WCXUQP: «не переносятся» - the game's drag begun at the press never saw its button come up, the
// press and the release being the driver's): a press on a skill, a drag of the window's own once the cursor moved past
// 10 pixels times the scale (till let go); released() - the skill a drag let go (the game's drag of it begun and let
// go there), -1 after a click or nothing pressed
struct Press {
    int index = -1;  // the skill pressed (in its tab), -1 none
    float x = 0, y = 0;
    bool dragging = false;
};
void pressed(Press& p, int index, float x, float y);
bool moved(Press& p, float x, float y, float scale);  // dragging now
int released(Press& p);

enum class Part : uint8_t { None, Close, Tab, Row, Plus, Less, More, Title, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a tab's; for a row's parts the skill's in its tab
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

}  // namespace uagfx::skillwin
