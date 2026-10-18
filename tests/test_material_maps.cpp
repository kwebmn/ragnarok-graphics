#include <cstdint>
#include <vector>

#include "core/material_maps.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::vector<uint32_t> bumps(int w, int h) {  // stripes of light and dark: relief, height
    std::vector<uint32_t> p(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) p[size_t(y * w + x)] = (x / 4) % 2 ? 0xFFC8C8C8u : 0xFF404040u;
    return p;
}

int ch(uint32_t v, int c) { return int(v >> (8 * c) & 0xFF); }  // RGBA as material_ps writes: r 0, g 1, b 2, a 3

}  // namespace

TEST(material_maps_of_a_flat_grey) {
    // 2.3: the material the driver makes of a picture (material_ps), on the processor for the studio's preview
    const std::vector<uint32_t> flat(64 * 64, 0xFF808080u);
    std::vector<uint32_t> m;
    int w = 0, h = 0;
    material::mapsOf(flat.data(), 64, 64, pack::MaterialScales{}, m, w, h);
    CHECK(w == 64 && h == 64 && m.size() == 64u * 64u);
    CHECK(ch(m[100], 0) == 128 && ch(m[100], 1) == 128);  // no relief: the normal straight up
    CHECK(ch(m[100], 2) == 128);                          // no height
    CHECK(ch(m[100], 3) > 100 && ch(m[100], 3) < 150);    // grey and mid-bright: some gloss (0.52)
}

TEST(material_maps_follow_the_scales) {
    const std::vector<uint32_t> b = bumps(64, 64);
    std::vector<uint32_t> as, none;
    int w = 0, h = 0;
    material::mapsOf(b.data(), 64, 64, pack::MaterialScales{}, as, w, h);
    material::mapsOf(b.data(), 64, 64, pack::MaterialScales{0.0f, 0.0f, 0.0f}, none, w, h);
    bool relief = false, height = false;
    for (size_t i = 0; i < as.size(); ++i) {
        relief = relief || ch(as[i], 0) != 128;
        height = height || ch(as[i], 2) != 128;
        CHECK(ch(none[i], 0) == 128 && ch(none[i], 1) == 128);  // relief 0: flat
        CHECK(ch(none[i], 2) == 128);                          // height 0: level
        CHECK(ch(none[i], 3) == 0);                            // gloss 0: none, no window's pane either
    }
    CHECK(relief && height);
    // a big picture's material is made at most 512 a side, as the driver's
    const std::vector<uint32_t> big(1024 * 1024, 0xFF808080u);
    material::mapsOf(big.data(), 1024, 1024, pack::MaterialScales{}, as, w, h);
    CHECK(w == 512 && h == 512);
}
