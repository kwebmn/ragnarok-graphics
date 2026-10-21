#include <cmath>
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

TEST(effect_light_knows_the_orbs_of_light) {
    // 2.7.21 (the tester: «рувотч не светит»): the game lays its orbs of light over the world as sprites, not added,
    // so the skills' light never saw them - Ruwach's (33: 이팩트\particle2.spr round the caster) and Sight's (22: its
    // fireball) light by their effect's number, Ruwach's pale blue, Sight's fire
    float c[3] = {0, 0, 0};
    CHECK(effect_light::orbOf(33, c));
    CHECK(c[2] == 1.0f && c[0] < 0.9f && c[0] > 0.5f);
    CHECK(effect_light::orbOf(22, c));
    CHECK(c[0] == 1.0f && c[2] < 0.5f);
    CHECK(!effect_light::orbOf(0, c));
    CHECK(!effect_light::orbOf(45, c));  // the maps' fireflies light as fireflies
    CHECK(!effect_light::orbOf(200, c));  // nor the level 99 aura
}

TEST(effect_light_orbs_light_calmly) {
    // 2.7.22 (the user on 2.7.21's Ruwach: «мне кажется слишком ярко ... чтобы было спокойнее»): an orb's light a
    // little weaker (0.5 at night, 2.7.21's 0.6), 15 units over its draws' middle - over the caster's head, no hot
    // spot on the ground where the orb flies - reaching 50; by day half; a faint place in proportion
    const effect_light::OrbLight night = effect_light::orbLight(2.65f, 1.0f, 1.0f);
    CHECK(std::fabs(night.strength - 0.5f) < 1e-6f);
    CHECK(night.height == 15.0f && night.reach == 50.0f);
    CHECK(std::fabs(effect_light::orbLight(2.65f, 0.0f, 1.0f).strength - 0.25f) < 1e-6f);
    CHECK(std::fabs(effect_light::orbLight(0.3f, 1.0f, 1.0f).strength - 0.15f) < 1e-6f);
    CHECK(std::fabs(effect_light::orbLight(2.65f, 1.0f, 0.5f).strength - 0.25f) < 1e-6f);  // «Fire of skills» 50 %
}

TEST(effect_light_gathers_a_casters_orbs_into_one_light) {
    // 2.7.22: Ruwach's orbs and their shadows round its caster one light, wherever the caster stands - 2.7.21's cells
    // of 80 pixels split the orbit into two lights that jumped as it flew («in 2 places»); another caster its own
    struct Spark {
        float view[3];
        float strength;
        float colour[3];
        float gameX, gameY;
        bool orb;
    };
    std::vector<Spark> sparks;
    for (int i = 0; i < 8; ++i) {  // the orbs round a caster at 400, 300 (the edge of 80 pixels' cells between)
        const float a = float(i) * 0.785f;
        sparks.push_back(Spark{{0, 0, 100}, 0.3f, {0.6f, 0.8f, 1.0f}, 400 + 60 * std::cos(a), 300 + 40 * std::sin(a),
                               true});
    }
    sparks.push_back(Spark{{0, 0, 100}, 0.2f, {0.6f, 0.8f, 1.0f}, 1200, 300, true});  // another caster
    sparks.push_back(Spark{{0, 0, 100}, 0.9f, {1, 0.5f, 0.2f}, 405, 300, false});      // a skill's fire, not an orb
    const std::vector<effect_light::OrbPlace> places = effect_light::gatherOrbs(sparks, 160.0f);
    CHECK_EQ(places.size(), size_t(2));
    CHECK(std::fabs(places[0].w - 2.4f) < 1e-4f);
    CHECK(std::fabs(places[0].gameX / places[0].w - 400.0f) < 1.0f);
    CHECK(std::fabs(places[1].w - 0.2f) < 1e-6f);
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
