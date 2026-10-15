#include "core/config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(soft_particles_on_by_default_and_by_the_file) {
    // 0.67: the effects fade at the ground - on unless the file turns it off; Pause turns it off.
    CHECK(parseConfig("").effects.softParticles);
    CHECK(!parseConfig("[effects]\nsoft_particles = 0\n").effects.softParticles);
    CHECK(!effectsOff().softParticles);
    CHECK(drawnByRenderer(parseConfig("").effects));
}

TEST(rim_light_on_by_default_its_strength_bounded) {
    // 0.68: the edge of a character towards a fire lit more.
    const Config c = parseConfig("[effects]\nrim_strength = 150\n");
    CHECK(c.effects.rimLight && c.effects.rimStrength == 150);
    CHECK(parseConfig("[effects]\nrim_strength = 900\n").effects.rimStrength == 100);  // out of range: the default
    CHECK(!effectsOff().rimLight);
}

TEST(sky_on_by_default_with_its_clouds) {
    // 0.69: the sky past the map's edge.
    const Config c = parseConfig("[effects]\nsky_clouds = 30\n");
    CHECK(c.effects.sky && c.effects.skyClouds == 30 && c.effects.skyStrength == 100);
    CHECK(!parseConfig("[effects]\nsky = 0\n").effects.sky);
    CHECK(!effectsOff().sky);
}

TEST(motes_on_by_default_their_amount_bounded) {
    // 0.70: pollen and dust in the air.
    CHECK(parseConfig("").effects.motes && parseConfig("").effects.motesAmount == 100);
    CHECK(parseConfig("[effects]\nmotes_amount = 40\n").effects.motesAmount == 40);
    CHECK(!effectsOff().motes);
}

TEST(relief_on_by_default_its_strength_bounded) {
    // 0.71: the ground's relief from its picture.
    CHECK(parseConfig("").effects.relief && parseConfig("").effects.reliefStrength == 100);
    CHECK(parseConfig("[effects]\nrelief_strength = 0\n").effects.reliefStrength == 0);
    CHECK(!effectsOff().relief);
}
