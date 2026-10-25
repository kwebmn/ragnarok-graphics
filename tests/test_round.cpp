#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/round.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.5-beta.9 (R-VMPQKX: lroundf a fifth of a Radeon 610M laptop's game thread): the library's answers, every one
TEST(round_away_answers_as_lround_for_floats) {
    int wrong = 0;
    for (int i = -2000000; i <= 2000000; ++i) {
        const float v = float(i) / 1024.0f;
        if (roundAway(v) != std::lround(v)) ++wrong;
    }
    const float edges[] = {0.49999997f, -0.49999997f, 0.5f, -0.5f, 1.5f, -1.5f, 2.5f, -2.5f, 8388607.5f,
                           -8388607.5f, 16777215.0f, -16777215.0f, 1e9f, -1e9f, 0.0f, -0.0f, 1e-30f, 254.5f, 126.99f};
    for (float v : edges)
        if (roundAway(v) != std::lround(v)) ++wrong;
    uint32_t seed = 11;
    for (int k = 0; k < 1000000; ++k) {  // any float of a magnitude under 2^30
        seed = seed * 1664525u + 1013904223u;
        float v;
        uint32_t bits = seed & 0xCFFFFFFFu;  // the exponent kept under 2^30
        std::memcpy(&v, &bits, 4);
        if (!std::isfinite(v)) continue;
        if (roundAway(v) != std::lround(v)) ++wrong;
    }
    CHECK_EQ(wrong, 0);
}

TEST(round_away_answers_as_lround_for_doubles_and_round_for_floats) {
    int wrong = 0;
    for (int i = -400000; i <= 400000; ++i) {
        const double d = double(i) / 2048.0 + 1e-9 * double(i % 7);
        if (roundAway(d) != std::lround(d)) ++wrong;
        const float f = float(i) / 4096.0f;
        if (roundAwayF(f) != std::round(f)) ++wrong;
    }
    CHECK(roundAway(0.49999999999999994) == 0 && roundAway(-2.5) == -3 && roundAway(2.5) == 3);
    CHECK_EQ(wrong, 0);
}

TEST(floor_and_ceil_int_answer_as_the_library) {
    int wrong = 0;
    for (int i = -3000000; i <= 3000000; ++i) {
        const float v = float(i) / 997.0f;
        if (floorInt(v) != int(std::floor(v)) || ceilInt(v) != int(std::ceil(v))) ++wrong;
    }
    const float edges[] = {-0.0f, 0.0f, -1.0f, 1.0f, -0.99999994f, 0.99999994f, -1e-30f, 1e-30f, 8388607.5f,
                           -8388607.5f, 16777216.0f, -16777216.0f, 2e9f, -2e9f};
    for (float v : edges)
        if (floorInt(v) != int(std::floor(v)) || ceilInt(v) != int(std::ceil(v))) ++wrong;
    CHECK_EQ(wrong, 0);
}
