#include "core/config.hpp"
#include "core/daylight.hpp"
#include "core/effect_groups.hpp"
#include "core/settings/schema.hpp"
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
    // 2.6.11: the minute the effects go by - the day's sun, the night, the creatures, the dew, the fog - is the chosen
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

TEST(deep_night_near_black_whatever_the_strength) {
    // 2.7.16 (the user: «ночь какая-то светлая, возможно надо ввести ещё одну ночь, когда вообще темно» - chose a
    // choice of «Годинник» of its own): «Глибока ніч» at 3:00, past the strength's whole night (night_ps darkens
    // further above 100) - but none where the night is off (0 %: the effects' set «off» makes it so)
    EffectSettings e;
    e.nightStrength = 30;
    e.clock = daylight::kClockDeepNight;
    applyClock(e, 13 * 60);  // the computer says noon
    CHECK_EQ(e.dayNow, 3 * 60);
    CHECK_EQ(e.nightNow, 200);
    e.nightStrength = 100;
    applyClock(e, 13 * 60);
    CHECK_EQ(e.nightNow, 200);
    e.nightStrength = 0;
    applyClock(e, 13 * 60);
    CHECK_EQ(e.nightNow, 0);
    const Config deep = parseConfig("[effects]\nclock = deep_night\n");
    CHECK(deep.effects.clock == daylight::kClockDeepNight);
    CHECK(settings::tokenOf(*settings::findSetting("effects.clock"), deep) == "deep_night");
}

TEST(night_lights_moon_and_hero_on_by_default) {
    // 2.7.18 (the user: «очень темно ночью, надо продумать какой-то источник освещения типа луна светит ... и такие
    // вещи как светлячки, скилы должны освещать»; chose «Луна и свет героя»): the moon lifts the night to a
    // silver-blue light, a warm pool lies about the hero, the fireflies in the grass light the ground under them
    const Config c = parseConfig("");
    CHECK(c.effects.moonLight && c.effects.moonStrength == 100);
    CHECK(c.effects.heroLight && c.effects.heroLightStrength == 100);
    CHECK(c.effects.firefliesLight);
    const Config off = parseConfig("[effects]\nmoon_light = 0\nhero_light = 0\nmoon_strength = 150\n"
                                   "hero_light_strength = 40\nfireflies_light = 0\n");
    CHECK(!off.effects.moonLight && !off.effects.heroLight && !off.effects.firefliesLight);
    CHECK(off.effects.moonStrength == 150 && off.effects.heroLightStrength == 40);
    CHECK(!effectsOff().moonLight && !effectsOff().heroLight);
    const effect_groups::Member* moon = effect_groups::memberOf("effects.moon_light");
    const effect_groups::Member* hero = effect_groups::memberOf("effects.hero_light");
    CHECK(moon && moon->group == effect_groups::Time && moon->level == 1);
    CHECK(hero && hero->group == effect_groups::Time && hero->level == 1);
}
