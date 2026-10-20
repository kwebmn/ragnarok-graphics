#include <cstdio>
#include <string>

#include "core/config.hpp"
#include "core/daylight.hpp"
#include "core/effect_groups.hpp"
#include "core/live_config.hpp"
#include "core/outdoors.hpp"
#include "core/seasons.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/schema.hpp"
#include "core/weather.hpp"
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
    // 0.70: pollen and dust in the air. 2.6.8: twice as many by default - at 100 % they were all but unseen over
    // Prontera's white paving (R-UT7AH3), and the pass costs 0.09 ms on an RTX 5060 Ti
    CHECK(parseConfig("").effects.motes && parseConfig("").effects.motesAmount == 200);
    CHECK(parseConfig("[effects]\nmotes_amount = 40\n").effects.motesAmount == 40);
    CHECK(!effectsOff().motes);
}

TEST(relief_on_by_default_its_strength_bounded) {
    // 0.71: the ground's relief from its picture; 2.6.8: at 30 % (100 turned the stones' tops gritty).
    CHECK(parseConfig("").effects.relief && parseConfig("").effects.reliefStrength == 30);
    CHECK(parseConfig("[effects]\nrelief_strength = 0\n").effects.reliefStrength == 0);
    CHECK(!effectsOff().relief);
}

TEST(floor_reflections_on_by_default_their_shine_bounded) {
    // 0.72: the floors reflect, indoors and in the rain.
    CHECK(parseConfig("").effects.floorReflections && parseConfig("").effects.floorReflectStrength == 35);
    CHECK(parseConfig("[effects]\nfloor_reflect_strength = 101\n").effects.floorReflectStrength == 35);
    CHECK(!effectsOff().floorReflections);
}

TEST(light_in_the_air_off_by_default) {
    // 0.73: the air glows about the lights, rays from the sky; 0.84: off unless asked.
    CHECK(!parseConfig("").effects.shafts && parseConfig("").effects.shaftsStrength == 100);
    CHECK(parseConfig("[effects]\nshafts = 1\n").effects.shafts);
    CHECK(!effectsOff().shafts);
}

TEST(time_of_day_off_by_default_its_minute_the_drivers) {
    // 0.74: the sun by the clock - off unless asked; the minute now is not the file's.
    CHECK(!parseConfig("").effects.dayCycle && parseConfig("").effects.dayNow == -1);
    CHECK(parseConfig("[effects]\nday_cycle = 1\n").effects.dayCycle);
    CHECK(!effectsOff().dayCycle && effectsOff().dayNow == -1);
}

TEST(taa_off_unless_asked) {
    // 0.75: [quality] taa
    CHECK(!parseConfig("").taa);
    CHECK(parseConfig("[quality]\ntaa = 1\n").taa);
}

TEST(keep_fps_off_unless_asked_and_bounded) {
    // 0.76: [frames] keep_fps
    CHECK(parseConfig("").keepFps == 0);
    CHECK(parseConfig("[frames]\nkeep_fps = 45\n").keepFps == 45);
    CHECK(parseConfig("[frames]\nkeep_fps = 999\n").keepFps == 0);
}

TEST(sun_shadows_are_gone) {
    // 0.77 and 1.12: the moving shadows; 2.1.4 off, 2.2 their code gone - the old lines are read and left alone
    const Config c = parseConfig("[effects]\nsun_shadows = 1\nreal_shadows = 1\nbloom = 1\n");
    CHECK(c.effects.bloom);
}

TEST(bloom_off_by_default) {
    // 0.78; 0.84: off unless asked (2.6.11: the filmic curve with it gone)
    const Config c = parseConfig("");
    CHECK(!c.effects.bloom && c.effects.bloomStrength == 60);
    CHECK(!effectsOff().bloom);
}

TEST(region_looks_off_by_default) {
    // 0.79; 0.84: off unless asked, the round characters too
    CHECK(!parseConfig("").effects.regionLooks && parseConfig("").effects.regionLooksStrength == 100);
    CHECK(!parseConfig("").effects.roundLight && parseConfig("[effects]\nround_light = 1\n").effects.roundLight);
    CHECK(!effectsOff().regionLooks);
}

TEST(footprints_and_storms_off_by_default) {
    // 0.80; 0.84: off unless asked
    CHECK(!parseConfig("").effects.footprints && !parseConfig("").effects.storms);
    CHECK(!effectsOff().footprints && !effectsOff().storms);
}

TEST(grass_off_by_default) {
    // 0.81: costly - asked for
    CHECK(!parseConfig("").effects.grass && parseConfig("").effects.grassAmount == 70);
    CHECK(parseConfig("[effects]\ngrass = 1\n").effects.grass);
}

TEST(skill_sparks_off_by_default) {
    // 0.82; 0.84: off unless asked
    CHECK(!parseConfig("").effects.skillSparks && parseConfig("").effects.skillSparksAmount == 100);
    CHECK(!effectsOff().skillSparks);
}

TEST(map_fade_off_by_default) {
    // 0.83; 0.84: the map's fade off (2.6.11: the film grain with it gone)
    CHECK(!parseConfig("").effects.mapFade);
    CHECK(!effectsOff().mapFade);
}

TEST(frames_limited_and_saving_power_by_default) {
    // 0.85: 60 frames a second, 15 in the background and 30 with no map
    const Config c = parseConfig("");
    CHECK(c.fpsLimit == 60 && c.savePower);
    CHECK(parseConfig("[frames]\nfps_limit = 0\nsave_power = 0\n").fpsLimit == 0);
    CHECK(!parseConfig("[frames]\nsave_power = 0\n").savePower);
}

TEST(profile_off_unless_asked) {
    // 0.87: [debug] profile
    CHECK(!parseConfig("").profile);
    CHECK(parseConfig("[debug]\nprofile = 1\n").profile);
}

TEST(profile_switch_written_into_the_shipped_settings_file) {
    // 0.87.1: the Diagnostics page's switch into an installed uaro-graphics.ini (no profile key in it yet) - read back
    // on, in [debug], the log level kept.
    std::string text;
    {
        FILE* f = std::fopen(UAGFX_SOURCE_DIR "/dist/uaro-graphics.ini", "rb");
        CHECK(f != nullptr);
        if (!f) return;
        char buf[4096];
        size_t n = 0;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
    }
    const std::string edited = settings::applyIniEdits(text, {{"debug", "profile", "1"}});
    const Config c = parseConfig(edited);
    CHECK(c.profile);
    CHECK(c.logLevel == LogLevel::Info);
    CHECK(!parseConfig(text).profile);
}

TEST(state_cache_on_unless_turned_off) {
    // 0.88: [debug] state_cache - Direct3D 11 sets only what changed between draws
    CHECK(parseConfig("").stateCache);
    CHECK(!parseConfig("[debug]\nstate_cache = 0\n").stateCache);
    // 0.89: [debug] fast_pick - the cell under the mouse along its ray
    CHECK(parseConfig("").fastPick);
    CHECK(!parseConfig("[debug]\nfast_pick = 0\n").fastPick);
    // 0.90: [debug] merge_draws - Direct3D 11's draw calls gathered
    CHECK(parseConfig("").mergeDraws);
    CHECK(!parseConfig("[debug]\nmerge_draws = 0\n").mergeDraws);
}

TEST(no_game_fog_on_by_default_a_fix_of_every_set) {
    // 2.6.10: the game's own fog left out on the fields and in the towns (R-P6KRQF: prt_fild08's light green
    // 0xBAFF77 veil, thicker as the camera goes back) - a fix, in every set; every switch off shows the game's own
    CHECK(parseConfig("").effects.noGameFog);
    CHECK(!parseConfig("[effects]\nno_game_fog = 0\n").effects.noGameFog);
    CHECK(!effectsOff().noGameFog);
    const effect_groups::Member* m = effect_groups::memberOf("effects.no_game_fog");
    CHECK(m && m->group == effect_groups::GameFixes && m->level == 1);
}

TEST(game_fog_drawn_but_under_the_open_sky_with_the_fix) {
    CHECK(gameFogDrawn(true, false, true));    // the fix off: the game's as it asks
    CHECK(!gameFogDrawn(true, true, true));    // a field or a town: none
    CHECK(gameFogDrawn(true, true, false));    // a dungeon keeps its dark fog (it hides the void past the map)
    CHECK(!gameFogDrawn(false, false, false));  // the game asked for none
}

TEST(clock_auto_by_default_a_moment_by_its_word) {
    // 2.6.11: [effects] clock - auto (the computer's) or one of five moments; a change of it alone is a change
    CHECK_EQ(parseConfig("").effects.clock, int(daylight::kClockAuto));
    const Config c = parseConfig("[effects]\nclock = sunset\n");
    CHECK_EQ(c.effects.clock, int(daylight::kClockSunset));
    CHECK(settings::tokenOf(*settings::findSetting("effects.clock"), c) == "sunset");
    CHECK_EQ(parseConfig("[effects]\nclock = noon\n").effects.clock, int(daylight::kClockAuto));  // not its word
    const Config server = parseConfig("[effects]\nclock = server\n");  // 2.6.19: the server's night
    CHECK_EQ(server.effects.clock, int(daylight::kClockServer));
    CHECK(settings::tokenOf(*settings::findSetting("effects.clock"), server) == "server");
    CHECK(!(c.effects == parseConfig("").effects));
}

TEST(season_auto_by_default_a_season_by_its_word) {
    // 2.6.22: [effects] season - auto (the computer's calendar) or one of four; a change of it alone is a change
    CHECK_EQ(parseConfig("").effects.season, int(seasons::kSeasonAuto));
    const Config c = parseConfig("[effects]\nseason = winter\n");
    CHECK_EQ(c.effects.season, int(seasons::kSeasonWinter));
    CHECK(settings::tokenOf(*settings::findSetting("effects.season"), c) == "winter");
    CHECK_EQ(parseConfig("[effects]\nseason = fall\n").effects.season, int(seasons::kSeasonAuto));  // not its word
    CHECK(!(c.effects == parseConfig("").effects));
    CHECK(appliesNow("effects.season"));
}

TEST(weather_kind_auto_by_default_a_weather_by_its_word) {
    // 2.6.22: [effects] weather_kind - auto (the map's own), clear, or one of four; a change of it alone is a change,
    // and the weather effect's own switch stays as it was
    CHECK_EQ(parseConfig("").effects.weatherKind, int(weather::kWeatherAuto));
    const Config c = parseConfig("[effects]\nweather_kind = rain\n");
    CHECK_EQ(c.effects.weatherKind, int(weather::kWeatherRain));
    CHECK(settings::tokenOf(*settings::findSetting("effects.weather_kind"), c) == "rain");
    CHECK(settings::tokenOf(*settings::findSetting("effects.weather_kind"), parseConfig("")) == "auto");
    CHECK_EQ(parseConfig("[effects]\nweather_kind = clear\n").effects.weatherKind, int(weather::kWeatherClear));
    CHECK_EQ(parseConfig("[effects]\nweather_kind = storm\n").effects.weatherKind, int(weather::kWeatherAuto));
    CHECK(!(c.effects == parseConfig("").effects));
    CHECK(c.effects.weather == parseConfig("").effects.weather);
    CHECK(appliesNow("effects.weather_kind"));
}

TEST(night_creatures_off_by_default_in_no_set) {
    // 2.6.11: the fireflies in the grass - a new look, the player's own (2.6.19: the moths at the fire removed - an
    // old ini's moths = 1 is not read)
    const EffectSettings d = parseConfig("").effects;
    CHECK(!d.nightFireflies);
    const EffectSettings e = parseConfig("[effects]\nnight_fireflies = 1\nmoths = 1\n").effects;
    CHECK(e.nightFireflies);
    CHECK(!effectsOff().nightFireflies);
    const effect_groups::Member* m = effect_groups::memberOf("effects.night_fireflies");
    CHECK(m && m->group == effect_groups::Creatures && m->level == 0);
    CHECK(!effect_groups::memberOf("effects.moths"));
}

TEST(debug_seams_view_off_by_default_applied_at_once) {
    // 2.7.8: [debug] seams_view - the ground seams' strips coloured (1) or coloured over everything (2), to find what
    // blinks on the hills (prt_fild08 200 200); a running game takes it at once (the console's row)
    CHECK_EQ(parseConfig("").seamsView, 0);
    CHECK_EQ(parseConfig("[debug]\nseams_view = 1\n").seamsView, 1);
    CHECK_EQ(parseConfig("[debug]\nseams_view = 2\n").seamsView, 2);
    CHECK_EQ(parseConfig("[debug]\nseams_view = 7\n").seamsView, 0);
    const Config two = parseConfig("[debug]\nseams_view = 2\n");
    CHECK(settings::tokenOf(*settings::findSetting("debug.seams_view"), two) == "2");
    CHECK(appliesNow("debug.seams_view"));
    CHECK_EQ(liveUpdate(parseConfig(""), two).next.seamsView, 2);
}
