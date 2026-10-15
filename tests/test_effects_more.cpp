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
