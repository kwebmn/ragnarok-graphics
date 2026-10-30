#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/image.hpp"
#include "core/reference_pages.hpp"
#include "core/window_panel.hpp"
#include "core/world_map.hpp"

namespace uagfx::refwin {

// 3.0.11-beta.7: the «Довідник» window's shapes (win/reference_window draws it): 1270 x 850 at 1080p (the concept's),
// centred, smaller on a small screen; the page's rows scrolled. The target's pixels throughout.
// 3.0.11-beta.26 (the owner:
// «у нас оно отличается от того как это выглядит в концепте»):
// the concept 2.3's style A, its mock-ups v23_ref_home and v23_ref_monster - a dark glass window (90 %), its title
// at the left of the header with «‹ Назад до …» and the way here, the search and the star at the right on
// the cards (the first screen and the results have the search's own big field), the data's chips and the keys at
// the bottom; the first screen's tiles five to a line with their icons, framed blocks of chips.

// The words' width at a size (the target's pixels): GDI's in the driver (win/picture_draw), guessed by the letters
// without a font (tests, previews)
using Measure = float (*)(const std::string& utf8, float px, bool bold);
float guessWidth(const std::string& utf8, float px, bool bold);

struct Layout {
    int x = 0, y = 0, w = 0, h = 0;  // the window
    int header = 0, footer = 0, pad = 0;
    int bodyY = 0, bodyH = 0;  // the scrolled part, from the window's top
    float s = 1;
    Measure measure = guessWidth;  // 3.0.11-beta.26: the chips' and the way's words
};
Layout layout(int targetW, int targetH, float scale, Measure measure = guessWidth);

struct Placed {  // a row's place in the body: from the body's top before the scroll
    int x = 0, y = 0, w = 0, h = 0;
    // 3.0.11-beta.26: a block's or a section's whole frame (its heading's own line is h: what a click finds), 0 none;
    // a row inside one
    int boxH = 0;
    bool in = false;
};
std::vector<Placed> place(const Layout& L, const refpages::Page& p, int& total);
int clampScroll(int scroll, int total, int bodyH);
bool shown(const Layout& L, const Placed& r, int scroll);  // the whole row in the body

// 3.0.11-beta.26: the header's star; the rail's sections (row: its index); 3.0.11-beta.30 «Головна» (the header's
// button and its title)
// 3.0.11-beta.64: Chain - a map of the world map's dungeon box (WorldLook::chain)
enum class Part : uint8_t { None, Close, Back, Search, Row, Body, Tile, Star, Rail, Home, Chain };
struct Under {
    Part part = Part::None;
    int row = -1;   // Part::Row: a row that leads somewhere; Part::Tile: the world map's row
    int tile = -1;  // 3.0.11-beta.11: Part::Tile - the world map's tile (worldmap::Sheet::world); Part::Chain its map
};
// 3.0.11-beta.64: a map of the dungeon box - its code, its name, its picture (the client's minimap at chainSide()
// square, premultiplied; null: the sea's colour)
struct ChainMap {
    std::string map, name;
    const panel::Picture* picture = nullptr;
};
// 3.0.11-beta.11: the world map's row - its picture and what goes over it; 3.0.11-beta.29 the concept's v19_route: the
// view zoomed and moved, the players' green pills, the towns' names, «Ви тут», the way in gold with its teleports'
// dashed arcs, its end's pin, the card of the map under the pointer
struct WorldLook {
    const panel::Picture* picture = nullptr;  // the sheet at the view in the row (its size; win/reference_window)
    const worldmap::Sheet* sheet = nullptr;
    std::string here;
    float hereX = -1, hereY = -1;              // the player on the sheet (-1 not known)
    std::vector<refpages::Hop> hops;          // 3.0.11-beta.29: the way shown, its moves from here on
    std::string target;                       // its end (off the sheet - a dungeon: the pin on its last map there)
    const std::map<std::string, int>* players = nullptr;
    std::string hover;                        // the tile under the pointer
    std::map<std::string, int> marks;         // 3.0.11-beta.19: maps marked (a monster's) with their numbers
    float zoom = 1, cx = -1, cy = -1;          // 3.0.11-beta.29: the view (worldmap::view)
    std::vector<std::pair<std::string, std::string>> labels;  // the towns' names on their tiles (refpages)
    std::vector<std::string> tip;             // the card of the map under the pointer (refpages::worldTip)
    std::string hereLabel, targetLabel;       // «Ви тут · …»; the pin's («Glast Heim → підземелля»), "" none
    // 3.0.11-beta.64 (the concept's v19_route): a dungeon's way - its maps from its way in to the target in a box at
    // the map's bottom left (refpages::WorldWay::chain), its title; fewer than two: none
    std::vector<ChainMap> chain;
    std::string chainTitle;
};
int chainSide(const Layout& L);  // 3.0.11-beta.64: the dungeon box's pictures' side (64 at scale 1)
// 3.0.11-beta.26: the words around the page - the header's and the bottom's
enum class Live : uint8_t { None, Fresh, Off };  // the site's live data: not asked for, fresh, no link
struct Texts {
    std::string title;                // «Довідник»
    std::string back;                 // «Назад до …» ("" on the first screen)
    std::vector<std::string> crumbs;  // the way here, the page's own the last
    std::string query, hint;          // typed; the big search's words while nothing is
    std::string find;                 // the header's search's words (the cards and the lists)
    std::string pack, live;           // the data's chips at the bottom ("" none)
    Live liveState = Live::None;
    std::string keys;                 // at the bottom's right
    bool home = false;                // 3.0.11-beta.30: «Головна» in the header (every page but the first screen)
};
// Where the header's and the bottom's parts are (the window's pixels; w 0: none)
struct Chrome {
    panel::Box title, back, search, kbd, star, close, keys;
    panel::Box home;  // 3.0.11-beta.30: «Головна» - the first screen (w 0: on it)
    std::vector<panel::Box> crumbs;  // the way's parts shown and a «›» between each two
    size_t crumbFirst = 0;           // the first part shown (the earlier ones do not fit)
    std::vector<panel::Box> chips;   // the pack's, the live data's (w 0: none)
    Live live = Live::None;
    int starred = -1;  // the page's star: -1 none, 0 not starred, 1 starred
    float caret = 0;   // the big search's caret, from its field's left
    std::vector<panel::Box> rail;  // the page's rail's sections, at the window's left (refpages::Page::rail)
    int railOn = -1;
    float railW = 0;     // its width (0 no rail); narrow: its icons alone
    bool narrow = false;
};
Chrome chrome(const Layout& L, const refpages::Page& p, const Texts& t);
// What is under a point of the window (its own pixels); `world`: the world map's tiles under the pointer (at its view)
Under under(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, const Chrome& c,
            float x, float y, const WorldLook* world = nullptr);
// The world map's fit in its row (the row's own pixels); 3.0.11-beta.29: at a look's view
worldmap::Fit worldFit(const worldmap::Sheet& sheet, const Placed& row);
worldmap::Fit worldView(const WorldLook& w, const Placed& row);

struct Look {
    Under hover;
    Chrome chrome;  // 3.0.11-beta.26 (was: «back» shown)
    WorldLook world;
    // 3.0.11-beta.14: the Portraits' pictures by their rows' keys, each the row's height square (premultiplied; none:
    // the words from the row's start)
    const std::map<std::string, panel::Picture>* pictures = nullptr;
    int selected = -1;  // 3.0.11-beta.21: the row the arrows picked (drawn as the one under the pointer)
};
// A Portrait's picture's side for a layout (the row's height); 3.0.11-beta.15: a row's icon's (its pictures' key
// "icon:" + the row's icon)
int portraitSide(const Layout& L);
int iconSide(const Layout& L);
// The window's shapes (premultiplied): its ground and frame, the header, the search field, the rows' grounds (the
// tiles, the row under the pointer, the headings' rules, the NPCs' dots), the footer, the scroll bar
void paint(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
           int total, const Look& look);
// 3.0.11-beta.25 (R-77TCM7: «когда по меню вожу, лагат курсор и фпс падает» -
// the whole window painted again, 290 ms at 4K, for each row the pointer went to): what paint makes, made again only
// in `area` of a picture paint made at this layout (else all of it) - the rest kept
void repaint(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows,
             int scroll, int total, const Look& look, const Rect& area);
// Where a look's highlights are drawn, the window's pixels: «back» and the cross under the pointer, the row under it
// and the one the arrows picked (with its band). Another hover changes these of the old look and the new one only.
std::vector<Rect> hotAreas(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
                           const Look& look);
// A row's area as hotAreas has it (a row's with its band, a tile's or a chip's its own; empty when the row is not
// shown whole) - a row whose icon came
Rect rowArea(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, int i);
// 3.0.11-beta.25: a row drawn the same as another (its style, words, link, marks, pictures) - and where a map's frame
// on the world map's row `row` is drawn (empty: the map not on the sheet, the row not shown whole): the world map's
// pointer moved to another map, its page made again, only what changed is painted again
bool sameRow(const refpages::Row& a, const refpages::Row& b);
// 3.0.11-beta.29: of the look's map under the pointer - its frame and its card (empty: none, the row not shown)
Rect worldArea(const Layout& L, const std::vector<Placed>& rows, int scroll, int row, const WorldLook& w);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = panel::kText;
    float px = 14;
    bool bold = false;
    Align align = Align::Left;
    bool wrap = false;  // 3.0.11-beta.16: a Text row's lines broken at words
};
std::vector<Words> words(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
                         const Texts& t, const Look& look);

// The concept's style A colours (its kit: --text, --muted, --faint, --accent, --accent2)
constexpr uint32_t kText = 0xECEFF5, kMuted = 0x9AA3B5, kFaint = 0x7D8597, kAccent = 0xC9A55B, kAccent2 = 0xE6CB8A;
constexpr uint32_t kGold = kAccent2, kLink = kText;  // a boss and a link's words

}  // namespace uagfx::refwin
