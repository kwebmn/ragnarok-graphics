#include <algorithm>
#include <vector>

#include "core/scale.hpp"
#include "core/twin_grid.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// Windows scaling 125-262 %, a 2560 x 1440 monitor for a 1920 x 1080 game, odd ratios.
const double kScales[] = {1.25, 1.5, 1.75, 2.0, 2.625, 4.0 / 3.0, 1.37, 2560.0 / 1920.0 * 1.3};
}  // namespace

TEST(twin_grid_blocks_at_1_75) {
    // Game pixels 0..3 at K = 1.75: blocks [0,1) [1,3) [3,5) [5,7) - one, two, two, two pixels.
    CHECK_EQ(twinEdge(0, 1.75), 0);
    CHECK_EQ(twinEdge(1, 1.75), 1);
    CHECK_EQ(twinEdge(2, 1.75), 3);
    CHECK_EQ(twinEdge(3, 1.75), 5);
    CHECK_EQ(twinEdge(4, 1.75), 7);
    CHECK_EQ(twinEdge(-3, 1.75), -6);  // floor(-5.25): lists scroll by drawing at negative x
}

TEST(twin_grid_every_block_has_a_pixel) {
    for (const double K : kScales)
        for (int g = -50; g < 700; ++g) CHECK(twinEdge(g + 1, K) - twinEdge(g, K) >= 1);
}

TEST(twin_grid_ceil_edge_is_the_stage_3_coverage_rule) {
    for (const double k : kScales)
        for (int x = -40; x < 2000; x += 7) CHECK_EQ(ceilEdge(x, k), scaleRect(Rect{x, 0, x + 1, 1}, k).left);
}

TEST(twin_grid_tile_places_follow_the_game) {
    const TilePlace a = tilePlace(0, 300, 120);
    const TilePlace b = tilePlace(1, 300, 120);
    CHECK(a.x == 0 && a.y == 0 && a.width == 256 && a.height == 120);
    CHECK(b.x == 256 && b.y == 0 && b.width == 44 && b.height == 120);
    const TilePlace c = tilePlace(4, 600, 600);
    const TilePlace d = tilePlace(8, 600, 600);
    CHECK(c.x == 256 && c.y == 256 && c.width == 256 && c.height == 256);
    CHECK(d.x == 512 && d.y == 512 && d.width == 88 && d.height == 88);
    CHECK_EQ(tilePlace(1, 256, 256).width, 0);  // there is no second tile
    CHECK_EQ(tilePlace(2, 300, 120).width, 0);
    CHECK_EQ(tilePlace(9, 600, 600).height, 0);
}

TEST(twin_grid_a_twin_matches_only_its_own_scale_above_1) {
    CHECK(twinMatches(1.75, 1.75));
    CHECK(!twinMatches(1.75, 1.0));  // Pause: the picture at the game's size
    CHECK(!twinMatches(1.0, 1.0));
    CHECK(!twinMatches(0.0, 1.0));
}

// Spec §3 with a reference rasterizer on one axis: a window w wide at game x0, cut into the game's tiles, drawn at
// k. Every target pixel of every tile quad takes one twin pixel from its tile's texture, and it is the one the whole
// window shows there: no seams, nothing past a texture but the clamp of the last tile. Every twin pixel - every game
// pixel's block - is shown, and each block lands within a target pixel of its game pixel.
TEST(twin_grid_every_target_pixel_shows_one_twin_pixel) {
    for (const double k : kScales)
        for (const int w : {1, 2, 3, 7, 255, 256, 257, 300, 513, 600})
            for (int x0 = -40; x0 < 40; ++x0) {
                const int twin = twinEdge(w, k);
                const int origin = ceilEdge(x0, k);
                std::vector<bool> shown(size_t(twin), false);
                for (int i = 0;; ++i) {
                    const TilePlace t = tilePlace(i, w, 1);
                    if (t.width == 0) break;
                    const TwinSpan span = tileSpan(t.x, t.width, twin, k);
                    const int size = span.end - span.first;
                    const bool last = t.x + t.width == w;
                    for (int p = ceilEdge(x0 + t.x, k); p < ceilEdge(x0 + t.x + t.width, k); ++p) {
                        const int texel = p - tileOrigin(x0, t.x, k, k);
                        CHECK(texel >= 0);
                        if (!last) CHECK(texel < size);  // only the last tile repeats its last column
                        const int column = span.first + std::clamp(texel, 0, size - 1);
                        CHECK_EQ(column, std::min(p - origin, twin - 1));  // the window's own mapping: no seam
                        shown[size_t(column)] = true;
                    }
                }
                CHECK(std::all_of(shown.begin(), shown.end(), [](bool b) { return b; }));
                for (int g = 0; g < w; ++g) {
                    const int shift = origin + twinEdge(g, k) - ceilEdge(x0 + g, k);
                    CHECK(shift >= -1 && shift <= 0);
                }
            }
}
