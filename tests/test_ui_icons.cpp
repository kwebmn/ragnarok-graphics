// 3.0.11-beta.26: the interface concept's line icons (core/ui_icons, tools/ui_icons.py) - drawn where their drawings
// are, at any size, inside the picture's clip
#include <cstdint>
#include <vector>

#include "core/ui_icons.hpp"
#include "test.hpp"

using namespace uagfx;
using uiicons::Icon;

namespace {

panel::Picture blank(int w, int h) {
    panel::Picture p;
    p.width = w;
    p.height = h;
    p.px.assign(size_t(w) * size_t(h), 0xFF000000u);
    return p;
}

uint32_t at(const panel::Picture& p, int x, int y) { return p.px[size_t(y) * size_t(p.width) + size_t(x)] & 0xFFFFFFu; }

}  // namespace

TEST(ui_icon_search_is_a_ring_and_a_handle) {
    for (const int size : {24, 48}) {
        panel::Picture p = blank(size + 8, size + 8);
        uiicons::draw(p, Icon::Search, 4, 4, float(size), 0xFFFFFF);
        const float k = float(size) / 24;
        const auto px = [&](float v) { return 4 + int(v * k); };
        CHECK(at(p, px(11 + 6.5f), px(11)) != 0);    // the ring
        CHECK(at(p, px(11), px(11 - 6.5f)) != 0);
        CHECK_EQ(at(p, px(11), px(11)), 0u);           // its middle empty
        CHECK(at(p, px(18.5f), px(18.5f)) != 0);      // the handle
        CHECK_EQ(at(p, 1, 1), 0u);
        CHECK_EQ(at(p, px(3), px(21)), 0u);
    }
}

TEST(ui_icons_each_drawn_inside_its_box) {
    for (int i = 0; i < int(Icon::kCount); ++i) {
        panel::Picture p = blank(64, 64);
        uiicons::draw(p, Icon(i), 16, 16, 32, 0xFFFFFF);
        int inside = 0, outside = 0;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                if (at(p, x, y) == 0) continue;
                (x >= 13 && x < 51 && y >= 13 && y < 51 ? inside : outside)++;  // the box and its lines' half width
            }
        CHECK(inside > 20);
        CHECK_EQ(outside, 0);
    }
}

TEST(ui_icon_keeps_to_the_clip) {
    panel::Picture p = blank(40, 40);
    p.clipX0 = 20;
    uiicons::draw(p, Icon::Tables, 4, 4, 32, 0xFFFFFF);
    int left = 0, right = 0;
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 40; ++x)
            if (at(p, x, y) != 0) (x < 20 ? left : right)++;
    CHECK_EQ(left, 0);
    CHECK(right > 30);
}

TEST(ui_icon_colour_and_strength) {
    panel::Picture p = blank(30, 30);
    uiicons::draw(p, Icon::Back, 3, 3, 24, 0xE6CB8A, 1.0f, 2.4f);
    uint32_t brightest = 0;
    for (const uint32_t c : p.px) brightest = std::max(brightest, c & 0xFFFFFFu);
    CHECK_EQ(brightest, 0xE6CB8Au);  // the line's middle is the colour itself
    panel::Picture q = blank(30, 30);
    uiicons::draw(q, Icon::Back, 3, 3, 24, 0xFFFFFF, 0.5f);
    for (const uint32_t c : q.px) CHECK((c & 0xFF) <= 0x80);
}
