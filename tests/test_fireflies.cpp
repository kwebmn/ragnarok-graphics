#include <cmath>

#include "core/fireflies.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(fireflies_flash_now_and_then) {
    // 0.28: a short flash every two or three seconds, a faint glow between them; each its own rhythm.
    float lo = 1.0f, hi = 0.0f;
    int flashes = 0;
    bool on = false;
    for (int i = 0; i < 2000; ++i) {  // 20 seconds
        const float b = fireflies::blink(i * 0.01, 0.3f);
        lo = std::fmin(lo, b);
        hi = std::fmax(hi, b);
        if (b > 0.8f && !on) ++flashes;
        on = b > 0.8f;
    }
    CHECK(lo >= 0.15f && lo < 0.25f);
    CHECK(hi > 0.95f && hi <= 1.0f);
    CHECK(flashes >= 6 && flashes <= 12);
    // Another seed flashes at other moments.
    int apart = 0;
    for (int i = 0; i < 2000; ++i)
        apart += std::fabs(fireflies::blink(i * 0.01, 0.3f) - fireflies::blink(i * 0.01, 0.8f)) > 0.3f;
    CHECK(apart > 200);
}

TEST(fireflies_of_the_map_file_wander_near_their_place) {
    // The map's own fireflies (713, 714, 946 - the game does not show them) drift about their place, a little above.
    float far = 0.0f, moved = 0.0f;
    map::Point last = fireflies::wander(0.0, 0.42f);
    for (int i = 1; i < 3000; ++i) {
        const map::Point p = fireflies::wander(i * 0.02, 0.42f);
        far = std::fmax(far, std::hypot(p.x, p.z));
        CHECK(p.y <= -1.0f && p.y >= -9.0f);  // up is -y in the game's world
        moved += std::hypot(p.x - last.x, p.z - last.z);
        last = p;
    }
    CHECK(far <= 6.0f && far > 2.0f);
    CHECK(moved > 20.0f);  // it does move
    const map::Point a = fireflies::wander(5.0, 0.1f), b = fireflies::wander(5.0, 0.9f);
    CHECK(std::hypot(a.x - b.x, a.z - b.z) > 0.1f);  // each its own way
}

TEST(fireflies_keep_the_games_colour_or_glow_yellow_green) {
    float c[3];
    fireflies::colourOf(0xC02878C8, c);  // the game's own blue: kept, its brightest channel 1
    CHECK(c[2] == 1.0f && c[1] > 0.55f && c[1] < 0.65f && c[0] < 0.25f);
    fireflies::colourOf(0xFFF0F0F0, c);  // grey or white: a firefly's yellow-green
    CHECK(c[1] == 1.0f && c[0] > 0.6f && c[2] < 0.5f);
    fireflies::colourOf(0x80000000, c);  // black: the same
    CHECK(c[1] == 1.0f);
}
