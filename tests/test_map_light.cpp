#include <cmath>

#include "core/config.hpp"
#include "core/effect_groups.hpp"
#include "core/map_light.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(map_light_a_weak_light_as_the_game_adds_it) {
    // 2.7.32: the maps' own tint all over (their coloured light up to 0.3) added as the game adds it - the map no
    // darker anywhere it was not blown out
    CHECK(std::fabs(map_light::soft(0.5f, 0.2f) - 0.7f) < 1e-6f);
    CHECK(std::fabs(map_light::soft(0.3f, 0.0f) - 0.3f) < 1e-6f);
}

TEST(map_light_a_strong_light_keeps_the_stones) {
    // 2.7.32 (the tester in Prontera's sewers: «стены от огня очень гладкие» - the game's own, as Pause showed): by a
    // torch the game added its light and cut at white - every shade of the wall one yellow; now never past white, and
    // a lighter stone stays lighter than a duller one
    CHECK(map_light::soft(0.6f, 0.95f) < 1.0f);
    CHECK(map_light::soft(0.5f, 0.9f) - map_light::soft(0.4f, 0.9f) > 0.01f);
    CHECK(map_light::soft(0.5f, 0.9f) > 0.95f);  // still bright
}

TEST(map_light_a_brighter_light_never_darker) {
    // 2.7.32: across a light's patch (its light rising towards the torch) no ring where it dims - for the textures up
    // to 0.7
    for (float b = 0.0f; b <= 0.7f; b += 0.05f) {
        float before = map_light::soft(b, 0.0f);
        for (float l = 0.01f; l <= 1.0f; l += 0.01f) {
            const float now = map_light::soft(b, l);
            CHECK(now >= before - 1e-6f);
            before = now;
        }
    }
}

TEST(map_light_soft_its_own_switch_among_the_fixes) {
    // 2.7.32: on unless turned off, with the game's fixes, off with the effects
    CHECK(parseConfig("").effects.softMapLight);
    CHECK(!parseConfig("[effects]\nsoft_map_light = 0\n").effects.softMapLight);
    CHECK(!effectsOff().softMapLight);
    const effect_groups::Member* m = effect_groups::memberOf("effects.soft_map_light");
    CHECK(m && m->group == effect_groups::GameFixes);
}
