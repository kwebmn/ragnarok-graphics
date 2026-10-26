#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

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

// 3.0.7-beta.3 (the user: «сделать как лампы, чтобы от них исходил свет и освещал карту дополнительно ... разным
// светом»): the maps' fireflies as lamps at their places - which effects they are, each its own colour by its place
TEST(fireflies_of_a_map_are_lamps_of_their_own_colours) {
    CHECK(fireflies::isFirefly(45) && fireflies::isFirefly(713) && fireflies::isFirefly(714) &&
          fireflies::isFirefly(946));
    CHECK(!fireflies::isFirefly(47) && !fireflies::isFirefly(693) && !fireflies::isFirefly(0));
    // the same place, the same colour; the places of a map - fireflies' colours, several of them
    float a[3], b[3];
    fireflies::lampColour(120.0f, -35.0f, a);
    fireflies::lampColour(120.0f, -35.0f, b);
    CHECK(a[0] == b[0] && a[1] == b[1] && a[2] == b[2]);
    std::vector<uint32_t> seen;
    for (int i = 0; i < 40; ++i) {
        float c[3];
        fireflies::lampColour(float(i * 37 % 200) - 100.0f, float(i * 53 % 300) - 150.0f, c);
        // a firefly's: green the most or near it, its brightest channel 1, never blue the most
        CHECK(std::max({c[0], c[1], c[2]}) == 1.0f && c[1] >= 0.69f && c[2] < 1.0f);
        const uint32_t key = uint32_t(c[0] * 255.0f) << 16 | uint32_t(c[1] * 255.0f) << 8 | uint32_t(c[2] * 255.0f);
        if (std::find(seen.begin(), seen.end(), key) == seen.end()) seen.push_back(key);
    }
    CHECK(seen.size() >= 4);
    // a lamp's: its reach some three cells, softer than a map's lamp
    CHECK(fireflies::kLampReach >= 20.0f && fireflies::kLampReach <= 40.0f);
    CHECK(fireflies::kLampStrength > 0.12f && fireflies::kLampStrength < 0.48f);
}
