#pragma once
#include <cstdint>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::minimap {

// 3.0.11-beta.1 (stage 1 of the new interface, the owner's «давай» 2026-10-28): the minimap prototype ([interface]
// minimap = 1) - the game's own minimap hidden (its UI window 14 deleted as Ctrl+Tab does), the driver's panel in its
// corner: the map's picture round the player, the player's mark, the player's cell under it (to set against /where),
// the mouse over it kept from the game. The parts of it that need no Windows.

struct Cell {
    int x = 0, y = 0;  // the map's cell: x east, y north (the .gat's, as /where says it)
};
// The player's cell by uaRO.exe's own minimap formula (FUN_0055c220, single precision): x / cell + (w / 2) and
// z / cell + (h / 2), each cut toward zero (__ftol); false when it is not a place on the map (no world, another build)
bool cellOf(float worldX, float worldZ, int width, int height, int cellSize, Cell& out);

// The panel in the target's top right corner, where the game's own minimap was: 160 px a side and a line of 18 under
// it at scale 1, 8 px from the edges (the target's pixels); 3.0.11-beta.6: `listLines` lines of 14 more under that
// (the map's monsters)
struct Geometry {
    int size = 0, x = 0, y = 0, textH = 0, listH = 0, lineH = 0;
};
Geometry geometry(int targetW, int targetH, float scale, int listLines = 0);
bool inside(const Geometry& g, float x, float y);  // the map's box, its line or its list under it

struct MapSize {
    int w = 0, h = 0;
};
// `cells` across, centred on the player, kept inside the map (as the mock-ups' map_view): the view's left edge and
// top edge in cells (the top edge as a cell y: north up) and the pixels a cell takes
struct View {
    float left = 0, top = 0, cellPx = 0;
};
View view(const MapSize& map, const Cell& player, int cells, int size);
void cellToPanel(const View& v, const MapSize& map, const Cell& c, float& px, float& py);  // a cell's centre
// 3.0.11-beta.6: a hint's mark on the map - an NPC (a dot of its kind's colour) or a warp to another map (a ring)
struct Mark {
    Cell cell;
    uint32_t rgb = 0;
    bool warp = false;
};
// 3.0.11-beta.9: the way followed (core/route_follow) - its next move's cell: a line from the player to it and a ring
// on it; out of the view the line to the box's edge, a dot there
struct Goal {
    bool shown = false;
    Cell cell;
};
constexpr uint32_t kRouteRgb = 0x5AC8FA;  // the way's line, ring and words
// The panel's picture (premultiplied): the map's picture (ARGB as decoded, the whole map, rows north first; the game's
// magenta see-through) through the view, a dark ground where it has none, the marks, the way's line, the player's mark
// over them, the band of the line and the list under the map (their words go over it by GDI)
void paint(panel::Picture& out, const Geometry& g, const std::vector<uint32_t>& mapArgb, int mapW, int mapH,
           const MapSize& map, const Cell& player, int cells, const std::vector<Mark>& marks = {},
           const Goal& goal = {});
// Where the line from the player to the goal ends in the box (the panel's pixels): the goal itself in the view
// (true), else the point on the box's edge (an inset in) toward it (false)
bool goalPoint(const Geometry& g, const MapSize& map, const Cell& player, int cells, const Cell& goal, float& x,
               float& y);
// The mark under a point of the panel (its pixels, the map's box): the nearest within its reach, -1 none
int markAt(const Geometry& g, const MapSize& map, const Cell& player, int cells, const std::vector<Mark>& marks,
           float x, float y);

// 3.0.11-beta.4 (the owner: «на Ctrl+Tab ничего не происходит»): Ctrl+Tab goes round the game's own way - opaque,
// see-through (the game's 0xAA of 0xFF), hidden
enum class Look { Opaque, Faded, Hidden };
Look next(Look l);
float opacity(Look l);

}  // namespace uagfx::minimap
