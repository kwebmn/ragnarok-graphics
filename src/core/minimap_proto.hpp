#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::minimap {

// 3.0.11-beta.1 (stage 1 of the new interface, the owner's «давай» 2026-10-28): the minimap prototype ([interface]
// minimap = 1) - the game's own minimap hidden (its UI window 14 deleted as Ctrl+Tab does), the driver's panel in its
// corner: the map's picture round the player, the player's mark, the player's cell under it (to set against /where),
// the mouse over it kept from the game. The parts of it that need no Windows.
// 3.0.11-beta.31 (the owner: «нету кнопочок + и -, нету возможности карту развернуть на весь екран», «описание под
// картой надо иметь возможность сворачивать», «поработать над дизайном карты»): the concept's look (v19_town, the
// tc_* cards) - a square with gold corners, the player's arrow where the camera looks and its view, the portals, the
// way in gold along the walked cells; its zoom, its bar under it (the name, the cell, ▾ the details), the way's bar,
// and the map big in the screen's middle.

struct Cell {
    int x = 0, y = 0;  // the map's cell: x east, y north (the .gat's, as /where says it)
};
// The player's cell by uaRO.exe's own minimap formula (FUN_0055c220, single precision): x / cell + (w / 2) and
// z / cell + (h / 2), each cut toward zero (__ftol); false when it is not a place on the map (no world, another build)
bool cellOf(float worldX, float worldZ, int width, int height, int cellSize, Cell& out);

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

// 3.0.11-beta.31: the zoom - the cells across the square round the player, the last step the whole map
constexpr int kZoomCells[] = {40, 60, 100, 150, 220};
constexpr int kZoomSteps = 6;  // those and the whole map
constexpr int kZoomFirst = 2;  // 100 cells, as before
int cellsAt(int step, const MapSize& map);

// The panel in the target's top right corner, where the game's own minimap was: the map's square (200 px at scale 1,
// 10 from the edges), its buttons in its bottom left corner (+, −, the big map), the bar under it, the way's bar, the
// details (the map's monsters). The boxes in the panel's own pixels; w 0: not there
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the panel in the target
    panel::Box map, zoomIn, zoomOut, expand, caption, fold, route, stop, list;
    float lineH = 0;
};
Layout layout(int targetW, int targetH, float scale, bool route, int listLines);
enum class Part : uint8_t { None, Map, ZoomIn, ZoomOut, Expand, Caption, Fold, Route, Stop, List };
Part partAt(const Layout& L, float x, float y);
enum class BigPart : uint8_t { None, Header, Close, Map };  // the big map's (below)
int lineAt(const Layout& L, float y);  // the details' line under y, -1 none

// 3.0.11-beta.72 (the owner: «сами картинки карт маленькие и не аккуратные», «розовые полосы по краям»): the map's
// picture ready to be shown - the game's magenta and the pinkish pixels an upscale smudges round it see-through
// (keyedPink; and pinkish ones within two pixels of the key - a lossy WebP's fringe), premultiplied, halved down
// level by level (each pixel the average of four): a view takes the smallest level with as many pixels a cell as it
// shows, between its four nearest pixels - the edges smooth into the ground, no pink, no shimmer far out
struct MapPicture {
    struct Level {
        int w = 0, h = 0;
        std::vector<uint32_t> px;  // premultiplied
    };
    std::vector<Level> levels;  // the picture's own size first
};
// See-through: no alpha, or min(red, blue) over green by more than 110 (inline: the world map's too, without the rest)
inline bool keyedPink(uint32_t argb) {
    const int r = int((argb >> 16) & 0xFF), g = int((argb >> 8) & 0xFF), b = int(argb & 0xFF);
    return !(argb >> 24) || (r < b ? r : b) - g > 110;
}
MapPicture prepare(const std::vector<uint32_t>& argb, int width, int height);

// What the map shows
// 3.0.11-beta.83 (the owner's 3): Member - a party member, its face in a round with a blue ring (a dead one's red)
enum class Glyph : uint8_t { Dot, Kafra, Warp, Member };
struct Mark {
    Cell cell;
    uint32_t rgb = 0;
    Glyph glyph = Glyph::Dot;
    std::string label;  // its words: under the pointer; on the big map a warp's and a Kafra's always
    const panel::Picture* picture = nullptr;  // 3.0.11-beta.83: a member's face (premultiplied); null: none
    bool dead = false;                        // a member's
};
struct Scene {
    const std::vector<uint32_t>* argb = nullptr;  // the map's own picture (the game's), the whole map, rows north first
    int imgW = 0, imgH = 0;
    const MapPicture* picture = nullptr;  // 3.0.11-beta.72: it prepared (prepare); none: argb's made each painting
    MapSize map;
    Cell player;
    float lookX = 0, lookY = 0;  // where the camera looks along the ground (east, north); 0, 0 not known: a dot
    // 3.0.11-beta.71 (the owner: «туда куда чар смотрит»): which way the character faces (east, north; facingOf) - the
    // arrow's way; the camera's (look) the fan's. 0, 0 not known: the arrow the camera's way as before
    float headX = 0, headY = 0;
    int cells = 100;             // the zoom
    std::vector<Mark> marks;
    std::vector<Cell> path;  // the way over this map, from the player on (walkpath)
    bool pin = false;        // the way's next move: a gold pin there
    Cell pinCell;
    std::vector<Cell> waypoints;  // 3.0.11-beta.71: the points set on the way before its end - gold dots
    int hoverMark = -1;  // the mark under the pointer: a ring round it
    Part hover = Part::None;
    BigPart bigHover = BigPart::None;
    float opacity = 1;  // Ctrl+Tab's see-through
    // 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment, [interface] minimap_reach): the
    // map's cells reached on foot from the player (walkgrid::partMask: map.w x map.h, rows from y 0) - the others
    // dimmed; null or another size: none
    const std::vector<uint8_t>* reach = nullptr;
};
constexpr uint32_t kRouteRgb = 0xF2C14E;  // 3.0.11-beta.31: the way's gold (the concept's; was blue)

// The words over the panel (win/minimap_proto draws them by GDI)
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
struct Line {
    std::string left, right;
    uint32_t leftRgb = 0, rightRgb = 0;  // 0: the text's, the muted
    bool bold = false;
};
struct Texts {
    std::string name, cell;   // the bar's: the map's short name, the player's cell
    bool folded = false;      // the details hidden: ▸
    std::vector<Line> route;  // the way's bar: its two lines
    std::vector<Line> list;   // the details
};
// The panel's picture (premultiplied: see-through round its parts), the words over it
void paint(panel::Picture& out, const Layout& L, const Scene& sc, const Texts& t);
std::vector<Words> words(const Layout& L, const Scene& sc, const Texts& t);
// The mark under a point of the panel (its pixels, the map's square): the nearest within its reach, -1 none
int markAt(const Layout& L, const Scene& sc, float x, float y);

// 3.0.11-beta.31 (the owner: «развернуть на весь екран»): the map big in the target's middle - the whole map fitted,
// its marks with their words, the way, the player; its bar (the name, the cell, ×)
struct Big {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // in the target
    panel::Box header, close, map;   // its own pixels
};
Big bigLayout(int targetW, int targetH, float scale, const MapSize& map);
BigPart bigPartAt(const Big& b, float x, float y);
// The map's picture alone at the big map's size (made once a map and a size) and the rest over a copy of it
void paintBigMap(panel::Picture& out, const Big& b, const Scene& sc);
void paintBig(panel::Picture& out, const panel::Picture& map, const Big& b, const Scene& sc);
std::vector<Words> bigWords(const Big& b, const Scene& sc, const std::string& title, const std::string& cell);
int bigMarkAt(const Big& b, const Scene& sc, float x, float y);

// 3.0.11-beta.71 (the owner: «иконка игрока не поворачивается в сторону куда идешь», «туда куда чар смотрит»): the
// character's facing as uaRO.exe keeps it - its object + 0x38, degrees from the south toward the east: FUN_00544da0
// turns it toward a point by atan2(east, -north), the packet 0x9c sets (dir + 4) * 45 - as a way east and north;
// false: not a number
bool facingOf(float degrees, float& east, float& north);
// 3.0.11-beta.71 (the owner: «хочется на карте прокладывать путь»): the cell under a point of the minimap's square (the
// panel's pixels) or of the big map's map (its pixels); false: not over the map
bool cellAt(const Layout& L, const Scene& sc, float x, float y, Cell& out);
bool bigCellAt(const Big& b, const Scene& sc, float x, float y, Cell& out);

// 3.0.11-beta.4 (the owner: «на Ctrl+Tab ничего не происходит»): Ctrl+Tab goes round the game's own way - opaque,
// see-through (the game's 0xAA of 0xFF), hidden
enum class Look { Opaque, Faded, Hidden };
Look next(Look l);
float opacity(Look l);

}  // namespace uagfx::minimap
