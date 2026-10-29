// 3.0.11-beta.1: the new interface's minimap prototype (stage 1, the owner's «давай» 2026-10-28) - the player's cell by
// uaRO.exe's own formula, the panel in the old one's corner, the view round the player, the picture
#include <vector>

#include "core/minimap_proto.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(minimap_proto_cell_by_the_games_formula) {
    // FUN_0055c220: x / cell + w / 2 and z / cell + h / 2, each cut toward zero (__ftol); prontera 312 x 392, cell 5
    minimap::Cell c;
    CHECK(minimap::cellOf(0.0f, 0.0f, 312, 392, 5, c));
    CHECK(c.x == 156 && c.y == 196);
    // the cell (136, 62) and a little into it: x = (136 - 156) * 5 + 1, z = (62 - 196) * 5 + 1
    CHECK(minimap::cellOf(-99.0f, -669.0f, 312, 392, 5, c));
    CHECK(c.x == 136 && c.y == 62);
    CHECK(minimap::cellOf(-99.0f, -669.0f, 301, 392, 5, c));  // an odd width: its half cut as the game cuts it
    CHECK(c.x == 130);
    CHECK(!minimap::cellOf(0.0f, 0.0f, 0, 392, 5, c));       // no map
    CHECK(!minimap::cellOf(0.0f, 0.0f, 312, 392, 0, c));     // no cell size
    CHECK(!minimap::cellOf(1e6f, 0.0f, 312, 392, 5, c));     // past the map: not a place
}

TEST(minimap_proto_sits_in_the_old_ones_corner) {
    // 3.0.11-beta.31: 220 px a side at scale 1, 10 from the edges; the bar under it; the way's bar and the details
    // only when there
    const minimap::Layout L = minimap::layout(1920, 1080, 1.5f, false, 0);
    CHECK(L.w == 330 && L.x + L.w == 1920 - 15 && L.y == 15);
    CHECK(L.map.w == 330 && L.caption.y > L.map.h && L.route.w == 0 && L.list.w == 0);
    CHECK(L.h == int(L.caption.y + L.caption.h));
    CHECK(minimap::partAt(L, 5, 5) == minimap::Part::Map);
    CHECK(minimap::partAt(L, L.zoomIn.x + 2, L.zoomIn.y + 2) == minimap::Part::ZoomIn);
    CHECK(minimap::partAt(L, L.expand.x + 2, L.expand.y + 2) == minimap::Part::Expand);
    CHECK(minimap::partAt(L, L.fold.x + 2, L.fold.y + 2) == minimap::Part::Fold);
    CHECK(minimap::partAt(L, 10, L.caption.y + 5) == minimap::Part::Caption);
    CHECK(minimap::partAt(L, 10, L.map.h + 2) == minimap::Part::None);  // the gap between
    const minimap::Layout all = minimap::layout(1920, 1080, 1.0f, true, 3);
    CHECK(all.route.w > 0 && all.list.w > 0 && all.list.y > all.route.y + all.route.h);
    CHECK(minimap::partAt(all, all.stop.x + 2, all.stop.y + 2) == minimap::Part::Stop);
    CHECK_EQ(minimap::lineAt(all, all.list.y + 7 + 19 + 3), 1);
    CHECK_EQ(minimap::lineAt(all, all.list.y + 1), -1);
    CHECK(minimap::cellsAt(minimap::kZoomFirst, {400, 300}) == 100 && minimap::cellsAt(0, {400, 300}) == 40);
    CHECK(minimap::cellsAt(minimap::kZoomSteps - 1, {400, 300}) == 400 && minimap::cellsAt(99, {50, 80}) == 80);
}

TEST(minimap_proto_view_keeps_inside_the_map) {
    const minimap::MapSize map{400, 400};
    minimap::View v = minimap::view(map, minimap::Cell{200, 200}, 100, 200);  // the middle: centred
    float px = 0, py = 0;
    minimap::cellToPanel(v, map, minimap::Cell{200, 200}, px, py);
    CHECK(px > 99 && px < 101 && py > 99 && py < 101);
    minimap::cellToPanel(v, map, minimap::Cell{210, 210}, px, py);  // north-east: right and up
    CHECK(px > 100 && py < 100);
    v = minimap::view(map, minimap::Cell{10, 390}, 100, 200);  // the north-west corner: the map's edge stays
    minimap::cellToPanel(v, map, minimap::Cell{0, 399}, px, py);
    CHECK(px >= 0 && px < 3 && py >= 0 && py < 3);
    minimap::cellToPanel(v, map, minimap::Cell{10, 390}, px, py);  // the player off the centre
    CHECK(px > 15 && px < 25 && py > 15 && py < 25);
}

TEST(minimap_proto_paints_the_map_round_the_player) {
    const minimap::Layout L = minimap::layout(1280, 720, 1.0f, false, 0);
    const minimap::MapSize map{100, 100};
    // a 2x2 picture of the map: magenta (the game's see-through) top left, green elsewhere
    const std::vector<uint32_t> argb{0xFFFF00FF, 0xFF00FF00, 0xFF00FF00, 0xFF00FF00};
    minimap::Scene sc;
    sc.argb = &argb;
    sc.imgW = sc.imgH = 2;
    sc.map = map;
    sc.player = minimap::Cell{50, 50};
    panel::Picture pic;
    minimap::paint(pic, L, sc, minimap::Texts{});
    CHECK(pic.width == L.w && pic.height == L.h);
    const auto at = [&](float x, float y) { return pic.px[size_t(y) * size_t(pic.width) + size_t(x)]; };
    const uint32_t keyed = at(L.map.w / 4, L.map.h / 4), green = at(L.map.w * 3 / 4, L.map.h * 3 / 4);
    CHECK((green & 0x00FF00) > 0x00A000 && (green >> 24) == 0xFF);
    CHECK((keyed & 0xFF00FF) != 0xFF00FF);  // no magenta left: the ground shows
    const uint32_t mark = at(L.map.w / 2, L.map.h / 2);  // the player's dot in the middle (no camera known), white
    CHECK((mark & 0xFF) > 0xE0 && ((mark >> 8) & 0xFF) > 0xE0);
    CHECK((at(2, 2) >> 24) < 0x80);  // the square's rounded corner see-through (its gold corner beside it)
    CHECK(((at(0, 0) >> 16) & 0xFF) > 0xC0);
    CHECK((at(1, L.map.h + 2) >> 24) == 0);  // the gap under the square
    sc.lookY = 1;  // the camera looking north: the arrow's blue a little above the middle
    minimap::paint(pic, L, sc, minimap::Texts{});
    const uint32_t arrow = at(L.map.w / 2, L.map.h / 2 - 4);
    CHECK((arrow & 0xFF) > 0xC0 && ((arrow >> 16) & 0xFF) < 0xA0);
}

TEST(minimap_proto_ctrl_tab_goes_round_as_the_games) {
    // 3.0.11-beta.4 (the owner: «на Ctrl+Tab ничего не происходит»): the game's own round - opaque, see-through, hidden
    CHECK(minimap::next(minimap::Look::Opaque) == minimap::Look::Faded);
    CHECK(minimap::next(minimap::Look::Faded) == minimap::Look::Hidden);
    CHECK(minimap::next(minimap::Look::Hidden) == minimap::Look::Opaque);
    CHECK(minimap::opacity(minimap::Look::Opaque) == 1.0f);
    CHECK(minimap::opacity(minimap::Look::Faded) > 0.5f && minimap::opacity(minimap::Look::Faded) < 0.8f);
    CHECK(minimap::opacity(minimap::Look::Hidden) == 0.0f);
}

TEST(minimap_proto_makes_room_for_the_monsters_under_it) {
    // 3.0.11-beta.31: lines of 19 at scale 1 in the details' box, folded - none
    const minimap::Layout L = minimap::layout(1920, 1080, 1.0f, false, 3);
    CHECK(L.lineH == 19 && L.list.h == 3 * 19 + 14);
    CHECK(minimap::partAt(L, 5, L.list.y + L.list.h - 1) == minimap::Part::List);  // the list keeps clicks
    CHECK(minimap::partAt(L, 5, L.list.y + L.list.h + 1) == minimap::Part::None);
    panel::Picture pic;
    minimap::Texts t;
    t.list = {{"Poring", "100"}, {"Lunatic", "40"}, {"Drops", "10"}};
    minimap::paint(pic, L, minimap::Scene{}, t);
    CHECK_EQ(pic.height, L.h);
    bool poring = false, count = false;
    for (const minimap::Words& w : minimap::words(L, minimap::Scene{}, t)) {
        poring = poring || (w.text == "Poring" && w.box.y >= L.list.y);
        count = count || (w.text == "100" && w.align == minimap::Align::Right);
    }
    CHECK(poring && count);
    t.folded = true;
    bool closed = false;
    for (const minimap::Words& w : minimap::words(minimap::layout(1920, 1080, 1.0f, false, 0), minimap::Scene{}, t))
        closed = closed || w.text == "▸";
    CHECK(closed);
}

TEST(minimap_proto_marks_the_npcs_and_the_warps) {
    const minimap::Layout L = minimap::layout(1280, 720, 1.0f, false, 0);  // 220 px for 100 cells
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    sc.player = minimap::Cell{50, 50};
    sc.marks = {{minimap::Cell{30, 70}, 0xD6336C, minimap::Glyph::Kafra, "Kafra Employee"},  // up left
                {minimap::Cell{80, 20}, 0, minimap::Glyph::Warp, "→ Prontera"},          // down right
                {minimap::Cell{300, 300}, 0x00FF00, minimap::Glyph::Dot, "far"}};        // off the view
    panel::Picture pic;
    minimap::paint(pic, L, sc, minimap::Texts{});
    const auto at = [&](float x, float y) { return pic.px[size_t(y) * size_t(pic.width) + size_t(x)]; };
    const minimap::View v = minimap::view(sc.map, sc.player, 100, int(L.map.w));
    float px = 0, py = 0;
    minimap::cellToPanel(v, sc.map, minimap::Cell{30, 70}, px, py);
    const uint32_t kafra = at(px - 3, py - 3);
    CHECK(((kafra >> 16) & 0xFF) > 0xC0 && ((kafra >> 8) & 0xFF) < 0x90);  // the pink «K» square
    bool letter = false;
    for (const minimap::Words& w : minimap::words(L, sc, minimap::Texts{})) letter = letter || w.text == "K";
    CHECK(letter);
    minimap::cellToPanel(v, sc.map, minimap::Cell{80, 20}, px, py);
    const uint32_t warp = at(px, py);
    CHECK((warp & 0xFF) > 0xE0 && ((warp >> 8) & 0xFF) > 0xE0);  // the portal's white heart
    CHECK(minimap::markAt(L, sc, px + 1, py - 1) == 1);  // the pointer by it names it
    sc.hoverMark = 1;
    bool named = false;
    for (const minimap::Words& w : minimap::words(L, sc, minimap::Texts{})) named = named || w.text == "→ Prontera";
    CHECK(named);
    CHECK(minimap::markAt(L, sc, L.map.w / 2, L.map.h / 2) == -1);  // the player's middle: none near
    CHECK(minimap::markAt(L, sc, 80, L.map.h + 5) == -1);            // the bar: not the map
}

// 3.0.11-beta.31: the way in gold along its cells, the pin on its next move, the zoom, the big map
TEST(minimap_proto_way_zoom_and_big_map) {
    const minimap::Layout L = minimap::layout(1280, 720, 1.0f, true, 0);
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    sc.player = minimap::Cell{50, 50};
    sc.path = {minimap::Cell{50, 50}, minimap::Cell{50, 60}, minimap::Cell{60, 60}};  // up, then right
    sc.pin = true;
    sc.pinCell = minimap::Cell{60, 60};
    panel::Picture pic;
    minimap::paint(pic, L, sc, minimap::Texts{});
    const auto at = [&](float x, float y) { return pic.px[size_t(y) * size_t(pic.width) + size_t(x)]; };
    const minimap::View v = minimap::view(sc.map, sc.player, 100, int(L.map.w));
    float x = 0, y = 0;
    minimap::cellToPanel(v, sc.map, minimap::Cell{50, 56}, x, y);  // on the way's first leg: gold
    const uint32_t gold = at(x, y);
    CHECK(((gold >> 16) & 0xFF) > 0xC0 && ((gold >> 8) & 0xFF) > 0x90 && (gold & 0xFF) < 0x90);
    minimap::cellToPanel(v, sc.map, minimap::Cell{56, 54}, x, y);  // off it: the ground
    CHECK(((at(x, y) >> 16) & 0xFF) < 0x40);
    sc.cells = minimap::cellsAt(0, sc.map);  // nearer: the same cells further from the middle
    float near = 0, farX = 0;
    minimap::cellToPanel(minimap::view(sc.map, sc.player, sc.cells, int(L.map.w)), sc.map, minimap::Cell{60, 50}, near,
                         y);
    minimap::cellToPanel(v, sc.map, minimap::Cell{60, 50}, farX, y);
    CHECK(near > farX);
    // the big map: the whole map fitted in the screen's middle, its cross, its marks' words
    const minimap::Big b = minimap::bigLayout(1920, 1080, 1.0f, sc.map);
    CHECK(b.x > 0 && b.y > 0 && b.x + b.w < 1920 && b.y + b.h < 1080 && b.map.w == b.map.h);
    CHECK(minimap::bigPartAt(b, b.close.x + 2, b.close.y + 2) == minimap::BigPart::Close);
    CHECK(minimap::bigPartAt(b, b.map.x + 5, b.map.y + 5) == minimap::BigPart::Map);
    CHECK(minimap::bigPartAt(b, -1, 5) == minimap::BigPart::None);
    sc.marks = {{minimap::Cell{20, 80}, 0, minimap::Glyph::Warp, "→ Izlude"}};
    panel::Picture base, big;
    minimap::paintBigMap(base, b, sc);
    minimap::paintBig(big, base, b, sc);
    CHECK(big.width == b.w && big.height == b.h && base.width == int(b.map.w));
    bool label = false, title = false;
    for (const minimap::Words& w : minimap::bigWords(b, sc, "Prontera Field · prt_fild08", "50, 50")) {
        label = label || (w.text == "→ Izlude" && w.box.x >= b.map.x);
        title = title || w.text == "Prontera Field · prt_fild08";
    }
    CHECK(label && title);
    float wx = 0, wy = 0;
    minimap::View whole;
    whole.cellPx = b.map.w / 100;
    whole.top = 100;
    minimap::cellToPanel(whole, sc.map, minimap::Cell{20, 80}, wx, wy);
    CHECK_EQ(minimap::bigMarkAt(b, sc, b.map.x + wx, b.map.y + wy), 0);
}
