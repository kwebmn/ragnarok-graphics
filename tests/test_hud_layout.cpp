// 3.0.11-beta.75 (the owner: «некоторые интерфейсы хотелось бы чтобы можно было бы двигать и настраивать»): the new
// interface's parts placed by the player - kept to the screen's nearest corner, edge or middle, sized 60-160 %
// (core/hud_layout)
#include <cmath>
#include <string>
#include <vector>

#include "core/hud_layout.hpp"
#include "core/hud_layout_panel.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(hud_layout_file_round_trip) {
    hudlayout::Layout l;
    l[int(hudlayout::Part::Minimap)] = hudlayout::Place{true, 2, -12.5f, 8, 1.1f};
    l[int(hudlayout::Part::Hotbar)].size = 0.8f;  // sized, not moved
    const std::string t = hudlayout::text(l);
    const hudlayout::Layout back = hudlayout::parse(t);
    const hudlayout::Place& m = back[int(hudlayout::Part::Minimap)];
    CHECK(m.moved && m.anchor == 2 && std::fabs(m.dx + 12.5f) < 0.01f && std::fabs(m.dy - 8) < 0.01f);
    CHECK(std::fabs(m.size - 1.1f) < 0.001f);
    const hudlayout::Place& bar = back[int(hudlayout::Part::Hotbar)];
    CHECK(!bar.moved && std::fabs(bar.size - 0.8f) < 0.001f);
    CHECK(!back[int(hudlayout::Part::Chat)].moved && back[int(hudlayout::Part::Chat)].size == 1.0f);
    CHECK(t.find("chat") == std::string::npos);  // the parts as they always are: not written
    // words it does not know skipped, sizes kept within, anchors within
    const hudlayout::Layout odd = hudlayout::parse("# a note\nnothing 1 2 3 4\nbuffs 1 5 5 9\ntarget 42 0 0 1\n");
    CHECK(odd[int(hudlayout::Part::Buffs)].moved && odd[int(hudlayout::Part::Buffs)].size == hudlayout::kMostSize);
    CHECK(!odd[int(hudlayout::Part::Target)].moved);
    CHECK_EQ(std::string(hudlayout::keyOf(hudlayout::Part::Character)), std::string("character"));
}

TEST(hud_layout_keeps_its_corner) {
    hudlayout::Place p;
    int x = 100, y = 50;
    hudlayout::apply(p, 2, 3840, 2160, 400, 300, x, y);  // not moved: where it stands by default
    CHECK(x == 100 && y == 50);
    // placed at the bottom right corner, 20 game pixels in from each edge (k 2: 40 of the picture's)
    const hudlayout::Box b{3840 - 400 - 40, 2160 - 300 - 40, 400, 300};
    p = hudlayout::placeOf(b, 2, 3840, 2160, 1.0f);
    CHECK(p.moved && p.anchor == 8);
    hudlayout::apply(p, 2, 3840, 2160, 400, 300, x, y);
    CHECK(std::abs(x - b.x) <= 1 && std::abs(y - b.y) <= 1);
    // another resolution and a smaller part: still 20 game pixels from the corner
    hudlayout::apply(p, 1, 1280, 720, 200, 150, x, y);
    CHECK(std::abs(x - (1280 - 200 - 20)) <= 1 && std::abs(y - (720 - 150 - 20)) <= 1);
    // the top middle keeps its middle
    const hudlayout::Box t{3840 / 2 - 200 + 60, 30, 400, 100};
    p = hudlayout::placeOf(t, 2, 3840, 2160, 1.0f);
    CHECK_EQ(p.anchor, 1);
    hudlayout::apply(p, 2, 1920, 1080, 400, 100, x, y);
    CHECK(std::abs(x - (1920 / 2 - 200 + 60)) <= 1 && std::abs(y - 30) <= 1);
    // never off the screen
    p.dx = 5000;
    hudlayout::apply(p, 2, 1920, 1080, 400, 100, x, y);
    CHECK(x == 1920 - 400 && y >= 0);
}

TEST(hud_layout_snaps_to_edges_and_the_others) {
    const std::vector<hudlayout::Box> others = {{1000, 200, 300, 100}};
    // near the screen's left edge and its top
    hudlayout::Snapped s = hudlayout::snap({6, 5, 200, 100}, others, 1920, 1080, 10);
    CHECK(s.x == 0 && s.y == 0 && s.guideX == 0 && s.guideY == 0);
    // its left edge near another's right edge, its top near that one's top
    s = hudlayout::snap({1306, 207, 200, 100}, others, 1920, 1080, 10);
    CHECK(s.x == 1300 && s.y == 200 && s.guideX == 1300 && s.guideY == 200);
    // its middle near the screen's middle
    s = hudlayout::snap({1920 / 2 - 100 + 4, 600, 200, 100}, {}, 1920, 1080, 10);
    CHECK(s.x == 1920 / 2 - 100 && s.guideX == 1920 / 2 && s.guideY == -1 && s.y == 600);
    // too far: as it was
    s = hudlayout::snap({500, 500, 200, 100}, others, 1920, 1080, 10);
    CHECK(s.x == 500 && s.y == 500 && s.guideX == -1 && s.guideY == -1);
}

TEST(hud_layout_size_steps) {
    CHECK(std::fabs(hudlayout::stepped(1.0f, 1) - 1.05f) < 1e-4f);
    CHECK(std::fabs(hudlayout::stepped(1.0f, -2) - 0.9f) < 1e-4f);
    CHECK(hudlayout::stepped(1.58f, 3) == hudlayout::kMostSize);
    CHECK(hudlayout::stepped(0.62f, -3) == hudlayout::kLeastSize);
}

// 3.0.11-beta.75: the editor's bar at the top middle and a part's frame with its name and size
TEST(hud_layout_bar_and_frames) {
    const hudlayout::Bar b = hudlayout::barLayout(1920, 1.0f);
    CHECK(b.w > 400 && b.h > 30 && std::abs(b.x + b.w / 2 - 960) <= 1 && b.y > 0);
    CHECK(hudlayout::barHit(b, b.done.x + 2, b.done.y + 2) == hudlayout::BarPart::Done);
    CHECK(hudlayout::barHit(b, b.reset.x + 2, b.reset.y + 2) == hudlayout::BarPart::Reset);
    CHECK(hudlayout::barHit(b, 4, b.h / 2) == hudlayout::BarPart::Body);
    CHECK(hudlayout::barHit(b, -3, 3) == hudlayout::BarPart::None);
    const hudlayout::Bar big = hudlayout::barLayout(3840, 2.0f);
    CHECK(big.w >= 2 * b.w - 2 && big.done.w > b.done.w);
    panel::Picture bar;
    hudlayout::paintBar(bar, b, hudlayout::BarPart::Done);
    CHECK(bar.width == b.w && bar.height == b.h && (bar.px[size_t(b.h / 2) * size_t(b.w) + size_t(b.w / 2)] >> 24) > 0);
    const std::vector<hudlayout::Words> bw = hudlayout::barWords(b, {"Розміщення", "тягніть", "Скинути", "Готово"});
    CHECK(bw.size() == 4u && bw[0].text == "Розміщення" && bw[3].text == "Готово");
    panel::Picture f;
    hudlayout::paintFrame(f, 300, 200, 1.0f, hudlayout::Look::Hover, 80);
    CHECK(f.width == 300 && f.height == 200);
    const uint32_t tag = f.px[size_t(14) * 300 + 40];  // the dark tag under its words
    CHECK((tag >> 24) > 0xC0 && ((tag >> 16) & 0xFF) < 0x40);
    CHECK((f.px[size_t(100) * 300 + 1] >> 24) > 0x80);    // its edge
    CHECK((f.px[size_t(100) * 300 + 150] >> 24) < 0x80);  // its middle: the part shows through
    const std::vector<hudlayout::Words> fw = hudlayout::frameWords(300, 200, 1.0f, "Мінікарта", 110);
    CHECK(!fw.empty() && fw[0].text == "Мінікарта · 110 %");
    CHECK(hudlayout::frameWords(300, 200, 1.0f, "Чат", 100)[0].text == "Чат");
}
