#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::hotbar {

// 3.0.11-beta.40 (stage 8 of the new interface - the hotbar first; the owner: «делай дальше»): the game's hotkeys bar
// redrawn at the bottom middle, style A (the concept's v17_hotbar) - the normal mode a row of nine cells F1-F9 with the
// row's number (F12 goes round them, as the game's), the battle mode the game's three rows Q, A, Z as the keyboard has
// them; each cell its icon, its key in the top left, an item's count in the bottom right (dimmed when none is in the
// bag), a skill's level in the bottom left; pointed at - its name in a tip. The parts that need no Windows; the words
// by GDI (win/hotbar). 3.0.11-beta.147 (the owner: «да, сделай так и предусмотри возможность чтобы можно было батлмод
// кнопкой на этой панели включать»): the pencil (the game's own bar shown to edit it) gone - a cell's icon dragged to
// another cell or off the bar, a right click its card; in the pencil's place the battle mode's button.

enum class Mode : uint8_t { Normal, Battle };

// 3.0.11-beta.45 (the concept's cell states; the owner: «занимаемся доработками интерфейса»): why the map forbids a
// slot - the server's own rules (uAthena pc_isUseitem, skill_not_ok and db/skill_nocast_db.txt) over the map's flags
// in the reference pack; a war of the guilds' castle (gvg_castle counts only while it goes) and the restricted zones
// (their numbers are not in the pack) are not judged
enum class NotHere : uint8_t {
    No, Teleport, Return, Warp, Branch, IceWall, Call, Skills, GuildWar, OnlyGuildWar,
    kCount
};

struct Cell {
    std::string key;    // «F1», «Q», «,»
    std::string name;   // its skill's or item's words; "" an empty cell
    int level = 0;      // a skill's
    int levelMost = 0;  // 3.0.11-beta.289: the level learned (the wheel's range 1 .. this; < 2 none)
    int count = -1;     // an item's in the bag (0: none there); -1 not an item
    bool low = false;   // 3.0.11-beta.281: a healing item 10 or fewer - its count red
    const panel::Picture* icon = nullptr;  // at iconSide(), premultiplied
    bool selected = false;       // 3.0.11-beta.45: the game waits for this skill's target - a gold outline 2 px
    int spNeed = 0, spHave = 0;  // a skill's SP price and the SP there: more needed than had - dimmed, a blue «SP»
    NotHere notHere = NotHere::No;  // greyed, a crossed circle, the reason in its tip
    bool unlearned = false;  // 3.0.11-beta.78: a skill the character has not learned - greyed so, its own words
    int pad = -1;  // 3.0.11-beta.86: the controller's button that uses it (padglyph::Button), -1 none
    // 3.0.11-beta.143 (the owner's concept «маг, прист и хант на геймпаде»): a ground skill's own count in the bottom
    // right - its walls standing («2/3»: Fire Wall, Quagmire, Fire Pillar) or a trap's Traps in the bag («×37»); ""
    // none; `standingOut` - none left (red); `flash` - a wall of it burnt out a moment ago: its edge amber (1 fading
    // to 0) - put it again
    std::string standing;
    bool standingOut = false;
    float flash = 0;
};
int iconSide(Mode mode, float scale);

// The battle mode's rows as shown, top down (Q, A, Z as on the keyboard), are the game's rows 1, 2, 0
// (FUN_004d8f10: Z..M,. the first row, Q..O the second, A..L the third)
int gameRow(int shownRow);
const char* battleKey(int shownRow, int cell);

struct Layout {
    float s = 1;
    Mode mode = Mode::Normal;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass;                 // the bar
    panel::Box row;                   // normal: the row's number and F12
    panel::Box header;                // battle: the mode's chip and its hint
    panel::Box toggle;                // 3.0.11-beta.147: the battle mode's button (the pencil's place)
    std::vector<panel::Box> cells;    // normal 9; battle 27, the shown rows' order
    std::vector<panel::Box> rowLabels;  // battle: each shown row's number
    panel::Box tipRoom;               // over the glass (under it when `below`)
    bool below = false;               // 3.0.12-beta.2: the tips and the ammo's pill under the glass
};
// Its bottom middle at (centreX, bottom) in the target
Layout layout(Mode mode, int centreX, int bottom, float scale);
// 3.0.12-beta.2 (a player's wish: «вікно запасу стріл перемістити вниз від скілів»; the owner: «если плашка навыков
// вверху то стрелы надо вниз, а если внизу то вверху стрелы»): the same picture, the glass at its top and the room for
// the tips and the ammo's pill under it - a bar in the screen's upper half goes up to the edge
void flip(Layout& L);

// 3.0.11-beta.117 (R-8YW96B: the bar jumped right for a second while walking): a window over the bar's place moves it
// aside only once it has stood there `afterMs` - a window there a moment no longer makes it jump; gone, it is back
struct Settle {
    uint64_t since = 0;  // the window there since (0 none)
    bool settled(bool over, uint64_t nowMs, uint64_t afterMs);
};

enum class Part : uint8_t { None, Cell, Toggle, Body };
struct Hit {
    Part part = Part::None;
    int cell = -1;
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

struct Texts {
    std::string battle;      // «бойовий режим»
    std::string battleHint;  // «літери — навички й предмети · Enter — чат»
    std::string battleOn, battleOff;  // 3.0.11-beta.147: the battle mode button's tips: «Увімкнути бойовий режим», ...
    std::string use;         // the cell's tip's second line: «клік — використати · ПКМ — опис · тягніть — перемістити»
    std::string noSp;        // 3.0.11-beta.45: «Немає SP · потрібно {1}, у вас {2}»
    std::string notHere;     // «Не тут · {1}»
    std::string unlearned;   // 3.0.11-beta.78: «Персонаж не має цієї навички»
    std::vector<std::string> reasons;  // by NotHere: «на цій карті телепорт заборонено», ...
    std::string wheel;  // 3.0.11-beta.289: a skill's third line - «колесо — рівень (1–{1})»
};
// 3.0.11-beta.125 (the owner: «когда играю арчером, есть стрелы, и не видно сколько осталось ... чтобы словить момент
// когда они заканчиваются»): the ammo worn on a pill over the bar's left end - its picture and how many, amber under 50,
// red at 10 or fewer; a fall under those (none left too) its words before the count a while (AmmoWarn)
struct Ammo {
    int count = -1;                        // how many (< 0 none worn: no pill)
    const panel::Picture* icon = nullptr;  // its picture at ammoIconSide(), premultiplied
    std::string warning;                   // «Стріли закінчуються» ("" none)
};
int ammoIconSide(float scale);
// 3.0.16-beta.8 (the owner: «для магов и пристов показывать количество гемов, а для монков ... сферы»): what the bar's
// skills spend (core/supplies) on pills after the ammo's - an item's picture and how many (red edge from 10 down, none
// red words), or the spirit spheres as dots (dots > 0: that many places, count of them lit)
struct Supply {
    int count = 0;
    const panel::Picture* icon = nullptr;  // at ammoIconSide()
    int dots = 0;                          // 0 an item; 5 or 10 the spheres' places
};
// How low: 0 plenty (50 or more; none worn), 1 under 50, 2 ten or fewer, 3 none left
int ammoLevel(int count);
// The level whose words to show with the count now (0 none): a fall to a lower level its words kShowMs, none left as
// long as it lasts; more again (a new stack) quiet at once. 3.0.11-beta.129: `fresh` - the level of a warning begun in
// this call (the sound's moment), 0 none; a count not known (< 0: a map loading) changes nothing
struct AmmoWarn {
    static constexpr uint64_t kShowMs = 5000;
    int level = 0, warned = 0, fresh = 0;
    uint64_t since = 0;
    int now(int count, uint64_t nowMs);
};

// What the mouse and the keys do to it now
struct Show {
    int hover = -1;          // the cell under the cursor: its backing lighter, its edge 20 %
    int pressed = -1;        // 3.0.11-beta.45: the cell pressed (a click, its key): 1 px down, its backing darker
    bool tip = true;         // the hovered cell's tip (the window part shows it after 0.4 s)
    bool hoverToggle = false;  // 3.0.11-beta.147: the battle mode's button under the cursor
    // 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO»): played with the controller - each cell its button's
    // badge (Cell::pad) over its top left corner instead of its key, a cell no button uses dimmed; PlayStation's look
    bool pad = false, sony = false;
    bool dropping = false;  // 3.0.11-beta.110: a thing or a skill dragged - the cells lit in gold (a drop takes it)
    Ammo ammo;              // 3.0.11-beta.125
    std::vector<Supply> supplies;  // 3.0.16-beta.8
    // 3.0.11-beta.147: a cell's icon lifted (dragged) - that cell's faded where it was, the cell it is over outlined
    int lifted = -1, over = -1;
    // 3.0.11-beta.276 (the owner's feature 7): the delay after a skill - its share left (0 none) darkening every
    // skill's cell clockwise from twelve, as the buffs' sweep
    float delay = 0;
    int profile = 1;  // 3.0.11-beta.288 (the owner's feature 20): the bar's profile - past the first its number by the
                      // toggle
};
// The ammo's pill over the glass's left end (w 0: none worn); a cell's tip over it hides it a while
panel::Box ammoBox(const Layout& L, const Show& show);
// 3.0.16-beta.8: the supplies' pills in a row after it (each w 0 when a tip is over it)
std::vector<panel::Box> supplyBoxes(const Layout& L, const Show& show);
void paint(panel::Picture& out, const Layout& L, const std::vector<Cell>& cells, const Show& show, const Texts& t);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
// `row`: the game's row shown in the normal mode (0-2)
std::vector<Words> words(const Layout& L, const std::vector<Cell>& cells, int row, const Show& show, const Texts& t);

// The game's tables of a key and its words a line (data\skillnametable.txt "AC_DOUBLE#Double_Strafe#",
// data\idnum2itemdisplaynametable.txt and idnum2itemresnametable.txt "501#Red_Potion#"); the "//" lines skipped
std::map<std::string, std::string> nameTable(const std::string& text);
std::string spaced(std::string words);  // a table's words as shown: "Double_Strafe" -> "Double Strafe"
std::string lowered(std::string text);  // a skill's icon is its name in small letters: "ac_double.bmp"
// A slot as the game's session keeps it (CSession + 0x314, 16 bytes a slot): "##AC_DOUBLE" a skill, "501" an item (its
// id, as the item's m_itemName - FUN_006344e0 reads it with atoi), "" or "nothing" none
struct Slot {
    enum class Kind : uint8_t { None, Skill, Item };
    Kind kind = Kind::None;
    std::string id;  // the skill's name, the item's number
};
Slot slotOf(const std::string& raw);
// 3.0.11-beta.45: whether this map (its flags: "noteleport", "gvg", ...) forbids the slot
NotHere notHere(const Slot& slot, const std::vector<std::string>& mapFlags);

// 3.0.11-beta.147: the game's slot (0-26) a shown cell keeps - the normal mode's of the row shown (`row` 0-2), the
// battle mode's shown rows Q, A, Z the game's 1, 2, 0; -1 no such cell
int gameSlot(Mode mode, int row, int cell);

// 3.0.11-beta.147 (the owner: «иногда панель сама двигается» - R-VYV3AG: a click whose pointer slipped dragged the
// whole bar): a press on a cell's icon, lifted (dragged) only once held kLiftMs and moved kLiftPx x the scale (the
// picture's pixels) - a quicker one stays the pressed cell's click wherever it is let go
struct Lift {
    static constexpr uint64_t kLiftMs = 250;
    static constexpr float kLiftPx = 10;
    int cell = -1;  // the cell pressed, -1 none
    float x = 0, y = 0;
    uint64_t at = 0;
    bool dragging = false;
};
void press(Lift& l, int cell, float x, float y, uint64_t nowMs);
bool moved(Lift& l, float x, float y, uint64_t nowMs, float scale);  // lifted now

// What a lifted icon let go at (x, y of the picture) does: over another cell - the two swapped (an empty one: moved
// there); well off the bar (kAwayPx x the scale past its glass) - its slot emptied; over its own cell, the bar's other
// parts or just past its edge - nothing
enum class Let : uint8_t { Nothing, Swap, Empty };
struct Release {
    Let what = Let::Nothing;
    int cell = -1;  // Swap: the other cell; Empty: the lifted one
};
constexpr float kAwayPx = 24;
bool away(const Layout& L, float x, float y);
Release letGo(const Layout& L, int lifted, float x, float y);
// The lifted icon's picture at the cursor, a cell's size: on a dark plate a little faded; away - a red badge with a
// minus at its top right
int liftedSide(Mode mode, float scale);
void paintLifted(panel::Picture& out, const panel::Picture* icon, bool away, float scale);

}  // namespace uagfx::hotbar
