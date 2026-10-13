#include <cmath>

#include "core/scale.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
}  // namespace

// The user's setup: a 1920x1080 window that Windows (175 %) shows as 3360x1890 real pixels.
TEST(scale_users_window_at_175) {
    const ScalePlan p = planScale(1920, 1080, 3360, 1890, 100, true);
    CHECK(near(p.s, 1.75));
    CHECK(near(p.k, 1.75));
    CHECK_EQ(p.targetWidth, 3360);
    CHECK_EQ(p.targetHeight, 1890);
    CHECK(p.picture == (Rect{0, 0, 3360, 1890}));  // r = 1: the target is shown pixel for pixel
}

TEST(scale_full_screen_on_4k) {
    const ScalePlan p = planScale(1920, 1080, 3840, 2160, 100, true);
    CHECK(near(p.k, 2.0));
    CHECK_EQ(p.targetWidth, 3840);
    CHECK_EQ(p.targetHeight, 2160);
}

// Windows at 100 % (or no SetThreadDpiAwarenessContext): the window's real pixels are the game's - as in 0.2.0.
TEST(scale_windows_at_100_percent_draws_at_the_game_size) {
    const ScalePlan p = planScale(1920, 1080, 1920, 1080, 100, true);
    CHECK(near(p.k, 1.0));
    CHECK_EQ(p.targetWidth, 1920);
    CHECK_EQ(p.targetHeight, 1080);
    CHECK(p.picture == (Rect{0, 0, 1920, 1080}));
}

TEST(scale_render_scale_supersamples) {
    const ScalePlan p = planScale(1920, 1080, 3360, 1890, 150, true);
    CHECK(near(p.r, 1.5));
    CHECK(near(p.k, 2.625));
    CHECK_EQ(p.targetWidth, 5040);
    CHECK_EQ(p.targetHeight, 2835);
    CHECK(p.picture == (Rect{0, 0, 3360, 1890}));  // shrunk into the window when shown
}

TEST(scale_off_draws_at_the_game_size) {
    const ScalePlan p = planScale(1920, 1080, 3360, 1890, 150, false);
    CHECK(near(p.k, 1.0));
    CHECK_EQ(p.targetWidth, 1920);
    CHECK_EQ(p.targetHeight, 1080);
}

TEST(scale_never_below_the_game_size) {
    const ScalePlan p = planScale(1920, 1080, 960, 540, 100, true);  // a window smaller than the game
    CHECK(near(p.s, 0.5));
    CHECK(near(p.k, 1.0));
    CHECK_EQ(p.targetWidth, 1920);
    CHECK(p.picture == (Rect{0, 0, 960, 540}));
}

TEST(scale_bars_for_another_shape) {
    const ScalePlan p = planScale(1024, 768, 1920, 1080, 100, true);
    CHECK(p.picture == (Rect{240, 0, 1680, 1080}));
    CHECK(near(p.s, 1.40625));
    CHECK_EQ(p.targetWidth, 1440);
    CHECK_EQ(p.targetHeight, 1080);
}

TEST(scale_target_within_d3d12_limits) {
    const ScalePlan p = planScale(4096, 2160, 16384, 8640, 200, true);  // s 4 and r 2 would make k 8
    CHECK(near(p.k, 4.0));
    CHECK_EQ(p.targetWidth, kMaxTarget);
}

// At 100 % the target is exactly the picture, so it is shown pixel for pixel (spec §3) - also where size * k is a
// half and floating point would round it the other way than fitPicture: 640x480 in 646x485 is 484.5 rows.
TEST(scale_target_is_the_picture_at_100_percent) {
    const ScalePlan p = planScale(640, 480, 646, 485, 100, true);
    CHECK_EQ(p.targetHeight, 485);
    CHECK(p.picture == (Rect{0, 0, 646, 485}));
    const int games[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}};
    int differ = 0;
    for (const auto& g : games)
        for (int w = g[0] + 1; w <= g[0] * 5 / 2; ++w)
            for (int h = g[1] * w / g[0] - 9; h <= g[1] * w / g[0] + 9; ++h) {
                const ScalePlan q = planScale(g[0], g[1], w, h, 100, true);
                if (q.k > 1.0 && (q.targetWidth != q.picture.width() || q.targetHeight != q.picture.height()))
                    ++differ;
            }
    CHECK_EQ(differ, 0);
}

// At any render scale the target is the game's size times k, rounded: the full-screen quad, whose far edges are at
// size * k + 0.5, covers its last row and column (spec §4). round(picture * r) can miss them by a pixel at r > 1.
TEST(scale_target_is_the_game_times_k_rounded) {
    int off = 0;
    for (int percent = 100; percent <= 200; percent += 25)
        for (int w = 1921; w <= 4800; ++w) {
            const ScalePlan q = planScale(1920, 1080, w, 1080 * w / 1920 + 3, percent, true);
            if (std::fabs(q.targetWidth - 1920 * q.k) > 0.5 + 1e-9 ||
                std::fabs(q.targetHeight - 1080 * q.k) > 0.5 + 1e-9)
                ++off;
        }
    CHECK_EQ(off, 0);
}

TEST(scale_rect_edges_follow_the_vertices) {
    CHECK(scaleRect(Rect{0, 0, 1920, 1080}, 1.75) == (Rect{0, 0, 3360, 1890}));
    CHECK(scaleRect(Rect{1, 1, 2, 2}, 1.75) == (Rect{2, 2, 4, 4}));
    CHECK(scaleRect(Rect{3, 3, 4, 4}, 2.0) == (Rect{6, 6, 8, 8}));
    CHECK(scaleRect(Rect{5, 7, 9, 11}, 1.0) == (Rect{5, 7, 9, 11}));
    CHECK(scaleRect(Rect{0, 0, 1920, 1080}, 3840.0 / 1920.0) == (Rect{0, 0, 3840, 2160}));
}

TEST(scale_target_coord) {
    CHECK(near(targetCoord(0.0, 1.75), 0.5));        // the left edge of the screen: first pixel centre
    CHECK(near(targetCoord(1920.0, 1.75), 3360.5));  // the right edge: just past the last one
    CHECK(near(targetCoord(7.0, 1.0), 7.5));         // k = 1: calibration.md §3
}

TEST(sharp_coord_keeps_texels_flat_and_steps_in_one_pixel) {
    CHECK(near(sharpCoord(2.5, 1.75), 2.5));  // a texel centre stays
    CHECK(near(sharpCoord(2.7, 1.75), 2.5));  // inside the texel: snapped to it
    CHECK(near(sharpCoord(3.0, 1.75), 3.0));  // the middle of the step between two texels
    const double half = 0.5 / 1.75;           // the step is one screen pixel = 1 / 1.75 texel wide
    CHECK(near(sharpCoord(3.0 - half, 1.75), 2.5));
    CHECK(near(sharpCoord(3.0 + half, 1.75), 3.5));
    CHECK(near(sharpCoord(2.8, 1.0), 2.8));  // not magnified: unchanged
    CHECK(near(sharpCoord(2.8, 0.5), 2.8));
}
