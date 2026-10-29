#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::hotbar {

// 3.0.11-beta.40 (stage 8 of the new interface - the hotbar first; the owner: «делай дальше»): the game's hotkeys bar
// redrawn at the bottom middle, style A (the concept's v17_hotbar) - the normal mode a row of nine cells F1-F9 with the
// row's number (F12 goes round them, as the game's), the battle mode the game's three rows Q, A, Z as the keyboard
// has them; each cell its icon, its key in the top left, an item's count in the bottom right (dimmed when none is in
// the bag), a skill's level in the bottom left; pointed at - its name in a tip. An edit button brings the game's own
// bar back to drag skills and items into it. The parts that need no Windows; the words by GDI (win/hotbar).

enum class Mode : uint8_t { Normal, Battle, Editing };

struct Cell {
    std::string key;    // «F1», «Q», «,»
    std::string name;   // its skill's or item's words; "" an empty cell
    int level = 0;      // a skill's
    int count = -1;     // an item's in the bag (0: none there); -1 not an item
    const panel::Picture* icon = nullptr;  // at iconSide(), premultiplied
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
    panel::Box glass;                 // the bar (editing: the done button alone)
    panel::Box row;                   // normal: the row's number and F12
    panel::Box header;                // battle: the mode's chip and its hint
    panel::Box edit;                  // the edit button (editing: done)
    std::vector<panel::Box> cells;    // normal 9; battle 27, the shown rows' order
    std::vector<panel::Box> rowLabels;  // battle: each shown row's number
    panel::Box tipRoom;               // over the glass
};
// Its bottom middle at (centreX, bottom) in the target
Layout layout(Mode mode, int centreX, int bottom, float scale);

enum class Part : uint8_t { None, Cell, Edit, Body };
struct Hit {
    Part part = Part::None;
    int cell = -1;
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

struct Texts {
    std::string battle;      // «бойовий режим»
    std::string battleHint;  // «літери — навички й предмети · Enter — чат»
    std::string edit, done;  // the button's tips: «редагувати панель гри», «готово»
    std::string use;         // the cell's tip's second line: «клік — використати»
};
void paint(panel::Picture& out, const Layout& L, const std::vector<Cell>& cells, int hover, bool hoverEdit,
           const Texts& t);

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
std::vector<Words> words(const Layout& L, const std::vector<Cell>& cells, int row, int hover, bool hoverEdit,
                         const Texts& t);

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

}  // namespace uagfx::hotbar
