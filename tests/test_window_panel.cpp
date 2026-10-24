// 3.0.3-beta.7: the panel of the game windows - laid out at a scale, hit, kept at its place in any window, its shapes
// drawn (core/window_panel)
#include <cmath>
#include <string>
#include <vector>

#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/states.hpp"
#include "core/window_panel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t alphaOf(uint32_t c) { return c >> 24; }
uint32_t redOf(uint32_t c) { return (c >> 16) & 0xFF; }
uint32_t greenOf(uint32_t c) { return (c >> 8) & 0xFF; }
uint32_t blueOf(uint32_t c) { return c & 0xFF; }
uint32_t at(const panel::Picture& p, float x, float y) {
    return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))];
}

}  // namespace

TEST(window_panel_a_row_of_three) {
    const panel::Geometry g = panel::geometry(3, panel::Layout::Row, 1.0f, 1024);
    CHECK_EQ(g.width, 474);
    CHECK_EQ(g.height, 82);
    CHECK(!g.narrow);
    CHECK_EQ(g.cards.size(), size_t(3));
    if (g.cards.size() != 3) return;
    CHECK(g.cards[0].plate.x == 6 && g.cards[0].plate.y == 24 && g.cards[0].plate.w == 150);
    CHECK(g.cards[2].plate.x == 318);
    CHECK(g.cards[1].levels.w > 0);
    CHECK(g.cards[1].hp.y < g.cards[1].sp.y && g.cards[1].sp.y + g.cards[1].sp.h <= g.cards[1].plate.y + 52);
}

TEST(window_panel_a_row_too_wide_for_half_the_window_narrows) {
    const panel::Geometry g = panel::geometry(3, panel::Layout::Row, 1.0f, 800);
    CHECK(g.narrow);
    CHECK_EQ(g.width, 336);
    if (!g.cards.empty()) CHECK(g.cards[0].levels.w == 0);  // the number and the start of the name
}

TEST(window_panel_a_column_of_three) {
    const panel::Geometry g = panel::geometry(3, panel::Layout::Column, 1.0f, 1024);
    CHECK_EQ(g.width, 222);
    CHECK_EQ(g.height, 184);
    if (g.cards.size() == 3) CHECK(g.cards[1].plate.y == 77 && g.cards[1].plate.x == 6);
}

TEST(window_panel_grows_with_the_scale) {
    const panel::Geometry g = panel::geometry(3, panel::Layout::Row, 2.0f, 3840);
    CHECK_EQ(g.width, 948);
    CHECK_EQ(g.height, 164);
    CHECK(g.scale == 2.0f);
}

TEST(window_panel_hits_its_parts) {
    const panel::Geometry g = panel::geometry(3, panel::Layout::Row, 1.0f, 1024);
    CHECK(panel::hitTest(g, 40, 10).part == panel::Part::Top);
    CHECK(panel::hitTest(g, g.close.x + g.close.w / 2, g.close.y + g.close.h / 2).part == panel::Part::Close);
    CHECK(panel::hitTest(g, g.layout.x + g.layout.w / 2, g.layout.y + g.layout.h / 2).part == panel::Part::Layout);
    CHECK(g.layout.x + g.layout.w <= g.close.x && g.title.x + g.title.w <= g.layout.x);
    const panel::Hit h = panel::hitTest(g, 170, 50);
    CHECK(h.part == panel::Part::Card && h.card == 1);
    CHECK(panel::hitTest(g, 159, 50).part == panel::Part::None);  // between two cards
    CHECK(panel::hitTest(g, 500, 50).part == panel::Part::None);
}

TEST(window_panel_keeps_to_its_edge_in_any_window) {
    const panel::Place left{0.0f, 0.2f};
    panel::Point p = panel::position(left, 474, 82, 1024, 768, 1.0f);
    CHECK(p.x == 8 && p.y == 142);  // 8 + 0.2 x (768 - 82 - 16)
    const panel::Place right{1.0f, 0.5f};
    p = panel::position(right, 474, 82, 1024, 768, 1.0f);
    CHECK_EQ(p.x, 1024 - 474 - 8);
    p = panel::position(right, 948, 164, 3840, 2160, 2.0f);
    CHECK_EQ(p.x, 3840 - 948 - 16);  // at the right edge in the big window too
    const panel::Place back = panel::placeOf(panel::Point{542, 343}, 474, 82, 1024, 768, 1.0f);
    CHECK(std::fabs(back.x - 1.0f) < 0.01f && std::fabs(back.y - 0.5f) < 0.01f);
    const panel::Place out = panel::placeOf(panel::Point{-50, 5000}, 474, 82, 1024, 768, 1.0f);
    CHECK(out.x == 0.0f && out.y == 1.0f);
    p = panel::position(right, 474, 82, 300, 60, 1.0f);  // a window smaller than the panel: at its corner
    CHECK(p.x == 8 && p.y == 8);
}

TEST(window_panel_place_in_the_file) {
    const panel::Place p = panel::parsePlace("0.250 0.750");
    CHECK(std::fabs(p.x - 0.25f) < 1e-4f && std::fabs(p.y - 0.75f) < 1e-4f);
    CHECK_EQ(panel::formatPlace(p), std::string("0.250 0.750"));
    const panel::Place d = panel::parsePlace("left top");
    CHECK(d.x == 0.0f && std::fabs(d.y - 0.2f) < 1e-4f);
    const panel::Place c = panel::parsePlace("1.7 -3");
    CHECK(c.x == 1.0f && c.y == 0.0f);
}

TEST(window_panel_bars_red_under_a_quarter) {
    CHECK_EQ(panel::barColour(85, true), panel::barColour(25, true));
    CHECK(panel::barColour(24, true) != panel::barColour(25, true));
    CHECK_EQ(panel::barColour(24, true), panel::barColour(10, false));  // SP as HP: the game's gauges both
    CHECK(panel::barColour(40, false) != panel::barColour(40, true));
}

TEST(window_panel_paints_its_shapes) {
    const panel::Geometry g = panel::geometry(2, panel::Layout::Row, 1.0f, 1024);
    std::vector<panel::Card> cards(2);
    cards[0].current = true;
    cards[0].hp = 85;
    cards[0].sp = 40;
    cards[1].hp = 10;
    panel::Picture pic;
    panel::paint(pic, g, cards, panel::Part::None, panel::Layout::Row);
    CHECK_EQ(pic.width, g.width);
    CHECK_EQ(pic.height, g.height);
    CHECK_EQ(pic.px.size(), size_t(g.width) * size_t(g.height));
    if (pic.px.size() != size_t(g.width) * size_t(g.height)) return;
    CHECK_EQ(alphaOf(at(pic, 0, 0)), 0u);  // the rounded corner
    const uint32_t ground = at(pic, g.width / 2.0f, 2);  // the dark top row
    CHECK(alphaOf(ground) > 200 && redOf(ground) < 40);
    const panel::CardBoxes& c0 = g.cards[0];
    const uint32_t plate = at(pic, c0.plate.x + 3, c0.plate.y + c0.plate.h - 4);  // the current card: blue
    CHECK(blueOf(plate) > redOf(plate) + 15);
    const uint32_t hpStart = at(pic, c0.hp.x + 4, c0.hp.y + c0.hp.h / 2);
    CHECK(greenOf(hpStart) > redOf(hpStart) + 60);  // HP 85 %: green
    const uint32_t hpEnd = at(pic, c0.hp.x + c0.hp.w - 3, c0.hp.y + c0.hp.h / 2);
    CHECK(greenOf(hpEnd) < 120);  // past 85 %: the track
    const panel::CardBoxes& c1 = g.cards[1];
    const uint32_t low = at(pic, c1.hp.x + 3, c1.hp.y + c1.hp.h / 2);
    CHECK(redOf(low) > greenOf(low) + 60);  // HP 10 %: red
    const uint32_t noSp = at(pic, c1.sp.x + 6, c1.sp.y + c1.sp.h / 2);  // SP not known: no bar, no track
    CHECK(redOf(noSp) < 90 && greenOf(noSp) < 90 && blueOf(noSp) < 90);
}

TEST(window_panel_layout_button_shows_the_other_layout) {
    const panel::Geometry g = panel::geometry(2, panel::Layout::Row, 2.0f, 3840);
    const std::vector<panel::Card> cards(2);
    panel::Picture row, column;
    panel::paint(row, g, cards, panel::Part::None, panel::Layout::Row);
    panel::paint(column, g, cards, panel::Part::None, panel::Layout::Column);
    int differ = 0;  // the button's icon: three bars one under another (to a column) or side by side (to a row)
    for (int y = int(g.layout.y); y < int(g.layout.y + g.layout.h); ++y)
        for (int x = int(g.layout.x); x < int(g.layout.x + g.layout.w); ++x)
            differ += at(row, float(x), float(y)) != at(column, float(x), float(y));
    CHECK(differ > 20);
    int outside = 0;
    for (size_t i = 0; i < row.px.size(); ++i) outside += row.px[i] != column.px[i];
    CHECK_EQ(outside, differ);  // nothing else changes
}

TEST(window_panel_fades_as_a_whole) {
    panel::Picture p;
    p.width = 2;
    p.height = 1;
    p.px = {0xE0102030u, 0x80FFFFFFu};
    panel::fade(p, 0.5f);
    CHECK_EQ(alphaOf(p.px[0]), 0x70u);
    CHECK_EQ(redOf(p.px[0]), 0x08u);
    CHECK_EQ(blueOf(p.px[0]), 0x18u);
    CHECK_EQ(alphaOf(p.px[1]), 0x40u);
    panel::fade(p, 1.0f);
    CHECK_EQ(alphaOf(p.px[1]), 0x40u);
}

namespace {

// A quad of the game's interface (two triangles) from x0,y0 to x1,y1
void quad(FrameRecord& r, float x0, float y0, float x1, float y1, bool ui) {
    DrawSetup s;
    s.ui = ui;
    DrawCmd cmd;
    cmd.state = r.addState(s);
    cmd.fvf = d3d7::FVF_TL1;
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    const float xs[6] = {x0, x1, x0, x1, x1, x0}, ys[6] = {y0, y0, y1, y0, y1, y1};
    float v[6][8] = {};
    for (int i = 0; i < 6; ++i) {
        v[i][0] = xs[i];
        v[i][1] = ys[i];
        v[i][3] = 1;
    }
    r.draw(cmd, v, 6, nullptr, 0);
}

}  // namespace

// 3.0.3-beta.9 (the user: the mouse over the panel «меняет цвет»): the game's own cursor - its last small interface
// quad under the cursor's point - found, so the panel goes under it
TEST(window_panel_finds_the_games_cursor) {
    FrameRecord r;
    quad(r, 0, 0, 800, 600, true);       // a window of the game under the cursor
    quad(r, 395, 295, 410, 310, false);  // a sprite of the world there
    quad(r, 400, 300, 432, 332, true);   // the cursor's
    quad(r, 10, 10, 60, 30, true);       // drawn after it, somewhere else
    CHECK_EQ(panel::cursorDraw(r, 402, 303), 2);
    CHECK_EQ(panel::cursorDraw(r, 700, 500), -1);  // nothing small there
    FrameRecord none;
    CHECK_EQ(panel::cursorDraw(none, 402, 303), -1);
}
