// 3.0.3-beta.7: the panel of the game windows - laid out at a scale, hit, kept at its place in any window, its shapes
// drawn (core/window_panel)
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/settings/strings.hpp"
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
void quad(FrameRecord& r, float x0, float y0, float x1, float y1, bool ui, uint32_t diffuse = 0) {
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
        std::memcpy(&v[i][4], &diffuse, 4);
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

// 3.0.3-beta.10 (the user: «при нажатии правой кнопкой мышки ... опции разные задавать»): the panel's menu - rows of
// choices (segments), switches and actions, laid out at a scale, hit, drawn
TEST(window_panel_menu_lays_out_its_rows) {
    const std::vector<panel::MenuRow> rows = {
        {panel::MenuRow::Kind::Choice, 2, 0, false}, {panel::MenuRow::Kind::Choice, 4, 1, false},
        {panel::MenuRow::Kind::Check, 0, 0, true},   {panel::MenuRow::Kind::Action, 0, 0, false}};
    const panel::MenuGeometry g = panel::menuGeometry(rows, 1.0f);
    CHECK_EQ(g.width, 360);
    CHECK_EQ(g.rows.size(), size_t(4));
    CHECK_EQ(g.segments.size(), size_t(4));
    if (g.rows.size() != 4 || g.segments.size() != 4) return;
    CHECK_EQ(g.segments[0].size(), size_t(2));
    CHECK_EQ(g.segments[1].size(), size_t(4));
    CHECK(g.segments[2].empty() && g.segments[3].empty());
    CHECK(g.rows[1].y >= g.rows[0].y + g.rows[0].h && g.height >= int(g.rows[3].y + g.rows[3].h));
    // the segments side by side within their row, right of its label
    CHECK(g.segments[1][0].x > g.label[1].x + g.label[1].w - 1 && g.segments[1][3].x + g.segments[1][3].w <= g.width);
    CHECK(g.segments[1][1].x >= g.segments[1][0].x + g.segments[1][0].w);
    const panel::MenuGeometry big = panel::menuGeometry(rows, 2.0f);
    CHECK_EQ(big.width, 720);
    // five segments (the opacity's): each one wide enough for «100 %»
    const panel::MenuGeometry five = panel::menuGeometry({{panel::MenuRow::Kind::Choice, 5, 0, false}}, 1.0f);
    CHECK(five.segments.size() == 1 && five.segments[0].size() == 5 && five.segments[0][0].w >= 34);
    // three (the layouts): each one wide enough for «Стовпчиком»
    const panel::MenuGeometry three = panel::menuGeometry({{panel::MenuRow::Kind::Choice, 3, 0, false}}, 1.0f);
    CHECK(three.segments.size() == 1 && three.segments[0].size() == 3 && three.segments[0][0].w >= 70);
}

TEST(window_panel_menu_hits_a_row_and_a_segment) {
    const std::vector<panel::MenuRow> rows = {{panel::MenuRow::Kind::Choice, 4, 1, false},
                                              {panel::MenuRow::Kind::Check, 0, 0, false}};
    const panel::MenuGeometry g = panel::menuGeometry(rows, 1.0f);
    const panel::Box s2 = g.segments[0][2];
    panel::MenuHit h = panel::menuHit(g, s2.x + s2.w / 2, s2.y + s2.h / 2);
    CHECK(h.row == 0 && h.segment == 2);
    h = panel::menuHit(g, g.label[0].x + 2, g.rows[0].y + g.rows[0].h / 2);  // its label: the row, no segment
    CHECK(h.row == 0 && h.segment == -1);
    h = panel::menuHit(g, g.rows[1].x + 5, g.rows[1].y + 3);
    CHECK(h.row == 1 && h.segment == -1);
    h = panel::menuHit(g, -3, 5);
    CHECK(h.row == -1);
}

TEST(window_panel_menu_paints_the_chosen_and_the_switch) {
    const std::vector<panel::MenuRow> rows = {{panel::MenuRow::Kind::Choice, 2, 1, false},
                                              {panel::MenuRow::Kind::Check, 0, 0, true}};
    const panel::MenuGeometry g = panel::menuGeometry(rows, 1.0f);
    panel::Picture p;
    panel::paintMenu(p, g, rows, panel::MenuHit{});
    CHECK_EQ(p.width, g.width);
    CHECK_EQ(p.height, g.height);
    if (p.px.size() != size_t(g.width) * size_t(g.height)) return;
    const panel::Box chosen = g.segments[0][1], other = g.segments[0][0];
    const uint32_t c = at(p, chosen.x + chosen.w / 2, chosen.y + 3);
    const uint32_t o = at(p, other.x + other.w / 2, other.y + 3);
    CHECK(blueOf(c) > blueOf(o) + 40);  // the chosen one blue
    const uint32_t box = at(p, g.check[1].x + 2, g.check[1].y + 2);
    CHECK(blueOf(box) > redOf(box) + 40);  // a switch on: its box blue
}

// The menu's rows as the settings are now: its layout, size and opacity chosen, the other windows' switches
TEST(window_panel_menu_rows_follow_the_settings) {
    Config c;
    c.panelLayout = PanelLayout::Column;
    c.panelSize = 125;
    c.panelOpacity = 70;
    c.windowOthersMusic = true;
    c.windowOthersSounds = false;
    c.windowOthersLight = true;
    std::vector<panel::MenuRow> rows = panel::menuRows(c);
    CHECK_EQ(rows.size(), size_t(7));
    if (rows.size() != 7) return;
    CHECK(rows[0].kind == panel::MenuRow::Kind::Choice && rows[0].segments == 3 && rows[0].chosen == 1);
    CHECK(rows[1].kind == panel::MenuRow::Kind::Choice && rows[1].segments == 4 && rows[1].chosen == 2);
    CHECK(rows[2].kind == panel::MenuRow::Kind::Choice && rows[2].segments == 5 && rows[2].chosen == 1);
    CHECK(rows[3].kind == panel::MenuRow::Kind::Check && rows[3].on);
    CHECK(rows[4].kind == panel::MenuRow::Kind::Check && !rows[4].on);
    CHECK(rows[5].kind == panel::MenuRow::Kind::Check && rows[5].on);
    CHECK(rows[6].kind == panel::MenuRow::Kind::Action);
    c.panelSize = 110;  // set in the settings to a size the menu does not offer: none of its chosen
    c.panelLayout = PanelLayout::Icons;
    rows = panel::menuRows(c);
    CHECK_EQ(rows[1].chosen, -1);
    CHECK_EQ(rows[0].chosen, 2);
}

TEST(window_panel_menu_pick_changes_the_setting) {
    Config c;
    panel::MenuChange ch;
    CHECK(panel::menuPick(c, panel::MenuHit{1, 3}, ch));
    CHECK_EQ(c.panelSize, 150);
    CHECK(ch.section == "panel" && ch.key == "size" && ch.value == "150" && !ch.hide);
    CHECK(panel::menuPick(c, panel::MenuHit{0, 1}, ch));
    CHECK(c.panelLayout == PanelLayout::Column);
    CHECK(ch.section == "panel" && ch.key == "layout" && ch.value == "column");
    CHECK(panel::menuPick(c, panel::MenuHit{0, 2}, ch));  // 3.0.3-beta.11: the icons
    CHECK(c.panelLayout == PanelLayout::Icons && ch.key == "layout" && ch.value == "icons");
    CHECK(panel::menuPick(c, panel::MenuHit{2, 0}, ch));
    CHECK_EQ(c.panelOpacity, 60);
    CHECK(ch.key == "opacity" && ch.value == "60");
    const bool sounds = c.windowOthersSounds;
    CHECK(panel::menuPick(c, panel::MenuHit{4, -1}, ch));
    CHECK(c.windowOthersSounds == !sounds);
    CHECK(ch.section == "windows" && ch.key == "others_sounds" && ch.value == (sounds ? "0" : "1"));
    const bool music = c.windowOthersMusic, light = c.windowOthersLight;
    CHECK(panel::menuPick(c, panel::MenuHit{3, -1}, ch) && c.windowOthersMusic == !music && ch.key == "others_music");
    CHECK(panel::menuPick(c, panel::MenuHit{5, -1}, ch) && c.windowOthersLight == !light && ch.key == "others_light");
    CHECK(panel::menuPick(c, panel::MenuHit{6, -1}, ch) && ch.hide);
    CHECK(!panel::menuPick(c, panel::MenuHit{1, -1}, ch));  // a choice's label: nothing
    CHECK(!panel::menuPick(c, panel::MenuHit{-1, -1}, ch));
    CHECK(!panel::menuPick(c, panel::MenuHit{9, 0}, ch));
    CHECK(!panel::menuPick(c, panel::MenuHit{1, 7}, ch));
}

TEST(window_panel_menu_says_its_rows) {
    const std::vector<panel::MenuText> t = panel::menuTexts(settings::Lang::Uk);
    CHECK_EQ(t.size(), size_t(7));
    if (t.size() != 7) return;
    CHECK(t[0].label == "Вигляд" && t[0].segments.size() == 3 && t[0].segments[1] == "Стовпчиком");
    if (t[0].segments.size() == 3) CHECK(t[0].segments[2] == "Іконки");
    CHECK(t[1].label == "Розмір" && t[1].segments.size() == 4 && t[1].segments[0] == "75 %");
    CHECK(t[2].label == "Непрозорість" && t[2].segments.size() == 5 && t[2].segments[4] == "100 %");
    CHECK(t[3].label == "Музика в інших вікнах гри" && t[3].segments.empty());
    CHECK(t[4].label == "Звуки в інших вікнах гри");
    CHECK(t[5].label == "Інші вікна гри — легкі");
    CHECK(t[6].label.find("Ctrl+Alt+0") != std::string::npos);
}

// 3.0.3-beta.11 (the user's report R-45N3WS: «мышка темная на менюшке переключения окон» on prontera): uaRO.exe's
// cursor (FUN_004f60e0) is an interface face of the colour 0xFAFFFFFF, and the screen's additive faces (FUN_0040f9e0)
// come after it - often more than the 8 draws looked at: found by its colour however many draws follow it
TEST(window_panel_finds_the_games_cursor_by_its_colour) {
    FrameRecord r;
    quad(r, 0, 0, 800, 600, true);                   // a window of the game
    quad(r, 400, 300, 432, 332, true, 0xFAFFFFFFu);  // a sprite at the mouse (FUN_0055bff0)
    quad(r, 401, 301, 425, 325, true, 0xFAFFFFFFu);  // the cursor
    for (int i = 0; i < 30; ++i) quad(r, 401, 301, 409, 309, true, 0xFFFFFFFFu);  // the screen's faces after it
    CHECK_EQ(panel::cursorDraw(r, 402, 303), 2);
    CHECK_EQ(panel::cursorDraw(r, 700, 500), -1);
    FrameRecord world;  // of that colour but not the interface's: not it
    quad(world, 401, 301, 425, 325, false, 0xFAFFFFFFu);
    CHECK_EQ(panel::cursorDraw(world, 402, 303), -1);
    // 3.0.3-beta.12: FUN_004f5a90 puts a sprite's quad half a pixel right and down of its point - the arrow's tip at
    // its picture's corner leaves the mouse's point just outside it
    FrameRecord tip;
    quad(tip, 0, 0, 800, 600, true);
    quad(tip, 402.5f, 303.5f, 426.5f, 327.5f, true, 0xFAFFFFFFu);
    CHECK_EQ(panel::cursorDraw(tip, 402, 303), 1);
}

// 3.0.3-beta.11 (the user: «супер минималистичный режим в котором типа иконки маленькие были этих окон, чтобы не
// занимало много места»): small square tiles with the windows' numbers in a row, a grip at its start - no title, no
// cross, no names (the name under the mouse in a tip of its own)
TEST(window_panel_icons_are_small) {
    const panel::Geometry icons = panel::geometry(3, panel::Layout::Icons, 1.0f, 1920);
    const panel::Geometry row = panel::geometry(3, panel::Layout::Row, 1.0f, 1920);
    CHECK_EQ(icons.cards.size(), size_t(3));
    CHECK(icons.width * icons.height * 4 < row.width * row.height);
    CHECK(icons.height <= 40);
    if (icons.cards.size() != 3) return;
    const panel::Box& t = icons.cards[0].plate;
    CHECK(std::fabs(t.w - t.h) < 0.5f);
    CHECK(icons.cards[1].plate.x >= t.x + t.w);
    CHECK(icons.cards[0].name.w == 0 && icons.cards[0].levels.w == 0);
    CHECK(icons.close.w == 0 && icons.layout.w == 0 && icons.title.w == 0);
    const panel::Box n = icons.cards[0].number;  // the number's place: in the tile, over its bars
    CHECK(n.w > 0 && n.x >= t.x && n.x + n.w <= t.x + t.w + 0.5f && n.y + n.h <= icons.cards[0].hp.y + 0.5f);
    CHECK(icons.cards[0].hp.y + icons.cards[0].hp.h <= t.y + t.h);
    CHECK(panel::hitTest(icons, 3, float(icons.height) / 2).part == panel::Part::Top);  // its grip drags it
    const panel::Hit h = panel::hitTest(icons, icons.cards[2].plate.x + 5, icons.cards[2].plate.y + 5);
    CHECK(h.part == panel::Part::Card && h.card == 2);
    const panel::Geometry big = panel::geometry(3, panel::Layout::Icons, 2.0f, 1920);
    CHECK(std::abs(big.width - 2 * icons.width) <= 1);
    // the cards' layouts keep the number in the badge
    const panel::CardBoxes& c = row.cards[0];
    CHECK(std::fabs(c.number.x - (c.badgeX - c.badgeR)) < 0.01f && std::fabs(c.number.w - 2 * c.badgeR) < 0.01f);
}

TEST(window_panel_icons_paint_the_current_one) {
    const panel::Geometry g = panel::geometry(2, panel::Layout::Icons, 1.0f, 1920);
    std::vector<panel::Card> cards(2);
    cards[1].current = true;
    cards[0].hp = cards[1].hp = 80;
    panel::Picture p;
    panel::paint(p, g, cards, panel::Part::None, panel::Layout::Icons);
    CHECK_EQ(p.width, g.width);
    CHECK_EQ(p.height, g.height);
    if (g.cards.size() != 2 || p.px.size() != size_t(g.width) * size_t(g.height)) return;
    const panel::Box a = g.cards[0].plate, b = g.cards[1].plate;
    const uint32_t other = at(p, a.x + a.w / 2, a.y + 3), current = at(p, b.x + b.w / 2, b.y + 3);
    CHECK(blueOf(current) > blueOf(other) + 30);
    const panel::Box hp = g.cards[0].hp;
    const uint32_t bar = at(p, hp.x + 2, hp.y + hp.h / 2);
    CHECK(greenOf(bar) > redOf(bar) + 40);
}

// The name of the icon under the mouse: above it, centred on it; no room above (the panel at the top) - under it;
// inside the window whatever it takes
TEST(window_panel_icons_tip_finds_room) {
    panel::Point p = panel::tipPlace(panel::Box{500, 600, 30, 30}, 80, 24, 1920, 1080, 1.0f);
    CHECK(p.y + 24 <= 600 && p.y >= 600 - 24 - 12);
    CHECK_EQ(p.x, 500 + 15 - 40);
    p = panel::tipPlace(panel::Box{500, 4, 30, 30}, 80, 24, 1920, 1080, 1.0f);
    CHECK(p.y >= 34);
    p = panel::tipPlace(panel::Box{2, 600, 30, 30}, 80, 24, 1920, 1080, 1.0f);
    CHECK_EQ(p.x, 0);
    p = panel::tipPlace(panel::Box{1900, 600, 30, 30}, 80, 24, 1920, 1080, 1.0f);
    CHECK_EQ(p.x, 1920 - 80);
    panel::Picture tip;
    panel::paintTip(tip, 80, 24, 1.0f);
    CHECK(tip.width == 80 && tip.height == 24 && tip.px.size() == size_t(80 * 24));
    if (tip.px.size() == size_t(80 * 24)) CHECK(alphaOf(at(tip, 40, 12)) > 200 && alphaOf(at(tip, 0, 0)) < 128);
}

// 3.0.3-beta.12 (the user's report R-KVTEMT: «всеравно курсор не видно»): the game's EndScene sends its scene to
// the GPU before the present (Device7::EndScene flushes the record), the game's cursor with it - the panel goes into
// the frame there, once a frame; at the present only into a picture drawn without a scene
TEST(window_panel_goes_into_the_frame_at_the_games_end_scene) {
    panel::FrameSlot slot;
    CHECK(slot.endScene(true));    // the game's scene: the panel into it
    CHECK(!slot.endScene(true));   // a second scene in the same frame: not again
    CHECK(!slot.present(true));    // drawn after the scene too: it is in the frame already
    CHECK(!slot.endScene(false));  // the next frame: a scene with no draw of the game's - nothing to go over
    CHECK(slot.present(true));     // a picture drawn without a scene: at the present
    CHECK(!slot.present(false));   // the same picture shown again: no panel over it again
    CHECK(slot.endScene(true));
}

// 3.0.4-beta.4 (the user's idea 8): a helper window's character fallen, whispered to, hit - marked on its card
TEST(window_panel_marks_a_card) {
    for (panel::Layout layout : {panel::Layout::Row, panel::Layout::Icons}) {
        const panel::Geometry g = panel::geometry(3, layout, 1.0f, 1024);
        std::vector<panel::Card> cards(3);
        cards[0].died = true;
        cards[1].whisper = true;
        cards[2].hit = true;
        panel::Picture pic;
        panel::paint(pic, g, cards, panel::Part::None, layout);
        if (pic.px.size() != size_t(g.width) * size_t(g.height)) {
            CHECK(false);
            continue;
        }
        const panel::Box& dead = g.cards[0].plate;
        const uint32_t edge = at(pic, dead.x + dead.w / 2, dead.y + 0.6f);  // its edge red
        CHECK(redOf(edge) > greenOf(edge) + 60 && redOf(edge) > blueOf(edge) + 60);
        const panel::Box& told = g.cards[1].plate;
        const uint32_t dot = at(pic, told.x + told.w - 5.0f, told.y + 5.0f);  // a blue dot in its corner
        CHECK(blueOf(dot) > redOf(dot) + 60);
        const panel::Box& hurt = g.cards[2].plate;
        const uint32_t orange = at(pic, hurt.x + hurt.w / 2, hurt.y + 0.6f);
        CHECK(redOf(orange) > blueOf(orange) + 80 && greenOf(orange) > blueOf(orange) + 30);
    }
}

namespace {

// The rule a rounded box was drawn by before 3.0.11-beta.25, a pixel at a time: its distance, filled by what is
// inside, the ring `edge` wide inside its border
float slowRoundBox(float px, float py, const panel::Box& b, float r) {
    r = std::min(r, std::min(b.w, b.h) / 2);
    const float qx = std::fabs(px - (b.x + b.w / 2)) - (b.w / 2 - r);
    const float qy = std::fabs(py - (b.y + b.h / 2)) - (b.h / 2 - r);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

float slowCover(float d) { return std::clamp(0.5f - d, 0.0f, 1.0f); }

void slowRoundRect(panel::Picture& p, const panel::Box& b, float r, uint32_t fill, float fillA, uint32_t edgeRgb,
                   float edgeA, float edge) {
    const int x0 = std::max(0, int(std::floor(b.x)) - 1), y0 = std::max(0, int(std::floor(b.y)) - 1);
    const int x1 = std::min(p.width, int(std::ceil(b.x + b.w)) + 1);
    const int y1 = std::min(p.height, int(std::ceil(b.y + b.h)) + 1);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const float d = slowRoundBox(float(x) + 0.5f, float(y) + 0.5f, b, r);
            const float inside = slowCover(d);
            if (inside <= 0.0f) continue;
            uint32_t& px = p.px[size_t(y) * size_t(p.width) + size_t(x)];
            panel::over(px, fill, fillA * inside);
            if (edgeA > 0.0f) panel::over(px, edgeRgb, edgeA * (inside - slowCover(d + edge)));
        }
}

}  // namespace

// 3.0.11-beta.25 (R-77TCM7: «когда по меню вожу, лагат курсор и фпс падает» -
// «Довідник» painted in 290 ms at 4K): the rows' inside of a rounded box filled as one run changes no pixel
// of the rule it was drawn by
TEST(round_rect_runs_match_the_distance_rule) {
    uint32_t seed = 12345;
    const auto rnd = [&](float lo, float hi) {
        seed = seed * 1664525u + 1013904223u;
        return lo + (hi - lo) * float(seed >> 8) / float(1u << 24);
    };
    int differing = 0;
    for (int n = 0; n < 400; ++n) {
        panel::Picture fast, slow;
        fast.width = slow.width = 96;
        fast.height = slow.height = 80;
        fast.px.assign(size_t(96) * 80, 0xFF102030u + uint32_t(n));
        slow.px = fast.px;
        const panel::Box b{rnd(-12, 60), rnd(-12, 50), n % 7 == 0 ? rnd(0, 2) : rnd(0, 110), rnd(0, 90)};
        const float r = n % 5 == 0 ? 0.0f : rnd(0, 34);
        const float fillA = n % 4 == 0 ? 1.0f : n % 4 == 1 ? 0.0f : rnd(0, 1.2f);
        const float edgeA = n % 3 == 0 ? 0.0f : rnd(0, 1);
        const float edge = n % 6 == 0 ? 0.0f : rnd(0.5f, 5);
        const uint32_t fill = 0xFFA0B0C0u - uint32_t(n) * 0x010203u, edgeRgb = 0x00405060u + uint32_t(n);
        panel::roundRect(fast, b, r, fill, fillA, edgeRgb, edgeA, edge);
        slowRoundRect(slow, b, r, fill, fillA, edgeRgb, edgeA, edge);
        if (fast.px != slow.px) ++differing;
    }
    CHECK_EQ(differing, 0);
}

TEST(round_rect_whole_picture_box_matches_the_distance_rule) {  // the window's frame: no fill, a ring inside its border
    panel::Picture fast, slow;
    fast.width = slow.width = 300;
    fast.height = slow.height = 200;
    fast.px.assign(size_t(300) * 200, 0xFF181B57u);
    slow.px = fast.px;
    panel::roundRect(fast, panel::Box{0, 0, 300, 200}, 0, 0, 0.0f, 0x2A2F3Au, 1.0f, 2.0f);
    slowRoundRect(slow, panel::Box{0, 0, 300, 200}, 0, 0, 0.0f, 0x2A2F3Au, 1.0f, 2.0f);
    CHECK(fast.px == slow.px);
    panel::roundRect(fast, panel::Box{10.5f, 7.25f, 250.3f, 120.7f}, 16, 0x223344u, 1.0f, 0x5566FFu, 1.0f, 2.0f);
    slowRoundRect(slow, panel::Box{10.5f, 7.25f, 250.3f, 120.7f}, 16, 0x223344u, 1.0f, 0x5566FFu, 1.0f, 2.0f);
    CHECK(fast.px == slow.px);
}

// 3.0.11-beta.57 (R-T942DZ): a part's picture made blank for its next painting in the memory it had - see-through, its
// clip the whole picture again; nothing to paint: false
TEST(window_panel_blank_keeps_its_memory) {
    panel::Picture p;
    CHECK(panel::blank(p, 300, 200));
    CHECK(p.width == 300 && p.height == 200 && p.px.size() == 60000u);
    p.px[123] = 0xFF112233u;
    p.clipX0 = 10;
    p.clipY1 = 50;
    const uint32_t* memory = p.px.data();
    CHECK(panel::blank(p, 300, 200));
    CHECK(p.px.data() == memory);  // not taken anew
    CHECK(p.px[123] == 0u && p.clipX0 == 0 && p.clipY0 == 0 && p.clipX1 == (1 << 30) && p.clipY1 == (1 << 30));
    CHECK(panel::blank(p, 200, 100));  // smaller: the same memory still
    CHECK(p.px.data() == memory && p.px.size() == 20000u);
    CHECK(!panel::blank(p, 0, 100));
    CHECK(p.width == 0 && p.px.empty());
    CHECK(!panel::blank(p, 100, -5));
    CHECK(p.height == 0 && p.px.empty());
}
