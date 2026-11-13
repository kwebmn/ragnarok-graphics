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

// 3.0.18-beta.241 (docs/superpowers/plans/2026-11-13-reference-rewrite.md, stage 4): what is left of the old hand
// layout of «Довідник» (core/reference_window, 3.0.11-beta.7 .. 3.0.18-beta.238) - the world map's painter. The live
// window (win/reference_window, uikit/reference_live) lays out the rest; this paints the world map's picture at a box
// (the target's pixels), says what is under a point of it and the words over it.

// The words' width at a size (the target's pixels): GDI's in the driver (win/picture_draw), guessed by the letters
// without a font (tests)
using Measure = float (*)(const std::string& utf8, float px, bool bold);
float guessWidth(const std::string& utf8, float px, bool bold);

struct Layout {
    float s = 1;                   // the scale (1 at 1080p): the document's dp
    Measure measure = guessWidth;  // the chips' and the way's words
};

// What is under a point of the world map: Tile - its tile (worldmap::Sheet::world), Chain - a map of the dungeon box
// (tile: its place in WorldLook::chain), Body - the map's ground
enum class Part : uint8_t { None, Body, Tile, Chain };
struct Under {
    Part part = Part::None;
    int row = -1;
    int tile = -1;
};
// 3.0.11-beta.64: a map of the dungeon box - its code, its name, its picture (the client's minimap at chainSide()
// square, premultiplied; null: the sea's colour)
struct ChainMap {
    std::string map, name;
    const panel::Picture* picture = nullptr;
};
// 3.0.11-beta.11: the world map and what goes over it; 3.0.11-beta.29 the concept's v19_route: the view zoomed and
// moved, the players' green pills, the towns' names, «Ви тут», the way in gold with its teleports' dashed arcs, its
// end's pin, the card of the map under the pointer
struct WorldLook {
    const panel::Picture* picture = nullptr;  // the sheet at the view (its size; win/reference_window)
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

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = panel::kText;
    float px = 14;
    bool bold = false;
    Align align = Align::Left;
    bool wrap = false;
};

// 3.0.18-beta.248 (R-QP7PQB): the world map's background at the view - the sheet's own pixels (argb, `detail` times
// its size) scaled to w x h at the zoom and centre; made again only when the view or the size changed (`made` the
// view it was made at; false: as it was). WorldLook::picture is this one, never the picture painted over it
bool backgroundAt(panel::Picture& back, worldmap::Fit& made, const std::vector<uint32_t>& argb, int sheetW, int sheetH,
                  int detail, int w, int h, float zoom, float cx, float cy);
// The world map's picture alone at b's size; what is under a point of it (its own pixels); the words over it (the
// towns, the card of the map under the pointer, the dungeon box's)
void paintWorldPicture(panel::Picture& out, const Layout& L, const panel::Box& b, const WorldLook& w);
Under worldUnder(const Layout& L, const panel::Box& b, const WorldLook& w, float x, float y);
std::vector<Words> worldWordsIn(const Layout& L, const WorldLook& w, const panel::Box& b);

}  // namespace uagfx::refwin
