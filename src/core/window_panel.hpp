#pragma once
#include "core/ui_scene.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::panel {

// 3.0.3-beta.7 (the user: the game windows' panel in the game - «делай современно», «у каждого свое разрешение
// екрана»): the panel's shapes laid out for a number of windows at a scale (the game's interface scale x [windows]
// panel_size), drawn into a premultiplied BGRA picture; its words go over it by GDI (win/window_panel).
// 3.0.3-beta.11 (the user: «супер минималистичный режим ... иконки маленькие ... чтобы не занимало много места»):
// Icons - small square tiles with the windows' numbers and bars in a row, a grip at its start; the name under the mouse
// in a tip of its own (tipPlace, paintTip)
enum class Layout : uint8_t { Row, Column, Icons };

struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
struct CardBoxes {
    Box plate, name, levels, hp, sp;  // levels w 0: narrowed, no room for them; name and levels w 0 too: icons
    Box number;                       // the window's number: its badge; an icon's upper part
    float badgeX = 0, badgeY = 0, badgeR = 0;
};
struct Geometry {
    int width = 0, height = 0;  // the picture's
    float scale = 1, radius = 0;
    Box top, grip, title, close;  // dragged by its top row; the close «x» hides it
    Box layout;  // the button that lays the cards the other way (the user: «по горизонтали так и по вертикали»)
    std::vector<CardBoxes> cards;
    bool narrow = false;  // a row wider than half the window: the number, the start of the name and the bars
};
Geometry geometry(int count, Layout layout, float scale, int roomWidth);

enum class Part : uint8_t { None, Top, Close, Layout, Card };
struct Hit {
    Part part = Part::None;
    int card = -1;
};
Hit hitTest(const Geometry& g, float x, float y);

// Where it stands: fractions of the room left beside it (0 the left or top edge, 1 the right or bottom), so it keeps
// to its edge in a window of any size; 8 (x the scale) from the window's edges. The first place: left, under the
// game's character window.
struct Place {
    float x = 0.0f, y = 0.2f;
};
Place parsePlace(const std::string& text);  // "0.000 0.200"; not two numbers: the first place
std::string formatPlace(const Place& p);
struct Point {
    int x = 0, y = 0;
};
Point position(const Place& p, int panelW, int panelH, int roomW, int roomH, float scale);
Place placeOf(Point at, int panelW, int panelH, int roomW, int roomH, float scale);

struct Card {
    bool current = false, hover = false;
    int hp = -1, sp = -1;  // percent; -1 not known: no bar
    // 3.0.4-beta.4: its character fallen (a red card), hit (orange), whispered to (a blue dot in its corner) -
    // core/game_windows marks()
    bool died = false, hit = false, whisper = false;
};
struct Picture {
    int width = 0, height = 0;
    std::vector<uint32_t> px;  // premultiplied 0xAARRGGBB, rows top down
    // 3.0.11-beta.25: the pixels the shapes may change - [clipX0, clipX1) x [clipY0, clipY1); all of them by default
    int clipX0 = 0, clipY0 = 0, clipX1 = 1 << 30, clipY1 = 1 << 30;
    // 3.0.18-beta.12 (uaUI, core/ui_scene): its shapes recorded here instead of painted, while set - blank() sets it
    // to its own scene while recording() (else none); copied with the picture
    uiscene::Scene* scene = nullptr;
    uiscene::SceneBox recorded;
    Picture() = default;
    Picture(const Picture& o) : width(o.width), height(o.height), px(o.px), clipX0(o.clipX0), clipY0(o.clipY0),
        clipX1(o.clipX1), clipY1(o.clipY1), recorded(o.recorded) { scene = o.scene ? recorded.s.get() : nullptr; }
    Picture& operator=(const Picture& o) {
        if (this == &o) return *this;
        width = o.width, height = o.height, px = o.px;
        clipX0 = o.clipX0, clipY0 = o.clipY0, clipX1 = o.clipX1, clipY1 = o.clipY1;
        recorded = o.recorded;
        scene = o.scene ? recorded.s.get() : nullptr;
        return *this;
    }
    Picture(Picture&& o) noexcept { *this = std::move(o); }
    Picture& operator=(Picture&& o) noexcept {
        width = o.width, height = o.height, px = std::move(o.px);
        clipX0 = o.clipX0, clipY0 = o.clipY0, clipX1 = o.clipX1, clipY1 = o.clipY1;
        const bool had = o.scene != nullptr;
        recorded = std::move(o.recorded);
        scene = had ? recorded.s.get() : nullptr;
        o.scene = nullptr;
        return *this;
    }
};
// 3.0.18-beta.12: uaUI's - while on, blank() gives each picture its own scene to record into (the game's thread)
void setRecording(bool on);
// `hovered`: the top row's button under the cursor (Close, Layout); `layout`: the cards' now - the button shows the
// other one
void paint(Picture& out, const Geometry& g, const std::vector<Card>& cards, Part hovered, Layout layout);
// 3.0.3-beta.11: the tip with the name of the icon under the mouse - its top left for a tip w x h (the target's pixels)
// above the tile (`tile` where it is in the target), centred on it; no room above: under it; inside the room
Point tipPlace(const Box& tile, int w, int h, int roomW, int roomH, float scale);
void paintTip(Picture& out, int w, int h, float scale);  // its ground; its words go over it by GDI
// The whole picture (premultiplied) at an opacity: [panel] opacity
void fade(Picture& p, float opacity);
// 3.0.11-beta.57 (R-T942DZ): a part's picture blank at w x h for its painting - see-through, its clip all of it - in
// the memory it had (a megabyte at 4K, not taken anew page by page at each painting); false: w or h none
bool blank(Picture& p, int w, int h);
// 3.0.3-beta.9 (the user: the mouse over the panel «меняет цвет»): the command of the game's own cursor in a frame -
// its last small interface quad over the cursor's point (game pixels) among the frame's last draws; -1 none (the
// panel then goes over everything, Windows' arrow over it). 3.0.3-beta.11 (R-45N3WS: still dark on prontera): uaRO.exe
// draws it as an interface face of the colour kCursorColour (FUN_004f60e0 -> FUN_004f5a90) and the screen's additive
// faces after it (FUN_0040f9e0), often more than those last draws - the last interface draw of that colour over the
// point among all the frame's draws first
constexpr uint32_t kCursorColour = 0xFAFFFFFF;
// 3.0.3-beta.12 (R-KVTEMT: the cursor still under the panel - at the present the record was empty): the game's
// EndScene sends its scene to the GPU before the present (Device7::EndScene flushes), its cursor with it - the panel
// goes into the frame there, once a frame; at the present only into a picture drawn without a scene (`drawn`: the game
// drew since the last present)
class FrameSlot {
public:
    bool endScene(bool drawn) {
        if (in_ || !drawn) return false;
        in_ = true;
        return true;
    }
    bool present(bool drawn) {
        const bool now = drawn && !in_;
        in_ = false;
        return now;
    }

private:
    bool in_ = false;  // in this frame already
};
int cursorDraw(const FrameRecord& r, float x, float y);
// 3.0.3-beta.10 (the user: «при нажатии правой кнопкой мышки ... опции разные задавать»): the panel's menu - rows
// of choices (segments side by side, one chosen), switches (a box) and actions, a thin line between kinds; its words
// go over it by GDI as the panel's
struct MenuRow {
    enum class Kind : uint8_t { Choice, Check, Action };
    Kind kind = Kind::Action;
    int segments = 0;  // Choice's
    int chosen = -1;   // Choice's
    bool on = false;   // Check's
};
struct MenuGeometry {
    int width = 0, height = 0;
    float scale = 1;
    std::vector<Box> rows, label, check;     // a row's place, its words', a Check's box
    std::vector<std::vector<Box>> segments;  // a Choice's
    std::vector<float> lines;                // the lines between kinds (their y)
};
MenuGeometry menuGeometry(const std::vector<MenuRow>& rows, float scale);
struct MenuHit {
    int row = -1, segment = -1;
};
MenuHit menuHit(const MenuGeometry& g, float x, float y);
void paintMenu(Picture& out, const MenuGeometry& g, const std::vector<MenuRow>& rows, MenuHit hover);
// Its rows, in this order: the cards' layout (a row, a column), [panel] size (kMenuSizes) and opacity
// (kMenuOpacities); the other game windows' music, sounds and light ([windows] others_*); hide the panel. A size or
// an opacity the menu does not offer (set in the settings): none of its own chosen
constexpr int kMenuSizes[] = {75, 100, 125, 150};
constexpr int kMenuOpacities[] = {60, 70, 80, 90, 100};
std::vector<MenuRow> menuRows(const Config& c);
struct MenuText {
    std::string label;
    std::vector<std::string> segments;  // a choice's
};
std::vector<MenuText> menuTexts(settings::Lang lang);  // UTF-8
// A click on a row (a choice: on one of its segments) - the setting changed in `c` and the line the file gets; hide:
// the panel to be hidden (its own way, win/window_panel). false: nothing to do there
struct MenuChange {
    std::string section, key, value;
    bool hide = false;
};
bool menuPick(Config& c, MenuHit hit, MenuChange& change);
// A colour (0xRRGGBB) at an opacity over a premultiplied pixel: the words' coverage over the picture
void over(uint32_t& pixel, uint32_t rgb, float opacity);
// 3.0.11-beta.73 (R-WXNNU2: the minimap painted in 13 ms in the game, 3.5 ms in the tests' build): the 32-bit build's
// lroundf and hypotf are library calls on the x87 (its control word changed for each) - a pixel's own inline: the same
// numbers as std::lround (|v| < 2^23), a distance as the root of the squares
inline long roundOf(float v) {  // no branches: a blend's channels are random (5.7 ns an over, 36 with branches)
    const long t = long(v);         // toward zero
    const float f = v - float(t);
    return t + long(f >= 0.5f) - long(f <= -0.5f);
}
inline float length(float x, float y) { return std::sqrt(x * x + y * y); }
// 3.0.9-beta.4: the panel's shapes for the driver's other pictures (core/answer_card) - a rounded box filled (and a
// ring `edge` wide inside its border) and a circle, their edges smooth, over what the picture has
void roundRect(Picture& p, const Box& b, float r, uint32_t fill, float fillA, uint32_t edgeRgb = 0, float edgeA = 0,
               float edge = 0);
void circle(Picture& p, float cx, float cy, float r, uint32_t rgb, float a);
// 3.0.18-beta.12: a primitive into the picture's scene (p.scene set), with the picture's clip
void record(Picture& p, uiscene::Prim prim);
// 3.0.18-beta.12: a picture put at (x, y) at its own size into p's scene - its pixels copied (grey: by their
// brightness, as the hotbar's), within the circle of radius `disc` round its middle when > 0
void recordImage(Picture& p, const Picture& pic, float x, float y, float opacity, float disc = 0, bool grey = false);
// 3.0.11-beta.35: words' width as GDI draws them at `px` (an estimate a little wide: boxes made from it hold the words)
float wordsWidth(const std::string& utf8, float px, bool bold);
// 3.0.18-beta.57: the engine that draws the words (uaUI's GDI faces) measures them instead of the table while set:
// its width (< 0: not known - the table's) and a pixel over; nullptr: the table again
using Measure = float (*)(const std::string& utf8, float px, bool bold);
void setMeasure(Measure m);
// 3.0.11-beta.196: how far a smaller word's box drops to stand on a bigger word's line beside it, both drawn centred in
// their boxes (GDI's DT_VCENTER over Plex's cell: 1025 over its line, 275 under - the line 0.375 of a size under the
// middle)
inline float baselineDrop(float bigPx, float smallPx) { return 0.375f * (bigPx - smallPx); }
// A bar's colour (0xRRGGBB): red under 25 % as the game's own gauges (0x4490d0); else green HP, blue SP
uint32_t barColour(int percent, bool hp);
constexpr uint32_t kText = 0xECEFF5, kMuted = 0x969EAE, kOnBadge = 0x0F172A;  // the words' colours

}  // namespace uagfx::panel
