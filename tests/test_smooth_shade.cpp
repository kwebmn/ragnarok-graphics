#include "core/smooth_shade.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A corner at a view place (x, y, z) by a projection of f 100 about (0, 0).
smooth_shade::Corner at(float x, float y, float z, uint32_t colour) {
    smooth_shade::Corner c;
    c.x = x / z * 100.0f;
    c.y = y / z * 100.0f;
    c.rhw = 1.0f / z;
    c.colour = colour;
    return c;
}

uint32_t grey(int v) { return 0xFF000000u | uint32_t(v) << 16 | uint32_t(v) << 8 | uint32_t(v); }

}  // namespace

TEST(smooth_shade_rounds_a_shallow_fold_keeps_a_sharp_one) {
    // two faces meeting along x = 0 at a shallow angle (a rock's), each its own colour, lists of their own corners
    std::vector<smooth_shade::Corner> v = {
        at(-1, 0, 10, grey(100)), at(0, 0, 10, grey(100)), at(0, 1, 10, grey(100)),     // left, flat
        at(0, 0, 10, grey(200)), at(1, 0, 10.3f, grey(200)), at(0, 1, 10, grey(200)),  // right, turned a little
    };
    std::vector<uint32_t> tris = {0, 1, 2, 3, 4, 5};
    std::vector<smooth_shade::Change> c = smooth_shade::smooth(v, tris, 100, 100, 0, 0);
    CHECK(!c.empty());
    bool sharedMid = false;
    for (const auto& ch : c)
        if (ch.vertex == 1 || ch.vertex == 3)
            sharedMid = sharedMid || ((ch.colour & 255) > 100 && (ch.colour & 255) < 200);
    CHECK(sharedMid);
    for (const auto& ch : c) CHECK(ch.vertex != 0 && ch.vertex != 4);  // the corners no other face shares
    // folded at a right angle (a house's wall and roof): nothing changes
    v[4] = at(0, 0, 11, grey(200));
    CHECK(smooth_shade::smooth(v, tris, 100, 100, 0, 0).empty());
}

TEST(smooth_shade_leaves_a_draw_lit_smoothly) {
    std::vector<smooth_shade::Corner> v = {at(-1, 0, 10, grey(100)), at(0, 0, 10, grey(150)), at(0, 1, 10, grey(120)),
                                          at(1, 0, 10.3f, grey(200))};
    std::vector<uint32_t> tris = {0, 1, 2, 1, 3, 2};
    CHECK(smooth_shade::smooth(v, tris, 100, 100, 0, 0).empty());
}
