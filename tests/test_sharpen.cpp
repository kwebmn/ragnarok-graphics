#include <algorithm>
#include <cmath>
#include <random>

#include "core/sharpen.hpp"
#include "core/toggle_key.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(double a, double b) { return std::fabs(a - b) < 1e-12; }
}  // namespace

TEST(sharpen_leaves_flat_and_linear_areas) {
    for (const double s : {0.0, 0.5, 1.0}) {
        CHECK(near(casSharpen(0.4, 0.4, 0.4, 0.4, 0.4, s), 0.4));  // flat
        CHECK(near(casSharpen(0.5, 0.5, 0.5, 0.2, 0.8, s), 0.5));  // a linear ramp has nothing to sharpen
    }
}

TEST(sharpen_steepens_a_soft_edge) {
    // A soft dark-to-light edge: the pixel on its dark side gets darker, more so with more sharpness.
    const double mild = casSharpen(0.3, 0.3, 0.3, 0.2, 0.5, 0.0);
    const double strong = casSharpen(0.3, 0.3, 0.3, 0.2, 0.5, 1.0);
    CHECK(mild < 0.3);
    CHECK(strong < mild);
    CHECK(strong >= 0.2);
}

TEST(sharpen_never_leaves_the_neighbourhood) {  // no halos: the result stays within its five pixels' range
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> v(0.0, 1.0);
    int outside = 0;
    for (int i = 0; i < 20000; ++i) {
        const double c = v(rng), n = v(rng), s = v(rng), w = v(rng), e = v(rng);
        const double r = casSharpen(c, n, s, w, e, v(rng));
        if (r < std::min({c, n, s, w, e}) - 1e-12 || r > std::max({c, n, s, w, e}) + 1e-12) ++outside;
    }
    CHECK_EQ(outside, 0);
}

TEST(toggle_key_fires_once_per_press) {
    ToggleKey k;
    CHECK(!k.pressed(false));
    CHECK(k.pressed(true));    // pressed
    CHECK(!k.pressed(true));   // still held over the next frames
    CHECK(!k.pressed(true));
    CHECK(!k.pressed(false));  // released
    CHECK(k.pressed(true));    // pressed again
}
