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
