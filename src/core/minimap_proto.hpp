#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/walk_grid.hpp"
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
// 3.0.11-beta.201 (the tester: «великі іконки коли зум максимальний ... і зменшуються коли збільшується зум»): the
// marks' size for `cells` across the minimap - 0.85 at the closest, about 1 at 100, 1.3 from 300 or the whole map
// 3.0.11-beta.262 (the tester: «у малому вікні зміни майже не помітні ... прикрутити логіку від розміру вікна»; the
// owner: «сделай»): `partScale` the minimap's own (smaller in a small game window) - under 1 the zoom's change grows
// as much (twice at 0.5 and below), so the marks still change by a few of the screen's pixels
float markScale(int cells, const MapSize& map, float partScale = 1.0f);
// 3.0.11-beta.187 (the tester: «коли зум робиш до самого мінімуму, а потім підвищуєш, в перший раз він стає ще
// менший»): the step `by` (-1 nearer, +1 farther) from `from`, past the steps that show the whole map or more (on a
// map under 220 cells the 220 step came after the whole map's, farther out); one standing on such a step is on the
// whole map's
int zoomStep(int from, int by, const MapSize& map);

// 3.0.11-beta.119 (R-5Z6J2S: «потрібно щоб на мінікарті та на великій карті була можливість включати та виключати
// НПЦ, портали та інше»; the owner «делать»): the map's layers - each switched on the maps themselves (a round button
// in the minimap's top left, a chip under the big map) as [interface] minimap_warps, minimap_npcs, minimap_kafra and
// minimap_party switch them
enum class Layer : uint8_t { Warps, Npcs, Kafra, Party };
constexpr int kLayers = 4;

// The panel in the target's top right corner, where the game's own minimap was: the map's square (200 px at scale 1,
// 10 from the edges), its buttons in its bottom left corner (+, −, the big map), the bar under it, the way's bar, the
// details (the map's monsters). The boxes in the panel's own pixels; w 0: not there
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the panel in the target
    panel::Box map, zoomIn, zoomOut, expand, caption, fold, route, stop, list;
    panel::Box layers[kLayers];  // 3.0.11-beta.119: a row in the square's top left
    panel::Box style;            // 3.0.11-beta.120: the picture's switch, after them
    // 3.0.11-beta.127 (the owner: «пинги вынеси в отдельную плашку, можешь еще график строить. но небольшую. там где
    // карта оно название карты сокращает»): the round trip's plate under the bar - its dot and words, a small graph
    panel::Box net;
    // 3.0.11-beta.213 (the owner: «вывод команды @mobsearch показывался на миникарте ... чтобы это можно было потом
    // убрать»): a search's plate under the bar while its marks show - its words, ✕ that clears them
    panel::Box found, foundClear;
    float lineH = 0;
    // 3.0.18-beta.51 (the owner: «увеличил карту, а там иконки маленькие»): the buttons and the marks this much bigger
    // still (an enlarged minimap: its size's root)
    float grow = 1;
};
Layout layout(int targetW, int targetH, float scale, bool route, int listLines, bool net = false, bool found = false,
              float grow = 1.0f);
panel::Box netGraphBox(const Layout& L);  // 3.0.11-beta.127: the plate's graph (w 0 none)
enum class Part : uint8_t {
    None, Map, ZoomIn, ZoomOut, Expand, Caption, Fold, Route, Stop, List, Layer, Style,
    Found, FoundClear  // 3.0.11-beta.213
};
Part partAt(const Layout& L, float x, float y);
int layerAt(const Layout& L, float x, float y);  // 3.0.11-beta.119: the layer's button there, -1 none
enum class BigPart : uint8_t { None, Header, Close, Map, Layer, Style };  // the big map's (below)
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
// 3.0.18-beta.220 (R-5773KD, R-FSNPYE: «Довідник»'s location picture pink round the map): a whole picture keyed -
// its keyedPink pixels see-through (0), the rest left as they are
inline void keyPink(std::vector<uint32_t>& argb) {
    for (uint32_t& c : argb)
        if (keyedPink(c)) c = 0;
}
MapPicture prepare(const std::vector<uint32_t>& argb, int width, int height);
// 3.0.11-beta.201 (the tester: «є 2 типи порталів ... треба прибрати "зайві" (червоні портали)»): the game's map
// pictures mark the warps with pure red dots - the minimap marks them itself; each dot (its pure red middle and its
// soft red edge) filled from round it, ring by ring. The dots taken out
int dropWarpDots(std::vector<uint32_t>& argb, int width, int height);

// 3.0.11-beta.120 (the owner: «карту в отчете, она какаято не красивая, может другую попробовать или сделать
// переключалку слоев разных?»): «Схема» - the map drawn from its walkable cells (walkgrid, the server's .gat): the
// floor light (the part reached on foot - `reach`, walkgrid::partMask's - lighter), walls dark, a darker rim on the
// floor where it meets a wall; `px` pixels a cell, rows north first as the game's picture (for prepare)
constexpr uint32_t kSchemeWall = 0xFF1B2029, kSchemeFloor = 0xFF6F7A8C, kSchemeReached = 0xFFB3BDCB;
constexpr uint32_t kSchemeEdge = 0xFF3C4554;
std::vector<uint32_t> schemePixels(const walkgrid::Grid& g, const std::vector<uint8_t>* reach, int px, int& width,
                                   int& height);

// What the map shows
// 3.0.11-beta.83 (the owner's 3): Member - a party member, its face in a round with a blue ring (a dead one's red)
// 3.0.11-beta.201: Spot - a server's mark (an NPC's viewpoint, @showmobs: the packet 0x0144) in its colour, shown
// whatever the layers, over the rest; Guild - a guild mate (0x01eb), with the party's layer
enum class Glyph : uint8_t { Dot, Kafra, Warp, Member, Spot, Guild };
struct Mark {
    Cell cell;
    uint32_t rgb = 0;
    Glyph glyph = Glyph::Dot;
    std::string label;  // its words: under the pointer; on the big map a warp's and a Kafra's always
    const panel::Picture* picture = nullptr;  // 3.0.11-beta.83: a member's face (premultiplied); null: none
    bool dead = false;                        // a member's
    // 3.0.18-beta.240 (the owner 13.11: «буквы в точках, подписи только у выходов»): its letter in its dot («$», «!»;
    // the dot bigger) on the big views; a warp out of the map - its words always there (the rest under the pointer)
    std::string letter{};
    bool exit = false;
};
// 3.0.18-beta.240: a monster's spawn area (its centre cell, half sizes in cells) - the reference's map shows them
struct ZoneMark {
    Cell cell;
    int rx = 0, ry = 0;
};
struct Layers {
    bool on[kLayers] = {true, true, true, true};
};
// 3.0.13-beta.2 (reports: the minimap the interface's dearest picture - 150-200 paintings in 30 s, 3.2 ms each, half
// of it the map's pixels made anew for every step): the map's pixels round the view made once, a margin about it -
// each painting copies its square out; made anew when the view leaves the margin or the picture, zoom or reach change
struct MapCache {
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    float left = 0, top = 0, cellPx = 0;  // the cache's top-left corner in cells, its zoom
    const void* level = nullptr;          // the picture's level it was made of
    int mapW = 0, mapH = 0;
    std::vector<uint8_t> reach;  // the reach it dimmed by (empty: none)
    int made = 0;                // how many times made (the log, the tests)
};

struct Scene {
    MapCache* cache = nullptr;  // 3.0.13-beta.2: the minimap's (none: the pixels made each painting)
    const std::vector<uint32_t>* argb = nullptr;  // the map's own picture (the game's), the whole map, rows north first
    int imgW = 0, imgH = 0;
    const MapPicture* picture = nullptr;  // 3.0.11-beta.72: it prepared (prepare); none: argb's made each painting
    MapSize map;
    Cell player;
    float lookX = 0, lookY = 0;  // where the camera looks along the ground (east, north); 0, 0 not known: a dot
    // 3.0.11-beta.71 (the owner: «туда куда чар смотрит»): which way the character faces (east, north; facingOf) - the
    // arrow's way; the camera's (look) the fan's. 0, 0 not known: the arrow the camera's way as before
    float headX = 0, headY = 0;
    bool viewFan = true;  // 3.0.11-beta.187: [interface] minimap_view - the camera's fan; off: the arrow alone
    bool classicMarks = false;  // 3.0.11-beta.222: [interface] minimap_classic - guild mates and members as dots
    int cells = 100;             // the zoom
    std::vector<Mark> marks;
    // 3.0.11-beta.201 (the tester: «у легенді зверху - забули поміняти»): the Kafras' face for their layer's button
    // and chip (null: the concept's «K»)
    const panel::Picture* kafraFace = nullptr;
    std::vector<Cell> path;  // the way over this map, from the player on (walkpath)
    bool pin = false;        // the way's next move: a gold pin there
    Cell pinCell;
    std::vector<Cell> waypoints;  // 3.0.11-beta.71: the points set on the way before its end - gold dots
    int hoverMark = -1;  // the mark under the pointer: a ring round it
    Part hover = Part::None;
    BigPart bigHover = BigPart::None;
    float opacity = 1;  // Ctrl+Tab's see-through
    // 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment, [interface] minimap_reach): the
    // map's cells reached on foot from the player (walkgrid::reachMask: map.w x map.h, rows from y 0) - another
    // part's walked-on cells dimmed (3.0.11-beta.126: not the walls nor the ground); null or another size: none
    const std::vector<uint8_t>* reach = nullptr;
    // 3.0.11-beta.127: the plate's graph - the round trips of the last minute (ms), the oldest first; -1 a stall
    const std::vector<int>* netGraph = nullptr;
    Layers layers;        // 3.0.11-beta.119: the buttons' and the chips' looks (the marks given already shown ones)
    bool showPlayer = true;          // 3.0.18-beta.240: false - another map than his (the reference's)
    std::vector<ZoneMark> zones;     // 3.0.18-beta.240: the monster zones (the reference's map view)
    int hoverLayer = -1;  // the layer's button or chip under the pointer
    bool scheme = false;  // 3.0.11-beta.120: «Схема» shown (its switch lit)
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
    std::string layers[kLayers];  // 3.0.11-beta.119: «Портали», «NPC», «Кафра», «Група» (a button pointed at: its name)
    std::string style;            // 3.0.11-beta.120: «Схема» (its button pointed at)
    std::string net;              // 3.0.11-beta.122: the round trip («45 мс», on its plate since .127); "" none
    uint32_t netRgb = 0;          // its colour
    std::string found;            // 3.0.11-beta.213: the search's plate («Poring · 12»); "" none
};
constexpr uint32_t kFoundRgb = 0xFF8A3D;  // 3.0.11-beta.213: a search's marks and its plate's words
// 3.0.18-beta.26 (uaUI's minimap): the map's square alone in `out` (L.map's size: its pixels, the marks, the rounded
// corners); words() split - the Kafras' «K» on the map (the square's pixels) and the rest (the panel's)
void paintMapSquare(panel::Picture& out, const Layout& L, const Scene& sc);
std::vector<Words> mapWords(const Layout& L, const Scene& sc);
std::vector<Words> frameWords(const Layout& L, const Scene& sc, const Texts& t);
// the round trip's graph: a bar a reading (the newest at the right end), its colour by the time
struct NetBar {
    panel::Box box;
    uint32_t rgb = 0;
};
std::vector<NetBar> netBars(const Layout& L, const Scene& sc);
// a layer's button sign and the scheme's, centred in `out` (its size the button's)
void paintLayerSign(panel::Picture& out, Layer l, bool on, const panel::Picture* kafraFace, float s);
void paintSchemeSign(panel::Picture& out);
// The mark under a point of the panel (its pixels, the map's square): the nearest within its reach, -1 none
int markAt(const Layout& L, const Scene& sc, float x, float y);

// 3.0.11-beta.31 (the owner: «развернуть на весь екран»): the map big in the target's middle - the whole map fitted,
// its marks with their words, the way, the player; its bar (the name, the cell, ×)
struct Big {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // in the target
    panel::Box header, close, map;   // its own pixels
    panel::Box chips[kLayers];       // 3.0.11-beta.119: the layers' chips in a row under the map (w 0: none)
    panel::Box styleChip;            // 3.0.11-beta.120: «Схема», after them
};
// `chipWidths` - each layer's chip as wide (its words measured), `styleWidth` the picture's switch's; null: no chips
Big bigLayout(int targetW, int targetH, float scale, const MapSize& map, const float* chipWidths = nullptr,
              float styleWidth = 0);
BigPart bigPartAt(const Big& b, float x, float y);
int bigLayerAt(const Big& b, float x, float y);  // 3.0.11-beta.119: the layer's chip there, -1 none
// The map's picture alone at the big map's size (made once a map and a size) and the rest over a copy of it
void paintBigMap(panel::Picture& out, const Big& b, const Scene& sc);
// 3.0.18-beta.42 (uaUI's big map): the map's square alone in `out` (b.map's size: the map's picture, the marks, the
// labels' grounds - paintBig()'s there)
void paintBigSquare(panel::Picture& out, const panel::Picture& map, const Big& b, const Scene& sc);
std::vector<Words> bigWords(const Big& b, const Scene& sc, const std::string& title, const std::string& cell,
                            const std::string* layerNames = nullptr,  // 3.0.11-beta.119: the chips' words
                            const std::string& styleName = "");       // 3.0.11-beta.120
int bigMarkAt(const Big& b, const Scene& sc, float x, float y);

// 3.0.18-beta.240 (the reference's «одна карта», the owner 13.11): the map at any box, zoom and centre - the big map's
// look (its picture, the zones, the marks one and a half times, the exits' words, the letters) for «Довідник»'s
// location card; zoom 1 the whole map fitted in the box, its limit mapViewMost (2 the game's picture, 4 the x4 one
// and «Схема»)
struct MapView {
    int w = 0, h = 0;
    float cx = -1, cy = -1;  // the cell in the middle (-1: the map's middle)
    float zoom = 1;
    float s = 1;
};
View viewOf(const MapView& m, const MapSize& map);
float mapViewMost(bool sharpPicture);
void zoomMapView(MapView& m, const MapSize& map, float px, float py, float by, float most);  // round (px, py)
void panMapView(MapView& m, const MapSize& map, float dx, float dy);                        // by pixels
void paintMapView(panel::Picture& out, const MapView& m, const Scene& sc);
std::vector<Words> mapViewWords(const MapView& m, const Scene& sc);
int mapViewMarkAt(const MapView& m, const Scene& sc, float x, float y);  // within 12 px of its middle, -1 none
bool mapViewCellAt(const MapView& m, const Scene& sc, float x, float y, Cell& out);

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
