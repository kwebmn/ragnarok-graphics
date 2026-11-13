// 3.0.11-beta.1: the new interface's minimap prototype (stage 1, the owner's «давай» 2026-10-28) - the player's cell by
// uaRO.exe's own formula, the panel in the old one's corner, the view round the player, the picture
#include <algorithm>
#include <cmath>
#include <vector>

#include "core/minimap_proto.hpp"
#include "core/walk_grid.hpp"
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

// 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment, [interface] minimap_reach): the cells
// not reached on foot from the player dimmed on the map's picture, those reached as they are; 3.0.11-beta.126: only
// the walked-on cells of another part (walkgrid::kElsewhere) - a wall or the map's ground stays as the picture has it
TEST(minimap_proto_dims_what_cannot_be_reached) {
    const std::vector<uint32_t> argb(64 * 64, 0xFFE0E0E0);  // a light map
    minimap::Scene sc;
    sc.argb = &argb;
    sc.imgW = sc.imgH = 64;
    sc.map = minimap::MapSize{40, 40};
    std::vector<uint8_t> reach(40 * 40, walkgrid::kElsewhere);
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 20; ++x) reach[size_t(y * 40 + x)] = 1;  // the west half reached
    const minimap::Big b = minimap::bigLayout(1920, 1080, 1.0f, sc.map);
    panel::Picture plain, dimmed;
    minimap::paintBigMap(plain, b, sc);
    sc.reach = &reach;
    minimap::paintBigMap(dimmed, b, sc);
    const auto at = [&](const panel::Picture& p, float fx, float fy) {
        return p.px[size_t(float(p.height) * fy) * size_t(p.width) + size_t(float(p.width) * fx)];
    };
    CHECK_EQ(at(dimmed, 0.25f, 0.5f), at(plain, 0.25f, 0.5f));  // the west: as it was
    const uint32_t east = at(dimmed, 0.75f, 0.5f), was = at(plain, 0.75f, 0.5f);
    CHECK((east >> 24) == 0xFF && ((east >> 16) & 0xFF) < ((was >> 16) & 0xFF) / 2 + 8);  // the east: dimmed
    CHECK(((east >> 16) & 0xFF) > 0x30);  // still seen
    std::vector<uint8_t> wrong(7, 1);  // a mask of another map's size: none
    sc.reach = &wrong;
    minimap::paintBigMap(dimmed, b, sc);
    CHECK(dimmed.px == plain.px);
}

// 3.0.11-beta.126 (R-4ZHJ3R «посмотри на карту», R-4T7N4D «она какаято не красивая»: c_tower2's map one flat grey with
// minimap_reach on - its light ground, 168, dimmed to 40 % is its dark corridors' 65): the walls and the ground kept
TEST(minimap_proto_reach_keeps_the_walls_as_drawn) {
    std::vector<uint32_t> argb(40 * 40, 0xFFA8A8A8);  // the dungeon's light ground (not walked on)
    for (int y = 0; y < 40; ++y)
        for (int x = 18; x < 22; ++x) argb[size_t(y * 40 + x)] = 0xFF414141;  // a dark corridor down the middle
    minimap::Scene sc;
    sc.argb = &argb;
    sc.imgW = sc.imgH = 40;
    sc.map = minimap::MapSize{40, 40};
    std::vector<uint8_t> reach(40 * 40, 0);  // walls
    for (int y = 0; y < 40; ++y)
        for (int x = 18; x < 22; ++x) reach[size_t(y * 40 + x)] = 1;  // the corridor reached
    const minimap::Big b = minimap::bigLayout(1920, 1080, 1.0f, sc.map);
    panel::Picture plain, lit;
    minimap::paintBigMap(plain, b, sc);
    sc.reach = &reach;
    minimap::paintBigMap(lit, b, sc);
    const auto at = [&](const panel::Picture& p, float fx, float fy) {
        return p.px[size_t(float(p.height) * fy) * size_t(p.width) + size_t(float(p.width) * fx)];
    };
    CHECK_EQ(at(lit, 0.2f, 0.5f), at(plain, 0.2f, 0.5f));  // the ground as drawn: still lighter than the corridor
    CHECK_EQ(at(lit, 0.5f, 0.5f), at(plain, 0.5f, 0.5f));
    CHECK(((at(lit, 0.2f, 0.5f) >> 16) & 0xFF) > ((at(lit, 0.5f, 0.5f) >> 16) & 0xFF) + 60);
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

// 3.0.11-beta.31: the way in gold along its cells, the pin on its next move, the zoom, the big map
TEST(minimap_proto_way_zoom_and_big_map) {
    const minimap::Layout L = minimap::layout(1280, 720, 1.0f, true, 0);
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    sc.player = minimap::Cell{50, 50};
    sc.path = {minimap::Cell{50, 50}, minimap::Cell{50, 60}, minimap::Cell{60, 60}};
    // up, then right

    sc.pin = true;
    sc.pinCell = minimap::Cell{60, 60};
    ;
    const minimap::View v = minimap::view(sc.map, sc.player, 100, int(L.map.w));
    float x = 0, y = 0;
    minimap::cellToPanel(v, sc.map, minimap::Cell{50, 56}, x, y);
    // on the way's first leg: gold

    minimap::cellToPanel(v, sc.map, minimap::Cell{56, 54}, x, y);
    // off it: the ground

    sc.cells = minimap::cellsAt(0, sc.map);
    // nearer: the same cells further from the middle

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

// 3.0.11-beta.72 (the owner: «сами картинки карт маленькие и не аккуратные», «розовые полосы по краям»): the game's
// magenta and the pinkish pixels an upscale smudges round it are see-through; the map's picture premultiplied and
// halved level by level
TEST(minimap_proto_pink_keyed_and_levels) {
    CHECK(minimap::keyedPink(0xFFFF00FF) && minimap::keyedPink(0xFFFA14F0) && minimap::keyedPink(0x00123456));
    CHECK(!minimap::keyedPink(0xFFC878C8) && !minimap::keyedPink(0xFF20A040) && !minimap::keyedPink(0xFF808080));
    std::vector<uint32_t> argb(4 * 4, 0xFF20A040);
    argb[0] = 0xFFFF00FF;  // magenta in the top left corner
    argb[1] = 0xFFF010E8;  // and an upscale's pink by it
    const minimap::MapPicture m = minimap::prepare(argb, 4, 4);
    CHECK_EQ(m.levels.size(), size_t(3));  // 4, 2, 1
    if (m.levels.size() != 3) return;
    CHECK(m.levels[0].w == 4 && m.levels[1].w == 2 && m.levels[2].w == 1);
    CHECK(m.levels[0].px[0] == 0u && m.levels[0].px[1] == 0u && m.levels[0].px[5] == 0xFF20A040u);
    const uint32_t corner = m.levels[1].px[0];  // two see-through of four: half covered, its colour green
    CHECK((corner >> 24) >= 0x7E && (corner >> 24) <= 0x81 && ((corner >> 8) & 0xFF) > ((corner >> 16) & 0xFF));
}

// 3.0.11-beta.72: a lossy upscale's pink fringe (texture_x4.zip: its first ring round the key 98.6 % pinkish, the
// second 35.8 %, further in 0.1 %) - pinkish within two pixels of the key see-through too; further in, a purple stays
TEST(minimap_proto_prepare_drops_the_pink_fringe) {
    std::vector<uint32_t> argb(16 * 8, 0xFF20A040);
    for (int y = 0; y < 8; ++y) {
        argb[size_t(y * 16 + 0)] = 0xFFFF00FF;   // the key, the first column
        argb[size_t(y * 16 + 1)] = 0xFFA050A0;   // the first ring
        argb[size_t(y * 16 + 2)] = 0xFF604060;   // the second
        argb[size_t(y * 16 + 3)] = 0xFF604060;   // the third: kept
        argb[size_t(y * 16 + 10)] = 0xFFA050A0;  // a purple far from the key: kept
    }
    const minimap::MapPicture m = minimap::prepare(argb, 16, 8);
    CHECK(!m.levels.empty());
    if (m.levels.empty()) return;
    const std::vector<uint32_t>& px = m.levels[0].px;
    CHECK_EQ(px[size_t(3 * 16 + 1)], 0u);
    CHECK_EQ(px[size_t(3 * 16 + 2)], 0u);
    CHECK_EQ(px[size_t(3 * 16 + 3)], 0xFF604060u);
    CHECK_EQ(px[size_t(3 * 16 + 10)], 0xFFA050A0u);
    CHECK_EQ(px[size_t(3 * 16 + 5)], 0xFF20A040u);
}

// 3.0.11-beta.72: an AI picture's soft edge (the pack's pictures keep the key as alpha) - premultiplied, not opaque
TEST(minimap_proto_prepare_keeps_soft_alpha) {
    std::vector<uint32_t> argb = {0x80FF0000, 0xFF0000FF, 0x00FFFFFF, 0x40404040};
    const minimap::MapPicture m = minimap::prepare(argb, 2, 2);
    CHECK_EQ(m.levels.size(), size_t(2));
    const uint32_t half = m.levels[0].px[0];
    CHECK_EQ(half >> 24, 0x80u);
    CHECK(((half >> 16) & 0xFF) >= 0x7F && ((half >> 16) & 0xFF) <= 0x81 && (half & 0xFFFF) == 0);
    CHECK_EQ(m.levels[0].px[1], 0xFF0000FFu);
    CHECK_EQ(m.levels[0].px[2], 0u);
    CHECK_EQ(m.levels[0].px[3] >> 24, 0x40u);
    CHECK(((m.levels[0].px[3] >> 16) & 0xFF) <= 0x11);
}

// The map's edge into the ground smooth and never pink; far out the picture's level whose pixels match the view's
TEST(minimap_proto_edges_smooth_and_far_levels) {
    // a field of green, the game's magenta round it with an upscale's pink between them
    std::vector<uint32_t> argb(64 * 64, 0xFFFF00FF);
    for (int y = 8; y < 56; ++y)
        for (int x = 8; x < 56; ++x) argb[size_t(y * 64 + x)] = 0xFF20A040;
    for (int i = 7; i < 57; ++i) argb[size_t(7 * 64 + i)] = argb[size_t(i * 64 + 7)] = 0xFFE628DC;  // pinkish
    minimap::Scene sc;
    sc.argb = &argb;
    sc.imgW = sc.imgH = 64;
    sc.map = minimap::MapSize{64, 64};
    const minimap::Big b = minimap::bigLayout(1920, 1080, 1.0f, sc.map);
    panel::Picture pic;
    minimap::paintBigMap(pic, b, sc);
    int pink = 0, blend = 0;
    for (const uint32_t c : pic.px) {
        const int r = int((c >> 16) & 0xFF), g = int((c >> 8) & 0xFF), bl = int(c & 0xFF);
        pink += r > g + 40 && bl > g + 40 && r > 0x60;  // a pink tint anywhere
        blend += g > 0x30 && g < 0x90;                  // a step between the field and the ground
    }
    CHECK_EQ(pink, 0);
    CHECK(blend > 0);
    // a fine checkerboard far out: its average grey, no shimmer of whole black or white
    std::vector<uint32_t> board(512 * 512);
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x) board[size_t(y * 512 + x)] = (x + y) % 2 ? 0xFFFFFFFF : 0xFF000000;
    const minimap::MapPicture m = minimap::prepare(board, 512, 512);
    minimap::Scene far;
    far.picture = &m;
    far.map = minimap::MapSize{128, 128};
    const minimap::Big fb = minimap::bigLayout(400, 300, 1.0f, far.map);  // about 2 pixels a cell: 512 -> ~250
    minimap::paintBigMap(pic, fb, far);
    int extreme = 0;
    for (const uint32_t c : pic.px) extreme += (c & 0xFF) < 0x40 || (c & 0xFF) > 0xC0;
    CHECK_EQ(extreme, 0);
}

// 3.0.11-beta.187 (the tester: «коли зум робиш до самого мінімуму, а потім підвищуєш, в перший раз він стає ще
// менший, а потім збільшується»): a step showing the whole map or more is the whole map's - on a map under 220 cells
// the 220 step came after the whole map's and showed it farther out; the buttons and the wheel move past such steps
TEST(minimap_proto_zoom_never_farther_than_the_whole_map) {
    const minimap::MapSize small{200, 180}, tiny{30, 30}, big{400, 300};
    const int whole = minimap::kZoomSteps - 1;
    CHECK(minimap::cellsAt(4, small) == 200 && minimap::cellsAt(3, small) == 150);
    CHECK(minimap::zoomStep(whole, -1, small) == 3);  // nearer from the whole map: 150, not 220
    CHECK(minimap::zoomStep(3, 1, small) == whole && minimap::zoomStep(whole, 1, small) == whole);
    CHECK(minimap::zoomStep(4, -1, small) == 3);  // a step kept from a bigger map counts as the whole one
    CHECK(minimap::zoomStep(0, -1, small) == 0 && minimap::zoomStep(1, -1, small) == 0);
    CHECK(minimap::zoomStep(whole, -1, big) == 4 && minimap::cellsAt(4, big) == 220);  // a big map as before
    CHECK(minimap::zoomStep(whole, -1, tiny) == whole && minimap::cellsAt(0, tiny) == 30);  // nothing nearer to go to
    for (int step = 0; step < minimap::kZoomSteps; ++step)
        CHECK(minimap::cellsAt(step, small) <= minimap::cellsAt(whole, small));
}

// 3.0.11-beta.71 (the owner: «иконка игрока не поворачивается в сторону куда идешь», «туда куда чар смотрит»): the
// character's facing as the game keeps it (its object + 0x38, degrees - FUN_00544da0 turns it toward a point:
// atan2(east, -north); the packet 0x9c sets (dir + 4) * 45) into the arrow's way, east and north
TEST(minimap_proto_facing_of_the_character) {
    float e = 0, n = 0;
    CHECK(minimap::facingOf(180, e, n) && std::fabs(e) < 1e-4f && std::fabs(n - 1) < 1e-4f);  // dir 0: north
    CHECK(minimap::facingOf(90, e, n) && std::fabs(e - 1) < 1e-4f && std::fabs(n) < 1e-4f);    // east
    CHECK(minimap::facingOf(450, e, n) && std::fabs(e - 1) < 1e-4f);  // 0x9c's dir 6, not wrapped: east
    CHECK(minimap::facingOf(0, e, n) && std::fabs(n + 1) < 1e-4f);     // south
    CHECK(minimap::facingOf(270, e, n) && std::fabs(e + 1) < 1e-4f);   // west
    const float toward = std::atan2(1.0f, -1.0f) * 180 / 3.14159265f;  // FUN_00544da0 toward a point north-east
    CHECK(minimap::facingOf(toward, e, n) && std::fabs(e - n) < 1e-4f && e > 0.7f);
    CHECK(!minimap::facingOf(std::nanf(""), e, n) && !minimap::facingOf(1e9f, e, n));
}

// The arrow the way the character faces, the fan where the camera looks; a click on the map: its cell, back and forth
TEST(minimap_proto_arrow_walks_and_clicks_find_cells) {
    const minimap::Layout L = minimap::layout(1280, 720, 1.0f, false, 0);
    const std::vector<uint32_t> argb(4, 0xFF404040);
    minimap::Scene sc;
    sc.argb = &argb;
    sc.imgW = sc.imgH = 2;
    sc.map = minimap::MapSize{100, 100};
    sc.player = minimap::Cell{50, 50};
    sc.lookX = 1;
    // the camera east

    sc.headY = 1;
    // facing north

    ;
    float px = 0, py = 0;
    minimap::cellToPanel(minimap::view(sc.map, sc.player, sc.cells, int(L.map.w)), sc.map, sc.player, px, py);
    px += L.map.x;
    py += L.map.y;
    // the arrow's blue to the north

    // not to the east

    // a click's cell: where cellToPanel puts it, on the minimap and on the big map

    for (const minimap::Cell want : {minimap::Cell{50, 50}, minimap::Cell{62, 41}, minimap::Cell{30, 70}}) {
        float x = 0, y = 0;
        minimap::cellToPanel(minimap::view(sc.map, sc.player, sc.cells, int(L.map.w)), sc.map, want, x, y);
        minimap::Cell got;
        CHECK(minimap::cellAt(L, sc, L.map.x + x, L.map.y + y, got) && got.x == want.x && got.y == want.y);
    }
    const minimap::Big b = minimap::bigLayout(1920, 1080, 1.0f, sc.map);
    minimap::Cell got;
    const float cx = b.map.w / 100.0f;
    // the whole map: a cell's pixels

    CHECK(minimap::bigCellAt(b, sc, b.map.x + (62 + 0.5f) * cx, b.map.y + (100 - 41 - 0.5f) * cx, got));
    CHECK(got.x == 62 && got.y == 41);
    CHECK(!minimap::cellAt(L, sc, L.map.x - 5, L.map.y - 5, got));
    // off its map

}

// 3.0.11-beta.83 (the owner's 3: «участники на миникарте и большой карте»): a party member - its face in a round with a
// blue ring (a dead one's red), named under the pointer
TEST(minimap_proto_party_members) {
    const minimap::Layout L = minimap::layout(1280, 720, 1.0f, false, 0);
    panel::Picture face;
    face.width = face.height = 12;
    face.px.assign(144, 0xFFC08040u);
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    sc.player = minimap::Cell{50, 50};
    minimap::Mark tor{minimap::Cell{30, 70}, 0, minimap::Glyph::Member, "Tor"};
    tor.picture = &face;
    minimap::Mark lis{minimap::Cell{70, 30}, 0, minimap::Glyph::Member, "Lis"};
    lis.dead = true;
    sc.marks = {tor, lis};
    ;
    const minimap::View v = minimap::view(sc.map, sc.player, 100, int(L.map.w));
    float px = 0, py = 0;
    minimap::cellToPanel(v, sc.map, minimap::Cell{30, 70}, px, py);
    // its face

    // its ring

    minimap::cellToPanel(v, sc.map, minimap::Cell{70, 30}, px, py);
    // the dead one's ring

    CHECK(minimap::markAt(L, sc, px + 1, py - 1) == 1);
}

// 3.0.11-beta.120 (the owner: «карту в отчете, она какаято не красивая, может другую попробовать или сделать
// переключалку слоев разных?»): «Схема» - the map drawn from its walkable cells (walkgrid): the floor light (the part
// reached on foot lighter), walls dark, a darker rim where the floor meets a wall; `px` pixels a cell, rows north first
TEST(minimap_proto_scheme_from_the_walkable_cells) {
    // 4 x 3 cells, y 0 the south row: a floor of two cells at (1, 1) and (2, 1), the rest walls
    std::vector<uint8_t> cells(12, 0);
    cells[1 * 4 + 1] = cells[1 * 4 + 2] = 1;
    const walkgrid::Grid g = walkgrid::fromCells(4, 3, cells);
    int w = 0, h = 0;
    const std::vector<uint32_t> px = minimap::schemePixels(g, nullptr, 4, w, h);
    CHECK(w == 16 && h == 12 && px.size() == size_t(w) * size_t(h));
    const auto at = [&](int x, int y) { return px[size_t(y) * size_t(w) + size_t(x)]; };
    CHECK(at(0, 0) == minimap::kSchemeWall);                 // the north-west corner: a wall
    CHECK(at(4 + 1, 4 + 1) == minimap::kSchemeFloor);         // (1, 1)'s inside (the middle row both ways)
    CHECK(at(4, 4 + 1) == minimap::kSchemeEdge);              // its west edge: a wall beyond it
    CHECK(at(4 + 3, 4 + 1) == minimap::kSchemeFloor);         // its east edge: the floor goes on
    const std::vector<uint8_t> reach(12, 1);  // every cell reached: the floor lighter
    const std::vector<uint32_t> lit = minimap::schemePixels(g, &reach, 4, w, h);
    CHECK(lit[size_t(4 + 1) * size_t(w) + size_t(4 + 1)] == minimap::kSchemeReached);
    CHECK(lit[0] == minimap::kSchemeWall);
}

// 3.0.11-beta.120: the picture's switch - a round button after the layers' in the minimap's top left, a chip after
// the layers' under the big map
TEST(minimap_proto_style_switch_places) {
    const minimap::Layout L = minimap::layout(1920, 1080, 1.0f, false, 0);
    const panel::Box& st = L.style;
    CHECK(st.w > 0 && st.x > L.layers[minimap::kLayers - 1].x && st.y == L.layers[0].y && st.x + st.w <= L.map.w);
    CHECK(minimap::partAt(L, st.x + st.w / 2, st.y + st.h / 2) == minimap::Part::Style);
    const float widths[minimap::kLayers] = {90, 70, 80, 75};
    const minimap::Big b = minimap::bigLayout(1600, 900, 1.0f, minimap::MapSize{312, 392}, widths, 84);
    CHECK(b.styleChip.w == 84 && b.styleChip.x > b.chips[minimap::kLayers - 1].x && b.styleChip.y == b.chips[0].y);
    CHECK(b.styleChip.x + b.styleChip.w <= float(b.w));
    CHECK(minimap::bigPartAt(b, b.styleChip.x + 5, b.styleChip.y + 5) == minimap::BigPart::Style);
}

// 3.0.11-beta.201 (the tester: «великі іконки коли зум максимальний (видно всю мапу ...) і зменшуються коли
// збільшується зум»): the marks bigger the more of the map the minimap shows - smaller up close
TEST(minimap_proto_marks_by_the_zoom) {
    const minimap::MapSize map{300, 300};
    CHECK(minimap::markScale(40, map) < 0.95f);
    CHECK(std::fabs(minimap::markScale(100, map) - 1.0f) < 0.12f);
    CHECK(minimap::markScale(300, map) > 1.2f);
    CHECK(minimap::markScale(40, map) < minimap::markScale(60, map));
    CHECK(minimap::markScale(150, map) < minimap::markScale(220, map));
    const minimap::MapSize small{120, 100};
    CHECK(minimap::markScale(120, small) > 1.2f);  // the whole of a small map: as big
    // 3.0.11-beta.262: a small window's minimap (its scale 0.6) - the zoom's change larger, the same at 100 cells
    const float closeUp = minimap::markScale(40, map, 0.6f), wide = minimap::markScale(300, map, 0.6f);
    CHECK(wide - closeUp > (minimap::markScale(300, map) - minimap::markScale(40, map)) * 1.5f);
    CHECK(std::fabs(minimap::markScale(100, map, 0.6f) - minimap::markScale(100, map)) < 0.06f);
    CHECK(minimap::markScale(40, map, 0.3f) >= 0.5f);
}

// 3.0.11-beta.201 (the tester: «є 2 типи порталів ... треба прибрати "зайві" (червоні портали)»): the game's map
// pictures mark the warps with pure red dots (texture_x4.zip's pay_dun01: two, 2601 pixels; prontera's at its gates) -
// the minimap marks them itself (its rings, their words); the dots taken out, their place as round them, the rest kept
TEST(minimap_proto_drops_the_pictures_warp_dots) {
    const int w = 64, h = 64;
    std::vector<uint32_t> px(size_t(w) * h, 0xFF505050u);           // a grey floor
    for (int x = 40; x < 50; ++x) px[size_t(10) * w + x] = 0xFFB4502Du;  // a brown roof's row: not a dot
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float d = std::hypot(float(x) - 20.0f, float(y) - 20.0f);
            if (d <= 6) px[size_t(y) * w + x] = 0xFFF00000u;                   // the dot
            else if (d <= 7) px[size_t(y) * w + x] = 0xFF901818u;              // its soft edge
        }
    CHECK(minimap::dropWarpDots(px, w, h) == 1);
    int red = 0;
    for (const uint32_t c : px) red += ((c >> 16) & 0xFF) > 130 && ((c >> 8) & 0xFF) < 70 && (c & 0xFF) < 70;
    CHECK(red == 0);
    for (int x = 40; x < 50; ++x) CHECK(px[size_t(10) * w + x] == 0xFFB4502Du);  // the roof kept
    const uint32_t mid = px[size_t(20) * w + 20];
    CHECK(std::abs(int((mid >> 16) & 0xFF) - 0x50) < 12 && std::abs(int(mid & 0xFF) - 0x50) < 12);  // the floor's
    std::vector<uint32_t> plain(size_t(w) * h, 0xFF505050u);
    CHECK(minimap::dropWarpDots(plain, w, h) == 0);
}

// 3.0.16-beta.6 (R-W9VM29): ama_dun01's picture draws the maze's route in the same pure red - a line, not a dot: kept
TEST(minimap_proto_keeps_a_red_route) {
    const int w = 128, h = 128;
    std::vector<uint32_t> px(size_t(w) * h, 0xFF505050u);
    for (int x = 10; x < 118; ++x)
        for (int t = 0; t < 2; ++t) px[size_t(20 + t) * w + x] = 0xFFFF0000u, px[size_t(100 + t) * w + x] = 0xFFFF0000u;
    for (int y = 20; y < 102; ++y)
        for (int t = 0; t < 2; ++t) px[size_t(y) * w + 116 + t] = 0xFFFF0000u;
    const std::vector<uint32_t> before = px;
    CHECK(minimap::dropWarpDots(px, w, h) == 0);
    CHECK(px == before);
    for (int y = 0; y < h; ++y)  // and a dot beside it still taken
        for (int x = 0; x < w; ++x)
            if (std::hypot(float(x) - 50.0f, float(y) - 60.0f) <= 3.0f) px[size_t(y) * w + x] = 0xFFF00000u;
    CHECK(minimap::dropWarpDots(px, w, h) == 1);
    CHECK(px[size_t(20) * w + 60] == 0xFFFF0000u);
}

// 3.0.13-beta.2 (reports: the minimap the interface's dearest picture, half of it the map's pixels made anew at each
// step): the map's pixels made once round the view - the steps copy them out, the same picture within a pixel's
// rounding; made anew past the margin and for another zoom
TEST(minimap_proto_map_pixels_cached_round_the_view) {
    // 3.0.18-beta.85: through the map's square (paintMapSquare), the picture uaUI's minimap shows
    const minimap::Layout L = minimap::layout(1920, 1080, 1.5f, false, 0, true);
    std::vector<uint32_t> argb(256 * 256);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x)
            argb[size_t(y) * 256 + size_t(x)] = 0xFF000000u | uint32_t(x) << 16 | uint32_t(y) << 8;
    const minimap::MapPicture mp = minimap::prepare(argb, 256, 256);
    minimap::Scene sc;
    sc.picture = &mp;
    sc.map = minimap::MapSize{300, 300};
    sc.player = minimap::Cell{150, 150};
    minimap::MapCache cache;
    panel::Picture direct, cached;
    const auto close = [&]() {
        int worst = 0;
        for (int y = 4; y < direct.height - 4 && y < cached.height - 4; ++y)
            for (int x = 4; x < direct.width - 4 && x < cached.width - 4; ++x) {
                const uint32_t a = direct.px[size_t(y) * size_t(direct.width) + size_t(x)];
                const uint32_t b = cached.px[size_t(y) * size_t(cached.width) + size_t(x)];
                for (int sh : {16, 8}) worst = std::max(worst, std::abs(int((a >> sh) & 0xFF) - int((b >> sh) & 0xFF)));
            }
        return worst;
    };
    for (int step = 0; step < 6; ++step) {
        sc.player.x = 150 + step;
        sc.cache = nullptr;
        minimap::paintMapSquare(direct, L, sc);
        sc.cache = &cache;
        minimap::paintMapSquare(cached, L, sc);
        CHECK(close() <= 3);  // a gradient of ~1 a pixel: half a pixel's shift at most
    }
    CHECK(cache.made == 1);  // the steps inside the margin
    sc.player.x = 260;
    minimap::paintMapSquare(cached, L, sc);
    CHECK(cache.made == 2);  // past it
    sc.cells = 60;
    minimap::paintMapSquare(cached, L, sc);
    CHECK(cache.made == 3);  // another zoom
}

// 3.0.18-beta.42: uaUI's big map - the map's square alone, as big as the big map's map box
TEST(minimap_proto_big_square_for_uaui) {
    const minimap::MapSize map{100, 100};
    const minimap::Big b = minimap::bigLayout(1920, 1080, 1.0f, map);
    minimap::Scene sc;
    sc.map = map;
    sc.player = minimap::Cell{50, 50};
    panel::Picture picture;
    minimap::paintBigMap(picture, b, sc);
    panel::Picture square;
    minimap::paintBigSquare(square, picture, b, sc);
    CHECK(square.width == int(b.map.w) && square.height == int(b.map.h));
}

// 3.0.18-beta.51 (the owner: «увеличил карту, а там иконки маленькие» - the marks and the buttons): `grow` - the
// buttons and the marks bigger than the map itself grew (an enlarged minimap)
TEST(minimap_proto_buttons_and_marks_grow) {
    const minimap::Layout plain = minimap::layout(1920, 1080, 1.6f, false, 0);
    const minimap::Layout big = minimap::layout(1920, 1080, 1.6f, false, 0, false, false, 1.5f);
    CHECK(big.grow == 1.5f && plain.grow == 1.0f);
    CHECK(big.map.w == plain.map.w);  // the map as big
    CHECK(std::fabs(big.zoomIn.w - plain.zoomIn.w * 1.5f) <= 1.0f && big.layers[0].w > plain.layers[0].w);
    CHECK(big.expand.y + big.expand.h <= big.map.h && big.style.x + big.style.w <= big.map.w);  // still on the map
}

// 3.0.18-beta.220 (R-5773KD «потрібно прибрати рожевий фон біля карти», R-FSNPYE «на всіх картах»): «Довідник»'s
// location picture keyed as the minimap's - the game's magenta and its smudges see-through, the rest as it was
TEST(minimap_pink_keyed_in_a_picture) {
    std::vector<uint32_t> argb = {0xFFFF00FF, 0xFFFA14F0, 0xFF20A040, 0xFFC878C8};
    minimap::keyPink(argb);
    CHECK_EQ(argb[0], 0u);
    CHECK_EQ(argb[1], 0u);
    CHECK_EQ(argb[2], 0xFF20A040u);
    CHECK_EQ(argb[3], 0xFFC878C8u);  // a pinkish colour of the map itself stays
}

// 3.0.18-beta.240 (the reference's «одна карта», the owner 13.11): the map at any box, zoom and centre - fitted at
// zoom 1, zoomed round a point up to its limit, the letters in the dots, the exits' words always (the rest under the
// pointer), the monster zones, no player on another map, the mark under a point within 12 px
TEST(minimap_map_view_fits_and_zooms) {
    const minimap::MapSize map{200, 100};
    minimap::MapView m;
    m.w = 400, m.h = 400;
    minimap::View v = minimap::viewOf(m, map);
    CHECK(std::fabs(v.cellPx - 2) < 1e-4f);
    float x = 0, y = 0;
    minimap::cellToPanel(v, map, minimap::Cell{0, 99}, x, y);  // the map's top left cell, the map in the middle
    CHECK(std::fabs(x - 1) < 1e-3f && std::fabs(y - 101) < 1e-3f);
    minimap::zoomMapView(m, map, 200, 200, 2, 2);
    CHECK(std::fabs(m.zoom - 2) < 1e-4f);
    minimap::zoomMapView(m, map, 200, 200, 2, 2);
    CHECK(std::fabs(m.zoom - 2) < 1e-4f);  // its limit
    v = minimap::viewOf(m, map);
    CHECK(std::fabs(v.cellPx - 4) < 1e-4f);
    minimap::zoomMapView(m, map, 0, 0, 0.1f, 2);
    CHECK(std::fabs(m.zoom - 1) < 1e-4f);  // never smaller than the map fitted
    CHECK(std::fabs(minimap::mapViewMost(false) - 2) < 1e-4f && std::fabs(minimap::mapViewMost(true) - 4) < 1e-4f);
}

TEST(minimap_map_view_letters_and_exit_words) {
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    minimap::Mark shop{minimap::Cell{20, 20}, 0x3B8AE0, minimap::Glyph::Dot, "Tool Dealer"};
    shop.letter = "$";
    minimap::Mark kafra{minimap::Cell{40, 40}, 0xE0447C, minimap::Glyph::Kafra, "Kafra Employee"};
    minimap::Mark out{minimap::Cell{90, 50}, 0, minimap::Glyph::Warp, "\xE2\x86\x92 Prontera Field"};
    out.exit = true;
    minimap::Mark inn{minimap::Cell{60, 60}, 0, minimap::Glyph::Warp, "\xE2\x86\x92 Inn"};
    sc.marks = {shop, kafra, out, inn};
    minimap::MapView m;
    m.w = m.h = 500;
    const auto has = [](const std::vector<minimap::Words>& w, const std::string& t) {
        for (const minimap::Words& x : w)
            if (x.text == t) return true;
        return false;
    };
    std::vector<minimap::Words> w = minimap::mapViewWords(m, sc);
    CHECK(has(w, "$") && has(w, "K") && has(w, "\xE2\x86\x92 Prontera Field"));
    CHECK(!has(w, "\xE2\x86\x92 Inn") && !has(w, "Tool Dealer") && !has(w, "Kafra Employee"));
    sc.hoverMark = 3;  // under the pointer: its words too
    w = minimap::mapViewWords(m, sc);
    CHECK(has(w, "\xE2\x86\x92 Inn"));
}

TEST(minimap_map_view_zones_and_no_player) {
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    sc.player = minimap::Cell{10, 10};
    sc.zones = {minimap::ZoneMark{minimap::Cell{70, 70}, 10, 10}};
    minimap::MapView m;
    m.w = m.h = 200;  // 2 px a cell
    panel::Picture with, without;
    minimap::paintMapView(with, m, sc);
    sc.showPlayer = false;
    minimap::paintMapView(without, m, sc);
    float x = 0, y = 0;
    minimap::cellToPanel(minimap::viewOf(m, sc.map), sc.map, sc.player, x, y);
    const auto at = [](const panel::Picture& p, float px, float py) { return p.px[size_t(int(py)) * p.width + int(px)]; };
    CHECK(at(with, x, y) != at(without, x, y));  // the player drawn on his own map only
    minimap::cellToPanel(minimap::viewOf(m, sc.map), sc.map, minimap::Cell{70, 70}, x, y);
    CHECK(at(without, x, y) != at(without, 100, 20));  // the zone tinted over the ground
}

TEST(minimap_map_view_mark_at) {
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    sc.marks = {minimap::Mark{minimap::Cell{30, 30}, 0x3B8AE0, minimap::Glyph::Dot, "A"}};
    minimap::MapView m;
    m.w = m.h = 400;
    float x = 0, y = 0;
    minimap::cellToPanel(minimap::viewOf(m, sc.map), sc.map, sc.marks[0].cell, x, y);
    CHECK_EQ(minimap::mapViewMarkAt(m, sc, x + 10, y), 0);  // within 12 px
    CHECK_EQ(minimap::mapViewMarkAt(m, sc, x + 30, y), -1);
}
