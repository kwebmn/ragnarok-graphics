#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/reference_pages.hpp"
#include "core/window_panel.hpp"
#include "core/world_map.hpp"

namespace uagfx::refwin {

// 3.0.11-beta.7: the «Довідник» window's shapes (win/reference_window draws it): 1270 x 850 at 1080p (the concept's),
// centred, smaller on a small screen; a header with «‹ Назад до …» and ×, the search field, the page's rows scrolled
// (the first screen's tiles three to a line), the data pack's line at the bottom. The target's pixels throughout.

struct Layout {
    int x = 0, y = 0, w = 0, h = 0;  // the window
    int header = 0, search = 0, footer = 0, pad = 0;
    int bodyY = 0, bodyH = 0;  // the scrolled part, from the window's top
    float s = 1;
};
Layout layout(int targetW, int targetH, float scale);

struct Placed {  // a row's place in the body: from the body's top before the scroll
    int x = 0, y = 0, w = 0, h = 0;
};
std::vector<Placed> place(const Layout& L, const refpages::Page& p, int& total);
int clampScroll(int scroll, int total, int bodyH);
bool shown(const Layout& L, const Placed& r, int scroll);  // the whole row in the body

enum class Part : uint8_t { None, Close, Back, Search, Row, Body, Tile };
struct Under {
    Part part = Part::None;
    int row = -1;   // Part::Row: a row that leads somewhere; Part::Tile: the world map's row
    int tile = -1;  // 3.0.11-beta.11: Part::Tile - the world map's tile (worldmap::Sheet::world)
};
// 3.0.11-beta.11: the world map's row - its picture and what goes over it
struct WorldLook {
    const panel::Picture* picture = nullptr;  // the tiles fitted into the row (its size; win/reference_window)
    const worldmap::Sheet* sheet = nullptr;
    std::string here;
    float hereX = -1, hereY = -1;              // the player on the sheet (-1 not known)
    std::vector<std::string> route;           // the followed way's maps, from here on
    const std::map<std::string, int>* players = nullptr;
    std::string hover;                        // the tile under the pointer
};
// What is under a point of the window (its own pixels); `sheet`: the world map's tiles under the pointer
Under under(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, bool backShown,
            float x, float y, const worldmap::Sheet* sheet = nullptr);
// The world map's fit in its row (the row's own pixels)
worldmap::Fit worldFit(const worldmap::Sheet& sheet, const Placed& row);

struct Look {
    Under hover;
    bool backShown = false;
    WorldLook world;
    // 3.0.11-beta.14: the Portraits' pictures by their rows' keys, each the row's height square (premultiplied; none:
    // the words from the row's start)
    const std::map<std::string, panel::Picture>* pictures = nullptr;
};
// A Portrait's picture's side for a layout (the row's height)
int portraitSide(const Layout& L);
// The window's shapes (premultiplied): its ground and frame, the header, the search field, the rows' grounds (the
// tiles, the row under the pointer, the headings' rules, the NPCs' dots), the footer, the scroll bar
void paint(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
           int total, const Look& look);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = panel::kText;
    float px = 14;
    bool bold = false;
    Align align = Align::Left;
};
struct Texts {
    std::string title, back, query, hint, footer;  // the header's, «‹ Назад до …» ("" none), typed, the field's hint
};
std::vector<Words> words(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
                         const Texts& t, const Look& look);

// The colours of the words
constexpr uint32_t kAccent = 0x8FB8FF, kGold = 0xF5C451, kLink = 0xC9DBFF;

}  // namespace uagfx::refwin
