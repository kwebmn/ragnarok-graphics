#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::worldmap {

// 3.0.11-beta.11 (the owner: the world map from the maps' own tiles as the site's, «не будет общей картинки»): the
// site's world map sheet (its public worldmap.json - the maps' tiles on a 1396 x 1009 sheet; the dungeons by groups)
// and the tiles' pictures put on it - the client's own minimaps (data\texture\유저인터페이스\map\<code>.bmp, the very
// pictures the site's tiles are). No Windows here.

struct Tile {
    std::string map;
    int l = 0, t = 0, w = 0, h = 0;  // on the sheet
};
struct Dungeon {
    std::string map, group;
};
struct Sheet {
    int w = 0, h = 0;
    std::vector<Tile> world;
    std::vector<Dungeon> dungeons;  // in the site's order, group by group
};
bool parse(const std::string& json, Sheet& out);

constexpr uint32_t kGround = 0xFF181B57;  // the site's sea (WorldMapView.vue)

// A tile's picture (ARGB as decoded, rows top down; the game's magenta see-through) put into its place on the sheet
// (ARGB, sheet.w x sheet.h): each pixel the average of the picture's pixels under it
void put(std::vector<uint32_t>& sheetArgb, int sheetW, int sheetH, const Tile& t, const std::vector<uint32_t>& argb,
         int w, int h);
// The sheet fitted into a box: its scale and where its top left is in the box (centred)
struct Fit {
    float s = 0, x = 0, y = 0;
};
Fit fit(int sheetW, int sheetH, int boxW, int boxH);
// The sheet's picture at a fit (premultiplied, opaque; the box's size; the ground around it)
void scaled(panel::Picture& out, const std::vector<uint32_t>& sheetArgb, int sheetW, int sheetH, int boxW, int boxH,
            const Fit& f);
// The tile under a point of the box: its index in sheet.world, -1 none (a smaller tile over a bigger one)
int tileAt(const Sheet& sheet, const Fit& f, float x, float y);
const Tile* tileOf(const Sheet& sheet, const std::string& map);
// A map's cell (x east, y north; the map w x h cells) on the sheet, inside its tile
bool cellOnSheet(const Tile& t, int mapW, int mapH, int x, int y, float& sx, float& sy);

}  // namespace uagfx::worldmap
