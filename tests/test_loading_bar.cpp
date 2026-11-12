// 3.0.18-beta.180 (the owner «давай» to «what else»): the game's map loading bar (its window 7, 240 x 15 under the
// loading picture) in the new look at its place - the game's own per cent, the words sharp
#include <cmath>
#include <limits>

#include "core/loading_bar.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }
}  // namespace

TEST(loading_bar_at_the_games_place_with_its_per_cent) {
    const loadingbar::Layout L = loadingbar::layout(997, 777, 240, 15, 1.75, 45.6f);
    CHECK(near(L.track.x, 1744.75f) && near(L.track.y, 1359.75f) && near(L.track.w, 420) && near(L.track.h, 26.25f));
    CHECK(L.percent == "45%");  // the game's own «%3ld%%»: its whole per cent, no space
    CHECK(L.fill.x > L.track.x && L.fill.y > L.track.y && L.fill.h > 0 && L.fill.h < L.track.h);
    const float inner = L.track.w - 2 * (L.fill.x - L.track.x);
    CHECK(near(L.fill.w, inner * 0.456f));
    CHECK(near(L.radius, L.track.h / 2) && L.px > 8 && L.px < L.track.h);
}

TEST(loading_bar_per_cent_kept_within_its_bounds) {
    CHECK(loadingbar::layout(0, 0, 240, 15, 1, -5).percent == "0%");
    CHECK(loadingbar::layout(0, 0, 240, 15, 1, -5).fill.w == 0);
    const loadingbar::Layout full = loadingbar::layout(0, 0, 240, 15, 1, 250);
    CHECK(full.percent == "100%" && near(full.fill.x + full.fill.w, full.track.w - (full.fill.x - full.track.x)));
    CHECK(loadingbar::layout(0, 0, 240, 15, 1, std::numeric_limits<float>::quiet_NaN()).percent == "0%");
    CHECK(loadingbar::layout(0, 0, 240, 15, 0, 50).track.w == 240);  // no scale known: the game's pixels
}
