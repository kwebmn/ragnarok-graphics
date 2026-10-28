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
    const minimap::Geometry g = minimap::geometry(1920, 1080, 1.5f);
    CHECK(g.size == 240 && g.x + g.size == 1920 - 12 && g.y == 12);  // 160 px a side at scale 1, 8 px from the edges
    CHECK(g.textH > 0);
    CHECK(minimap::inside(g, float(g.x + 5), float(g.y + 5)));
    CHECK(minimap::inside(g, float(g.x + g.size - 1), float(g.y + g.size + g.textH - 1)));  // its line too
    CHECK(!minimap::inside(g, float(g.x - 1), float(g.y + 5)));
    CHECK(!minimap::inside(g, float(g.x + 5), float(g.y + g.size + g.textH + 1)));
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
    const minimap::Geometry g = minimap::geometry(1280, 720, 1.0f);
    const minimap::MapSize map{100, 100};
    // a 2x2 picture of the map: magenta (the game's see-through) top left, green elsewhere
    const std::vector<uint32_t> argb{0xFFFF00FF, 0xFF00FF00, 0xFF00FF00, 0xFF00FF00};
    panel::Picture pic;
    minimap::paint(pic, g, argb, 2, 2, map, minimap::Cell{50, 50}, 100);
    CHECK(pic.width == g.size && pic.height == g.size + g.textH);
    const auto at = [&](int x, int y) { return pic.px[size_t(y) * size_t(pic.width) + size_t(x)]; };
    const uint32_t keyed = at(g.size / 4, g.size / 4), green = at(g.size * 3 / 4, g.size * 3 / 4);
    CHECK((green & 0x00FF00) > 0x00A000 && (green >> 24) == 0xFF);
    CHECK((keyed & 0xFF00FF) != 0xFF00FF);  // no magenta left: the ground shows
    const uint32_t mark = at(g.size / 2, g.size / 2);  // the player's mark in the middle, white
    CHECK((mark & 0xFF) > 0xE0 && ((mark >> 8) & 0xFF) > 0xE0);
    panel::Picture none;  // no picture of the map: the ground and the mark
    minimap::paint(none, g, {}, 0, 0, map, minimap::Cell{50, 50}, 100);
    CHECK(none.width == g.size && none.px.size() == size_t(none.width) * size_t(none.height));
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
