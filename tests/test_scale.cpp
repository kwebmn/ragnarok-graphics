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

TEST(ui_taps_sharp_at_1_75_blocks_with_a_one_pixel_blend) {
    // Target pixel p belongs to game pixel floor(p / k); where a game pixel edge crosses it, the part beyond the
    // edge shows the next game pixel. Game pixels 0-3 of a wide quad cover target pixels 0-1, 2-3, 4-5, 6.
    const int g[7] = {0, 0, 1, 1, 2, 2, 3};
    const double w[7] = {0.0, 0.25, 0.0, 0.5, 0.0, 0.75, 0.0};
    for (int p = 0; p < 7; ++p) {
        const UiTaps t = uiTaps(p, 1.75, 0, 9, true);
        CHECK_EQ(t.g0, g[p]);
        CHECK_EQ(t.g1, g[p] + 1);
        CHECK(near(t.w, w[p]));
    }
}

TEST(ui_taps_never_leave_the_quad) {
    // A quad over game pixels 2-3: target pixel 6 belongs to game pixel 3, its last; nothing of game pixel 4.
    const UiTaps last = uiTaps(6, 1.75, 2, 3, true);
    CHECK_EQ(last.g0, 3);
    CHECK_EQ(last.g1, 3);
    const UiTaps left = uiTaps(4, 2.0, 2, 3, false);  // smooth: the first pixel would blend with game pixel 1
    CHECK_EQ(left.g0, 2);
    CHECK_EQ(left.g1, 2);
    const UiTaps right = uiTaps(7, 2.0, 2, 3, false);
    CHECK_EQ(right.g0, 3);
    CHECK_EQ(right.g1, 3);
}

TEST(ui_taps_smooth_at_2_is_bilinear_between_game_pixel_centres) {
    const UiTaps a = uiTaps(5, 2.0, 0, 9, false);  // centre 5.5 = game 2.75: between game pixels 2 and 3
    CHECK_EQ(a.g0, 2);
    CHECK_EQ(a.g1, 3);
    CHECK(near(a.w, 0.25));
    const UiTaps b = uiTaps(6, 2.0, 0, 9, false);
    CHECK_EQ(b.g0, 2);
    CHECK(near(b.w, 0.75));
}

TEST(ui_taps_at_1_are_the_pixel_itself) {
    for (const bool sharp : {true, false})
        for (int p = 0; p < 5; ++p) {
            const UiTaps t = uiTaps(p, 1.0, 0, 9, sharp);
            CHECK_EQ(t.g0, p);
            CHECK(near(t.w, 0.0));
        }
}

TEST(ui_filter_mode_only_above_scale_1) {  // review I2: high_resolution = 0 and 100 % must be exactly 0.2.0
    CHECK_EQ(uiFilterMode(true, 1.0, UiFilter::Sharp), 0);
    CHECK_EQ(uiFilterMode(true, 1.0, UiFilter::Smooth), 0);
    CHECK_EQ(uiFilterMode(true, 1.75, UiFilter::Sharp), 1);
    CHECK_EQ(uiFilterMode(true, 1.75, UiFilter::Smooth), 2);
    CHECK_EQ(uiFilterMode(false, 1.75, UiFilter::Sharp), 0);  // the world keeps its sampling
}

TEST(scale_under_100_draws_smaller_than_the_picture) {
    // 0.58: render_scale under 100 % - the world smaller than the 4K picture, made larger sharply when shown; never
    // below the game's own size.
    const ScalePlan p = planScale(1920, 1080, 3840, 2160, 75, true);
    CHECK(p.targetWidth == 2880 && p.targetHeight == 1620);
    const ScalePlan full = planScale(1920, 1080, 1920, 1080, 50, true);  // a 1080p screen: the game's size
    CHECK(full.targetWidth == 1920 && full.k == 1.0);
    CHECK(planScale(1920, 1080, 3840, 2160, 10, true).targetWidth == 1920);  // 50 % at most: 4K / 2
}

TEST(scale_upscaler_samples_textures_for_the_pictures_pixels) {
    // 2.2.4: under DLSS the world is drawn smaller than the picture and its textures' mip levels were picked for the
    // world's pixels - the texture pack's pictures (mip levels of their own) came out blurred on a 4K screen, the
    // game's (none) not; lowered by log2(world / picture), never raised
    CHECK(std::fabs(upscaleMipBias(2559.0 / 1920.0, 2.0) - std::log2(2559.0 / 3840.0)) < 1e-4);  // Quality, 4K
    CHECK(std::fabs(upscaleMipBias(1.0, 2.0) + 1.0f) < 1e-6);                                    // Performance
    CHECK_EQ(upscaleMipBias(2.0, 2.0), 0.0f);  // DLAA: the picture's own size
    CHECK_EQ(upscaleMipBias(3.0, 2.0), 0.0f);  // drawn larger: the levels as they are
    CHECK_EQ(upscaleMipBias(0.0, 2.0), 0.0f);
    CHECK_EQ(upscaleMipBias(1.5, 0.0), 0.0f);
}
