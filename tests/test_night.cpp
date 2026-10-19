#include "core/config.hpp"
#include "core/daylight.hpp"
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

TEST(night_and_day_follow_the_chosen_clock) {
    // 2.6.11: the minute the effects go by - the day's sun, the night by the clock, the creatures, the dew, the fog -
    // is the chosen moment when there is one, the computer's otherwise
    EffectSettings e;
    e.night = e.nightClock = true;
    e.nightStrength = 60;
    e.clock = daylight::kClockNight;
    applyClock(e, 13 * 60);  // the computer says noon
    CHECK_EQ(e.dayNow, 0);
    CHECK_EQ(e.nightNow, 60);
    e.clock = daylight::kClockDay;
    applyClock(e, 23 * 60);  // the computer says night
    CHECK_EQ(e.dayNow, 13 * 60);
    CHECK_EQ(e.nightNow, 0);
    e.clock = daylight::kClockAuto;
    applyClock(e, 23 * 60);
    CHECK_EQ(e.dayNow, 23 * 60);
    CHECK_EQ(e.nightNow, 60);
    e.night = false;  // no night effect: no night, whatever the hour
    applyClock(e, 23 * 60);
    CHECK_EQ(e.nightNow, 0);
}

TEST(a_chosen_moment_rules_the_night_even_without_the_clock_switch) {
    // 2.6.11: night on without "night by the clock" is night always - but a moment the player chose says which
    // hour it is: day is day, night is night; with the computer's clock it stays night always
    EffectSettings e;
    e.night = true;
    e.nightClock = false;
    e.nightStrength = 60;
    e.clock = daylight::kClockDay;
    applyClock(e, 23 * 60);
    CHECK_EQ(e.nightNow, 0);
    e.clock = daylight::kClockNight;
    applyClock(e, 13 * 60);
    CHECK_EQ(e.nightNow, 60);
    e.clock = daylight::kClockAuto;
    applyClock(e, 13 * 60);
    CHECK_EQ(e.nightNow, 60);  // always, as before
}
