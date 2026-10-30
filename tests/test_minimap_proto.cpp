// 3.0.11-beta.1: the new interface's minimap prototype (stage 1, the owner's «давай» 2026-10-28) - the player's cell by
// uaRO.exe's own formula, the panel in the old one's corner, the view round the player, the picture
#include <algorithm>
#include <cmath>
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

// 3.0.11-beta.65 (the concept's «Подсветка, куда можно дойти» - an experiment, [interface] minimap_reach): the cells
// not reached on foot from the player dimmed on the map's picture, those reached as they are
TEST(minimap_proto_dims_what_cannot_be_reached) {
    const std::vector<uint32_t> argb(64 * 64, 0xFFE0E0E0);  // a light map
    minimap::Scene sc;
    sc.argb = &argb;
    sc.imgW = sc.imgH = 64;
    sc.map = minimap::MapSize{40, 40};
    std::vector<uint8_t> reach(40 * 40, 0);
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

namespace {

// The map's pixels as 3.0.11-beta.31..56 worked them out pixel by pixel (minimap_proto.cpp mapPixels then) - the
// faster one must give the very same picture
void mapPixelsBefore(panel::Picture& out, const panel::Box& box, const minimap::Scene& sc, const minimap::View& v) {
    const bool picture = sc.argb && sc.imgW > 0 && sc.imgH > 0 && sc.map.w > 0 && sc.map.h > 0 && v.cellPx > 0 &&
                         sc.argb->size() >= size_t(sc.imgW) * size_t(sc.imgH);
    for (int y = 0; y < int(box.h); ++y) {
        const int ty = int(box.y) + y;
        if (ty < 0 || ty >= out.height) continue;
        const float cy = v.top - (float(y) + 0.5f) / v.cellPx;
        for (int x = 0; x < int(box.w); ++x) {
            const int tx = int(box.x) + x;
            if (tx < 0 || tx >= out.width) continue;
            uint32_t c = 0xFF141922;
            const float cx = v.left + (float(x) + 0.5f) / v.cellPx;
            if (picture && cx >= 0 && cy >= 0 && cx < float(sc.map.w) && cy < float(sc.map.h)) {
                const float fx = std::clamp(cx / float(sc.map.w) * float(sc.imgW) - 0.5f, 0.0f, float(sc.imgW - 1));
                const float fy = std::clamp((float(sc.map.h) - cy) / float(sc.map.h) * float(sc.imgH) - 0.5f, 0.0f,
                                            float(sc.imgH - 1));
                const int x0 = int(fx), y0 = int(fy), x1 = std::min(x0 + 1, sc.imgW - 1),
                          y1 = std::min(y0 + 1, sc.imgH - 1);
                const auto px = [&](int ix, int iy) { return (*sc.argb)[size_t(iy) * size_t(sc.imgW) + size_t(ix)]; };
                const auto keyed = [](uint32_t q) { return (q & 0xFFFFFF) == 0xFF00FF || (q >> 24) == 0; };
                const uint32_t q[4] = {px(x0, y0), px(x1, y0), px(x0, y1), px(x1, y1)};
                const float wx = fx - float(x0), wy = fy - float(y0);
                if (keyed(q[0]) || keyed(q[1]) || keyed(q[2]) || keyed(q[3])) {
                    const uint32_t n = q[(wy >= 0.5f ? 2 : 0) + (wx >= 0.5f ? 1 : 0)];
                    if (!keyed(n)) c = 0xFF000000 | (n & 0xFFFFFF);
                } else {
                    uint32_t mixed = 0xFF000000;
                    for (int sh = 0; sh <= 16; sh += 8) {
                        const float top = float((q[0] >> sh) & 0xFF) * (1 - wx) + float((q[1] >> sh) & 0xFF) * wx;
                        const float low = float((q[2] >> sh) & 0xFF) * (1 - wx) + float((q[3] >> sh) & 0xFF) * wx;
                        mixed |= uint32_t(top * (1 - wy) + low * wy + 0.5f) << sh;
                    }
                    c = mixed;
                }
            }
            out.px[size_t(ty) * size_t(out.width) + size_t(tx)] = c;
        }
    }
}

// A map's picture of the game's colours, its magenta (see-through) and a few pixels with no alpha
std::vector<uint32_t> noisyPicture(int w, int h, uint32_t seed) {
    std::vector<uint32_t> out(size_t(w) * size_t(h));
    for (uint32_t& p : out) {
        seed = seed * 1664525u + 1013904223u;
        const uint32_t pick = seed >> 28;
        p = pick == 0 ? 0xFFFF00FFu : pick == 1 ? (seed & 0x00FFFFFFu) : 0xFF000000u | (seed >> 4 & 0xFFFFFFu);
    }
    return out;
}

}  // namespace

// 3.0.11-beta.57 (R-T942DZ: the minimap painted again at each step - 15.6 ms at 4K): the map's pixels worked out
// faster, the same as before to the last bit - the big map (the whole map fitted) and the minimap round the player,
// its view past the map's edge too (the ground's colour there)
TEST(minimap_proto_map_pixels_as_before) {
    struct Case {
        int imgW, imgH;
        minimap::MapSize map;
        int targetW, targetH;
        float scale;
    };
    const Case cases[] = {{512, 512, {400, 400}, 3839, 2159, 2.0f}, {512, 512, {312, 392}, 1920, 1080, 1.0f},
                          {333, 257, {50, 80}, 1280, 720, 1.5f},   {2, 3, {100, 100}, 1280, 720, 1.0f},
                          {1, 1, {40, 30}, 1920, 1080, 1.25f}};
    uint32_t seed = 7;
    for (const Case& c : cases) {
        const std::vector<uint32_t> argb = noisyPicture(c.imgW, c.imgH, seed += 101);
        minimap::Scene sc;
        sc.argb = &argb;
        sc.imgW = c.imgW;
        sc.imgH = c.imgH;
        sc.map = c.map;
        const minimap::Big b = minimap::bigLayout(c.targetW, c.targetH, c.scale, c.map);
        panel::Picture now, before;
        minimap::paintBigMap(now, b, sc);
        before.width = now.width;
        before.height = now.height;
        before.px.assign(now.px.size(), 0xFF141922);
        minimap::View whole;
        whole.cellPx = b.map.w / float(c.map.w);
        whole.top = float(c.map.h);
        mapPixelsBefore(before, panel::Box{0, 0, b.map.w, b.map.h}, sc, whole);
        CHECK(now.px == before.px);
        // the minimap: zoomed in (the view inside the map), the whole map (past its edges), the player in a corner
        const minimap::Layout L = minimap::layout(c.targetW, c.targetH, c.scale, false, 0);
        for (const auto& [player, step] : {std::pair{minimap::Cell{c.map.w / 2, c.map.h / 2}, minimap::kZoomFirst},
                                           std::pair{minimap::Cell{c.map.w / 3, c.map.h - 2}, minimap::kZoomSteps - 1},
                                           std::pair{minimap::Cell{1, 1}, 0}}) {
            sc.player = player;
            sc.cells = minimap::cellsAt(step, c.map);
            minimap::paint(now, L, sc, minimap::Texts{});
            before = panel::Picture{};
            before.width = now.width;
            before.height = now.height;
            before.px.assign(now.px.size(), 0u);
            mapPixelsBefore(before, L.map, sc, minimap::view(sc.map, sc.player, sc.cells, int(L.map.w)));
            // away from the square's rim, its corners, its buttons and the player's mark
            float mx = 0, my = 0;
            minimap::cellToPanel(minimap::view(sc.map, sc.player, sc.cells, int(L.map.w)), sc.map, sc.player, mx, my);
            const float reach = 12 * L.s + 3;
            int same = 0, other = 0;
            for (int y = int(L.map.h * 0.1f); y < int(L.map.h * 0.4f); ++y)
                for (int x = int(L.map.w * 0.2f); x < int(L.map.w * 0.9f); ++x) {
                    if (std::hypot(float(x) + 0.5f - mx, float(y) + 0.5f - my) < reach) continue;
                    const size_t i = size_t(y) * size_t(now.width) + size_t(x);
                    ++(now.px[i] == before.px[i] ? same : other);
                }
            CHECK(same > 0);
            CHECK_EQ(other, 0);
        }
    }
}

// 3.0.11-beta.57: the camera's fan worked out only near it - the arrow and its fan as before, turned every way
TEST(minimap_proto_view_fan_as_before) {
    const minimap::Layout L = minimap::layout(1920, 1080, 2.0f, false, 0);
    minimap::Scene sc;
    sc.map = minimap::MapSize{100, 100};
    sc.player = minimap::Cell{50, 50};
    panel::Picture pic;
    for (int i = 0; i < 16; ++i) {
        const float a = float(i) * 0.3927f + 0.05f;  // round the circle
        sc.lookX = std::cos(a);
        sc.lookY = std::sin(a);
        minimap::paint(pic, L, sc, minimap::Texts{});
        // ahead of the arrow (past its tip) the fan's blue over the ground; behind it the ground alone
        float px = 0, py = 0;
        minimap::cellToPanel(minimap::view(sc.map, sc.player, sc.cells, int(L.map.w)), sc.map, sc.player, px, py);
        const float ahead = 9.5f * L.s * 2.2f, dx = sc.lookX, dy = -sc.lookY;
        const uint32_t fan = pic.px[size_t(py + dy * ahead) * size_t(pic.width) + size_t(px + dx * ahead)];
        const uint32_t back = pic.px[size_t(py - dy * ahead) * size_t(pic.width) + size_t(px - dx * ahead)];
        CHECK((fan & 0xFF) > (back & 0xFF) + 0x10);
        CHECK(back == 0xFF141922u);
    }
}
