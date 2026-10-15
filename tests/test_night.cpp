#include "core/config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(night_by_its_switch_and_the_clock) {
    // 0.63: night - its strength, by the clock only at night with an hour of dusk and of dawn.
    EffectSettings e;
    CHECK(nightAmount(e, 23, 0) == 0);  // off
    e.night = true;
    e.nightStrength = 60;
    CHECK(nightAmount(e, 12, 0) == 60);  // always
    e.nightClock = true;
    CHECK(nightAmount(e, 12, 0) == 0);
    CHECK(nightAmount(e, 23, 30) == 60);
    CHECK(nightAmount(e, 3, 0) == 60);
    CHECK(nightAmount(e, 20, 30) == 30);  // dusk
    CHECK(nightAmount(e, 5, 30) == 30);   // dawn
    CHECK(nightAmount(e, 6, 0) == 0);
    CHECK(!effectsOff().night && effectsOff().nightNow == 0);
}
