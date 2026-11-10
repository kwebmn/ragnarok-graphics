#include <cmath>

#include "core/ui_glide.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.58: a bar glides to its value with the motion on, jumps with it off
TEST(ui_glide_eases_a_bar_to_its_value) {
    uiglide::Glide g;
    CHECK(g.step(80, 1000, true) == 80.0f);  // the first time: at once
    CHECK(g.step(40, 1000, true) == 80.0f);  // no time gone: still there
    const float a = g.step(40, 1050, true);
    CHECK(a < 80 && a > 40);
    CHECK(std::fabs(a - (40 + 40 * std::exp(-1.0f))) < 0.01f);
    CHECK(g.moving(40));
    float v = a;
    for (uint64_t t = 1066; t <= 1600; t += 16) v = g.step(40, t, true);
    CHECK(v == 40.0f && !g.moving(40));
    CHECK(g.step(10, 1616, false) == 10.0f);  // off: at once
}
