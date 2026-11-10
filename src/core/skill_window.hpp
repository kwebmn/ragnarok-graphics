#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/skill_tree_view.hpp"
#include "core/window_drag.hpp"
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

// 3.0.11-beta.223 (the skills concept, stage 1: «Ще закриті»): why a skill of the tree is not the player's to raise -
// the server sends a skill only once its requirements are met (pc_calc_skilltree), never a quest, wedding or Soul Link
// one by them, and while the points of the job before are not spent (player_skillup_limit) not the next job's
// 3.0.11-beta.226: NextJob - a skill of the job a character may change to next («Наступна»), all its requirements met
enum class Lock : uint8_t { None, Needs, Quest, Wedding, SoulLink, Earlier, NextJob };

struct Skill {
    int id = 0;
    std::string name, aegis;
    std::string needs;  // «Bash 5, Provoke 5» - what an unlearned one needs; "" none known
    int type = 0;       // the game's: 0 passive, 1 an enemy, 2 the ground, 4 itself, 16 a friend, 32 a trap
    int level = 0, most = 0, use = 0;  // learned, its most (0 not known), the level it is used at
    int sp = 0, range = 0;
    bool up = false;  // the server lets a point raise it
    Lock lock = Lock::None;  // not sent by the server: shown after the sent ones, dimmed, without «+»
    std::string lockJob;     // Lock::Earlier: the job whose points come first
    std::vector<std::pair<int, int>> req;  // 3.0.11-beta.224: what it needs (a skill, its level) - the tree's links
    int target = 0;  // 3.0.11-beta.225: the level planned (core/skill_plan); 0 none
    // 3.0.11-beta.230 (the side panel in both views): SP by level (the server's table; empty none known) and the game's
    // description lines (UTF-8, no colour marks)
    std::vector<int> spLevels;
    std::vector<std::string> desc;
    const panel::Picture* icon = nullptr;  // at iconSide(), premultiplied
};
struct Tab {
    std::string name;  // the job's
    std::vector<Skill> skills;
    // 3.0.11-beta.226 (stage 4: «Наступна»): the next job's new skills - when the character may change to one of two,
    // their names as buttons over the list, the one shown
    bool next = false;
    std::vector<std::string> choices;
    int chosen = 0;
};
struct Data {
    std::vector<Tab> tabs;
    int points = 0;  // the free skill points
    std::string notice;  // 3.0.11-beta.223: a line over the list - why skills are closed; "" none
};

struct Texts {
    std::string title, hotkey;  // «Навички», «Alt+S»
    std::string points, drag;   // «Вільних очок: {1}», «Перетягніть навичку на панель клавіш»
    std::string passive, enemy, ground, self, ally, trap;  // what a skill is used on
    std::string sp, range, needs, unlearned;               // «SP {1}», «дальність {1}», «Потрібно: {1}», «Не вивчено»
    std::string use, raise, assign, close;                 // the controller's: «Застосувати», «+1 рівень» ...
    // 3.0.11-beta.223: a closed skill's line - «Квест — очками не вчиться», «Лише для подружжя», «Лише під Soul
    // Link», «Спершу витратьте очки {1}»
    std::string quest, wedding, soulLink, earlier;
    // 3.0.11-beta.224 (stage 2): the view's buttons «Список», «Дерево»; the path's line «До {1} — ще очок: {2}»
    std::string list, tree, path;
    // 3.0.11-beta.225 (stage 3): «План», «Застосувати», «Застосовується…», «Скинути», «У плані: {1}»
    std::string plan, apply, applying, reset, planned;
    std::string afterChange;  // 3.0.11-beta.226: «Після зміни професії»
    // 3.0.11-beta.229: the side panel - «Рівень {1} з {2}», «Потрібно», «Відкриває», «Наведіть на навичку»
    std::string levelOf, needsHead, unlocks, sideHint;
    std::string spByLevel;  // 3.0.11-beta.230: «SP за рівнями»
};

bool canRaise(const Skill& k, int points);  // a free point and the server's leave, below its most
// 3.0.11-beta.225: a «+» on it - in «План» one more level fits the plan, else canRaise
bool plusOn(const Data& d, const Skill& k, bool plan);
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
    // 3.0.11-beta.224 (stage 2): «Дерево» - the tab as a tree (core/skill_tree_view), scrolled both ways (its content's
    // pixels at scale 1); the player's choice, kept
    bool tree = false;
    bool hoverList = false, hoverTree = false;
    float treeX = 0, treeY = 0;
    // 3.0.11-beta.225 (stage 3): «План» - a «+» plans a level, a «−» takes one off; the plan's buttons in the footer
    bool plan = false, applying = false;
    bool hoverPlan = false, hoverApply = false, hoverReset = false;
    int hoverMinus = -1;
    int hoverChoice = -1;  // 3.0.11-beta.226
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    int tab = 0, first = 0;  // the tab shown, its first row's index
    panel::Box glass, title, close, tabsRow, list, footer, scroll;
    panel::Box notice;  // 3.0.11-beta.223: Data::notice's line (w 0: none)
    // 3.0.11-beta.224: the view's buttons; «Дерево»: the tree's place (rows, icons and pluses: a box per skill of the
    // tab by its index, the window's - first 0), its content, scrolled by (scrollX, scrollY) of at most (maxX, maxY)
    panel::Box viewList, viewTree;
    panel::Box planButton, apply, reset;  // 3.0.11-beta.225 (apply, reset: w 0 out of «План»)
    std::vector<panel::Box> minuses;      // a row's «−» (w 0: none)
    std::vector<panel::Box> choices;      // 3.0.11-beta.226: «Наступна»'s jobs
    bool tree = false;
    panel::Box view, scrollH;
    panel::Box side;  // 3.0.11-beta.229: the side panel
    skilltreeview::Plan plan;
    float scrollX = 0, scrollY = 0, maxX = 0, maxY = 0;
    std::vector<panel::Box> tabs;
    // a row each shown, its parts (w 0: none)
    std::vector<panel::Box> rows, icons, pluses, less, more, dots;
    std::vector<panel::Box> padGlyphs, padWords;  // the controller's buttons in the footer (A, Y, X, B) and words
};
Layout layout(int x, int y, float scale, const Data& d, const Show& sh, const Texts& t, const Measure& m);

// 3.0.11-beta.106 (R-WCXUQP: «не переносятся»): a press on a skill, a drag of the window's own past 10 pixels -
// 3.0.11-beta.110: the windows' common one (core/window_drag)
using windrag::Press;
using windrag::pressed;
using windrag::moved;
using windrag::released;

enum class Part : uint8_t {
    None, Close, Tab, Row, Plus, Less, More, Title, Body, ViewList, ViewTree, View, Plan, Minus, Apply, Reset, Choice,
    BarV, BarH  // 3.0.11-beta.245 (R-YCYYXU: «скрол не крутится внизу»): the scroll bars, taken by the mouse
};
struct Hit {
    Part part = Part::None;
    int index = -1;  // a tab's; for a row's parts the skill's in its tab
};
Hit hitTest(const Layout& L, float x, float y);

// 3.0.11-beta.224: «Дерево» scrolled by window pixels; scrolled to show a skill (the controller's)
void scrollTree(Show& sh, const Layout& L, float dx, float dy);
bool treeInSight(Show& sh, const Layout& L, int index);  // true: it moved
// 3.0.11-beta.245: a scroll bar pressed or dragged at (x, y) - the list (its rows) or the tree (either way) scrolled so
// that its thumb's middle stands there
void barTo(Show& sh, const Layout& L, const Data& d, bool horizontal, float x, float y);
// 3.0.11-beta.227 (stage 5: the controller in «Дерево»): the skill the D-pad goes to from `from` - the nearest one
// that way (dx, dy: -1, 0, 1), across counted twice; `from` when none
int treeStep(const Layout& L, int from, int dx, int dy);

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

// 3.0.18-beta.29 (uaUI's «Дерево»): the tree's parts as paintTree() draws them - in the window's pixels (the view's
// scroll counted; cut at L.view by the drawer)
struct TreeLink {
    panel::Box box;  // a straight piece of a link
    uint32_t rgb = 0;
    float alpha = 0;
};
struct TreeNode {
    panel::Box box, icon, plus, minus;  // plus / minus: w 0 none
    uint32_t fill = 0, edge = 0, plusRgb = 0;
    float fillA = 0, edgeA = 0, edgeW = 0, iconOpacity = 1, plusA = 0, minusA = 0;
    const panel::Picture* picture = nullptr;  // the skill's icon
};
struct TreeView {
    std::vector<TreeLink> links;
    std::vector<TreeNode> nodes;
};
TreeView treeView(const Layout& L, const Data& d, const Show& sh);

}  // namespace uagfx::skillwin
