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
// it at scale 1, 8 px from the edges (the target's pixels)
struct Geometry {
    int size = 0, x = 0, y = 0, textH = 0;
};
Geometry geometry(int targetW, int targetH, float scale);
bool inside(const Geometry& g, float x, float y);  // the map's box or its line under it

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
// The panel's picture (premultiplied): the map's picture (ARGB as decoded, the whole map, rows north first; the game's
// magenta see-through) through the view, a dark ground where it has none, the player's mark, the band of the line
// under the map (its words go over it by GDI)
void paint(panel::Picture& out, const Geometry& g, const std::vector<uint32_t>& mapArgb, int mapW, int mapH,
           const MapSize& map, const Cell& player, int cells);

}  // namespace uagfx::minimap
