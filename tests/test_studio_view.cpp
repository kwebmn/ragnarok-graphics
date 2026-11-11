#include <cstdint>
#include <string>
#include <vector>

#include "core/studio_view.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(view_zoom_keeps_the_texel_under_the_cursor) {
    // 2.3, the studio's compare view (T2): the wheel zooms about the cursor
    const studio::View v{2.0f, 10.0f, 20.0f};
    const studio::View z = studio::zoomAt(v, 2.0f, 100.0f, 50.0f);
    CHECK(z.zoom == 4.0f);
    CHECK(z.x + 100.0f / z.zoom == 60.0f && z.y + 50.0f / z.zoom == 45.0f);
    const studio::View most = studio::zoomAt(studio::View{16.0f, 3.0f, 4.0f}, 2.0f, 10.0f, 10.0f);
    CHECK(most.zoom == 16.0f && most.x == 3.0f && most.y == 4.0f);  // 16 at most
    const studio::View least = studio::zoomAt(studio::View{0.25f, 0.0f, 0.0f}, 0.5f, 10.0f, 10.0f);
    CHECK(least.zoom == 0.25f);  // a quarter at least
}

TEST(view_difference_marks_changes) {
    std::vector<uint32_t> a(16 * 16, 0xFF808080u), b(32 * 32, 0xFF808080u);
    b[size_t(10 * 32 + 10)] = 0xFF888080u;  // one texel of the x2 picture eight levels redder
    const std::vector<uint32_t> d = studio::differenceImage(a.data(), 16, 16, b.data(), 32, 32, 32, 32);
    CHECK_EQ(d.size(), size_t(32 * 32));
    CHECK_EQ(d[size_t(10 * 32 + 10)], 0xFF200000u);  // 8 levels times 4, red
    CHECK_EQ(d[0], 0xFF000000u);
}

TEST(rows_filter_by_navigator_and_text) {
    catalog::FileEntry f;
    f.name = "data\\texture\\fields\\sand01.bmp";
    f.maps = {3, 7};
    catalog::PictureOf p;
    p.key = "256x256\\0123456789abcdef";
    studio::RowFacts facts;
    facts.state = catalog::State::InPack;
    facts.model = "anime";
    facts.flags = 1;
    studio::RowFilter r;
    CHECK(studio::rowShown(f, p, facts, r));
    r.nav = "map:7";
    CHECK(studio::rowShown(f, p, facts, r));
    r.nav = "map:4";
    CHECK(!studio::rowShown(f, p, facts, r));
    r.nav = "folder:data\\texture\\fields\\";
    CHECK(studio::rowShown(f, p, facts, r));
    r.nav = "state:missing";
    CHECK(!studio::rowShown(f, p, facts, r));
    r.nav = "state:inpack";
    CHECK(studio::rowShown(f, p, facts, r));
    r.nav = "model:anime";
    CHECK(studio::rowShown(f, p, facts, r));
    r.nav = "model:general";
    CHECK(!studio::rowShown(f, p, facts, r));
    r.nav = "flagged";
    CHECK(studio::rowShown(f, p, facts, r));
    r.nav = "overrides";
    CHECK(!studio::rowShown(f, p, facts, r));
    r.nav = "seen:cmd_fild02";
    CHECK(!studio::rowShown(f, p, facts, r));
    r.keys.insert(p.key);
    CHECK(studio::rowShown(f, p, facts, r));
    r.nav.clear();
    r.text = "SAND";  // any case
    CHECK(studio::rowShown(f, p, facts, r));
    r.text = "0123456789AB";  // a key's
    CHECK(studio::rowShown(f, p, facts, r));
    r.text = "grass";
    CHECK(!studio::rowShown(f, p, facts, r));
    // 2.3.1: a game at a lower texture detail draws (and picks) the texture by its lower key
    p.lowKeys = {"128x128\\00000000000000aa"};
    r.text = "128x128\\00000000000000aa";
    CHECK(studio::rowShown(f, p, facts, r));
}
