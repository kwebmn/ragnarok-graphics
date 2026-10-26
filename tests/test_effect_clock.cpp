#include "core/effect_clock.hpp"
#include "test.hpp"

#include <cmath>

using namespace uagfx;

TEST(effect_clock_seconds_never_go_round) {
    // 3.0.6-beta.2 (the RTX 3060's player: «свернув окно, зайшов в браузер, зайшов в гру і все чорно» - the world
    // black an hour after the start, a portal brought it back): Direct3D 12's clock went round its hour, and the ages
    // taken from it on the processor went below nothing
    effect_clock::Clock c;
    CHECK(c.seconds(5000, 1000) == 0.0);                                   // the first reading: its start
    CHECK(c.seconds(5000 + 1200 * 1000, 1000) == 1200.0);
    CHECK(c.seconds(5000 + 3706 * 1000, 1000) == 3706.0);                  // an hour and more on: not round
    CHECK(std::abs(effect_clock::forShaders(3706.5) - 106.5f) < 1e-3f);    // the shaders' an hour round
    CHECK(effect_clock::forShaders(10.25) == 10.25f);
}

TEST(effect_clock_map_fade_shows_the_world_an_hour_on) {
    // 0.83: the world comes in from black in 0.7 s after a new map - and stays shown, whatever the hour
    effect_clock::Clock c;
    c.seconds(0, 1000);
    const double entered = c.seconds(1200 * 1000, 1000);                  // the map entered 20 minutes in
    CHECK(effect_clock::mapFade(entered, entered) == 0.0f);               // black at once
    CHECK(std::abs(effect_clock::mapFade(entered + 0.35, entered) - 0.5f) < 1e-4f);
    CHECK(effect_clock::mapFade(entered + 0.7, entered) == 1.0f);
    CHECK(effect_clock::mapFade(c.seconds(3706 * 1000, 1000), entered) == 1.0f);  // the player's 08:29: shown
    CHECK(effect_clock::mapFade(c.seconds(9000 * 1000, 1000), entered) == 1.0f);
    CHECK(effect_clock::mapFade(10.0, -1.0) == 1.0f);                     // no map yet
}
