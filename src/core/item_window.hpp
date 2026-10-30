#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::itemwin {

// 3.0.11-beta.42 (stage 8 of the new interface, its windows; the owner: «делай окна»): «Речі» (Alt+E) redrawn in
// style A (the concept's v17_windows): its tabs as the game's window has them (the items, the gear, the rest), a search
// by the name, a grid of eight cells a row (the game's icons, their counts on dark plates), the weight's bar and the
// zeny under it. The game's own window stays the game's (win/item_window): a double click uses or wears as its own,
// a press drags as its own, a right click its item's card. The parts that need no Windows; the words by GDI.

struct Item {
    int index = 0;     // the bag's place (the packets' item index)
    int id = 0;        // the item's number
    int type = 0;      // the game's kind of it (ITEM_INFO + 0)
    int count = 0;
    int location = 0;  // where it is worn (0 not gear)
    int refine = 0;
    bool identified = true;
    std::string name;  // UTF-8
    const panel::Picture* icon = nullptr;  // iconSide() square, premultiplied
};

enum class Tab : uint8_t { Items, Gear, Etc };
constexpr int kTabs = 3;
// As the game's window sorts them (FUN_004ba410): 0, 1, 2, 0x12 the items; 4, 5, 8, 9, 0xB-0xF the gear; the rest
Tab tabOf(int type);
enum class Use : uint8_t { None, Use, Confirm, Wear, Card, Ammo };  // Confirm: used after a second ask
// What a double click does with it, as the game's window (FUN_004b9a10): the items used, the gear worn (identified
// only), a card into a slot, the arrows worn
Use useOf(const Item& it);
// The tab's items whose names hold the search's words (any case), in the bag's order: their places in `items`
std::vector<int> shownOf(const std::vector<Item>& items, Tab tab, const std::string& search);
std::string lowered(const std::string& utf8);  // the Latin and Cyrillic letters small

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;
int iconSide(float scale);  // 32 at scale 1

struct Texts {
    std::string title, hotkey;  // «Речі», «Alt+E»
    std::string tabs[kTabs];    // «Предмети», «Спорядження», «Інше»
    std::string search;         // «Пошук за назвою»
    std::string weight;         // «Вага»
    std::string use, wear, card, info;  // the tip's: «двічі клік — використати», ... «ПКМ — опис»
};

struct Show {
    Tab tab = Tab::Items;
    int scroll = 0;     // rows down
    int hover = -1;     // the cell pointed at (of the shown)
    int hoverTab = -1;
    bool hoverClose = false;
    bool typing = false;  // the search has the keys
    std::string search;   // UTF-8
    bool caretOn = true;
    int weight = 0, weightMost = 0;
    long long zeny = 0;
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    int columns = 8, rows = 5;
    panel::Box glass, title, close, tabsRow, search, grid, footer, bar;
    std::vector<panel::Box> tabs, cells;  // the cells shown (columns x rows)
};
Layout layout(int x, int y, float scale, const Texts& t, const Measure& m);
int mostScroll(const Layout& L, size_t count);

enum class Part : uint8_t { None, Close, Tab, Search, Cell, Grid, Title, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a tab's; a cell's (of the shown, the scroll counted)
};
Hit hitTest(const Layout& L, size_t shownCount, int scroll, float x, float y);

void paint(panel::Picture& out, const Layout& L, const std::vector<Item>& items, const std::vector<int>& shown,
           const Show& sh, const Texts& t, const Measure& m);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Item>& items, const std::vector<int>& shown,
                         const Show& sh, const Texts& t, const Measure& m);
std::string nameOf(const Item& it);  // «+7 Sword», its count after a stack's: «Red Potion ×38»
std::string grouped(long long n);

}  // namespace uagfx::itemwin
