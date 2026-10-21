#include <vector>

#include "core/effect_light.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
std::vector<uint32_t> filled(uint32_t argb, int n = 64 * 64) { return std::vector<uint32_t>(size_t(n), argb); }
}  // namespace

TEST(effect_light_knows_fire_by_its_colours) {
    // 0.26: a skill's picture that is fire-coloured lights around it; white, blue, purple and green ones do not.
    float c[3];
    CHECK(effect_light::fireOf(filled(0xFFFF8020).data(), 64, 64, c) > 0.6f);  // orange
    CHECK(c[0] == 1.0f && c[1] > 0.4f && c[1] < 0.6f && c[2] < 0.2f);
    CHECK(effect_light::fireOf(filled(0xFFFF3020).data(), 64, 64, c) > 0.5f);  // red
    CHECK(effect_light::fireOf(filled(0xFFFFE070).data(), 64, 64, c) > 0.2f);  // yellow
    CHECK(effect_light::fireOf(filled(0xFFFFFFFF).data(), 64, 64, c) == 0.0f);  // white: the level 99 aura's
    CHECK(effect_light::fireOf(filled(0xFF4080FF).data(), 64, 64, c) == 0.0f);  // blue: frost
    CHECK(effect_light::fireOf(filled(0xFFC040FF).data(), 64, 64, c) == 0.0f);  // purple
    CHECK(effect_light::fireOf(filled(0xFF40FF40).data(), 64, 64, c) == 0.0f);  // green
    CHECK(effect_light::fireOf(filled(0x00FF8020).data(), 64, 64, c) == 0.0f);  // transparent
    CHECK(effect_light::fireOf(filled(0xFF000000).data(), 64, 64, c) == 0.0f);  // black: adds nothing
    // A fire on black (an additive picture): the black does not dilute it.
    std::vector<uint32_t> spark = filled(0xFF000000);
    for (int y = 24; y < 40; ++y)
        for (int x = 24; x < 40; ++x) spark[size_t(y * 64 + x)] = 0xFFFF7010;
    CHECK(effect_light::fireOf(spark.data(), 64, 64, c) > 0.5f);
    CHECK(effect_light::fireOf(nullptr, 0, 0, c) == 0.0f);
}

TEST(effect_light_skills_light_at_night) {
    // 2.7.20 (the tester: «рувотч не светит» - its blue mist a place of about 0.2, a tenth of a torch's light): at
    // night a place of skills' light lights at least 0.7, the faintest in proportion; by day as it was; never past 1
    CHECK(effect_light::skillWeight(0.2f, 0.0f) == 0.2f);
    CHECK(effect_light::skillWeight(0.2f, 1.0f) >= 0.7f);
    const float half = effect_light::skillWeight(0.2f, 0.5f);
    CHECK(half > 0.2f && half < 0.7f);
    CHECK(effect_light::skillWeight(0.05f, 1.0f) >= 0.7f);  // a fourth of Ruwach's still lights as much
    CHECK(effect_light::skillWeight(0.01f, 1.0f) > 0.01f && effect_light::skillWeight(0.01f, 1.0f) < 0.2f);
    CHECK(effect_light::skillWeight(3.0f, 1.0f) == 1.0f);
    CHECK(effect_light::skillWeight(3.0f, 0.0f) == 1.0f);
}

TEST(effect_light_knows_the_colour_other_skills_light_in) {
    // 0.36: a skill's picture lights in its own colour: ice blue, poison green, holy gold, lightning near white.
    float c[3];
    CHECK(effect_light::lightOf(filled(0xFF4080FF).data(), 64, 64, c) > 0.3f);  // ice
    CHECK(c[2] == 1.0f && c[0] < 0.4f);
    CHECK(effect_light::lightOf(filled(0xFF40FF50).data(), 64, 64, c) > 0.3f);  // poison
    CHECK(c[1] == 1.0f && c[0] < 0.4f);
    CHECK(effect_light::lightOf(filled(0xFFFFE070).data(), 64, 64, c) > 0.3f);  // holy
    CHECK(c[0] == 1.0f && c[1] > 0.8f && c[2] < 0.6f);
    CHECK(effect_light::lightOf(filled(0xFFE8F0FF).data(), 64, 64, c) > 0.2f);  // lightning: bright, a little blue
    CHECK(c[2] == 1.0f && c[0] > 0.85f);
    CHECK(effect_light::lightOf(filled(0xFF808080).data(), 64, 64, c) == 0.0f);  // grey: dust, smoke
    CHECK(effect_light::lightOf(filled(0xFF000000).data(), 64, 64, c) == 0.0f);  // black adds nothing
    CHECK(effect_light::lightOf(filled(0x00FF8020).data(), 64, 64, c) == 0.0f);  // transparent
    CHECK(effect_light::lightOf(filled(0xFF301008).data(), 64, 64, c) == 0.0f);  // too dark to light
    CHECK(effect_light::lightOf(nullptr, 0, 0, c) == 0.0f);
}
