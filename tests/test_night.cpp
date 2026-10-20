#include "core/config.hpp"
#include "core/daylight.hpp"
#include "core/effect_groups.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(night_always_by_the_clock) {
    // 2.6.19, «Часы» the master (the user's choice): the night goes by the clock always - dusk from 20:00, deep from
    // 21:00 to 5:00, dawn to 6:00 - at its strength alone (effects.night_strength; 0: none)
    EffectSettings e;
    e.nightStrength = 60;
    CHECK(nightAmount(e, 12, 0) == 0);
    CHECK(nightAmount(e, 23, 30) == 60);
    CHECK(nightAmount(e, 3, 0) == 60);
    CHECK(nightAmount(e, 20, 30) == 30);  // dusk
    CHECK(nightAmount(e, 5, 30) == 30);   // dawn
    CHECK(nightAmount(e, 6, 0) == 0);
    e.nightStrength = 0;
    CHECK(nightAmount(e, 23, 30) == 0);
    CHECK(effectsOff().nightNow == 0);
}

TEST(night_and_day_follow_the_chosen_clock) {
    // 2.6.11: the minute the effects go by - the day's sun, the night, the creatures, the fog - is the chosen
    // moment when there is one, the computer's otherwise
    EffectSettings e;
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
    e.clock = daylight::kClockServer;  // 2.6.19: the server's word, as the server clock makes it a minute
    applyClock(e, 13 * 60, 0);
    CHECK_EQ(e.dayNow, 0);
    CHECK_EQ(e.nightNow, 60);
    applyClock(e, 23 * 60, -1);  // nothing heard from the server: day
    CHECK_EQ(e.dayNow, 13 * 60);
    CHECK_EQ(e.nightNow, 0);
}

TEST(night_in_old_files_goes_by_the_clock_too) {
    // 2.6.19: an old file's night and night_clock switches are not read - the night is the clock's, at 60 %
    EffectSettings e = parseConfig("[effects]\nnight = 0\nnight_clock = 0\n").effects;
    applyClock(e, 23 * 60);
    CHECK_EQ(e.nightNow, 60);
}

TEST(night_none_with_the_effects_set_off) {
    // «Якість: Вимк.» - the game's own picture: no night either
    EffectSettings e = effect_groups::effective(parseConfig("[effects]\npreset = off\n").effects, 3);
    applyClock(e, 23 * 60);
    CHECK_EQ(e.nightNow, 0);
    e = effect_groups::effective(parseConfig("[effects]\npreset = weak\n").effects, 3);
    applyClock(e, 23 * 60);
    CHECK_EQ(e.nightNow, 60);
}
