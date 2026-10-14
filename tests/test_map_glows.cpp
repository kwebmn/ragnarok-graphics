#include <cmath>

#include "core/map_glows.hpp"
#include "test.hpp"

using namespace uagfx;
using map_glows::Shape;

TEST(map_glows_know_the_maps_hidden_lights) {
    // 0.29: what the maps' makers placed that uaRO.exe does not show (it knows effect ids up to 629).
    map_glows::Glow g;
    for (int id : {693, 694, 695, 701, 702}) {  // glow1, glow2, glow4, glow11, glow12: floating orbs
        CHECK(map_glows::glowOf(id, g));
        CHECK(g.shape == Shape::Orb);
        CHECK(g.reach > 0.0f && g.light > 0.0f);
    }
    CHECK(map_glows::glowOf(703, g) && g.shape == Shape::Pool);  // circlelight: a pool of light on the ground
    CHECK(g.reach >= 20.0f);
    CHECK(map_glows::glowOf(710, g) && g.shape == Shape::Orb && g.colour[1] == 1.0f && g.colour[0] < 0.6f);  // green
    CHECK(map_glows::glowOf(712, g) && g.colour[1] == 1.0f);
    CHECK(map_glows::glowOf(689, g) && g.shape == Shape::Dust && g.light == 0.0f);  // dust motes: no light
    for (int id : {47, 690, 713, 946, 650, 821, 0}) CHECK(!map_glows::glowOf(id, g));  // drawn elsewhere, or not
}

TEST(map_glows_breathe_and_bob_slowly) {
    float lo = 2.0f, hi = 0.0f, low = 0.0f, high = -20.0f;
    for (int i = 0; i < 3000; ++i) {
        const float p = map_glows::pulse(i * 0.01, 0.4f);
        lo = std::fmin(lo, p);
        hi = std::fmax(hi, p);
        const float y = map_glows::bob(i * 0.01, 0.4f);
        low = std::fmin(low, y);
        high = std::fmax(high, y);
    }
    CHECK(lo >= 0.6f && lo < 0.7f);
    CHECK(hi > 0.95f && hi <= 1.0f);
    CHECK(low >= -1.2f && high <= 1.2f && high - low > 1.0f);  // world units, about a unit each way
    CHECK(std::fabs(map_glows::pulse(3.0, 0.1f) - map_glows::pulse(3.0, 0.7f)) > 0.01f);  // each its own
}
