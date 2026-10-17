#include <cstdio>
#include <string>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"
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

TEST(sun_shadows_off_by_default) {
    // 0.77: the houses' and trees' shadows from the sun; 0.84: off unless asked.
    CHECK(!parseConfig("").effects.sunShadows && parseConfig("").effects.sunShadowsStrength == 60);
    CHECK(!parseConfig("[effects]\nsun_shadows = 1\n").effects.sunShadows);  // 2.1.4: gone
    CHECK(!parseConfig("[effects]\nreal_shadows = 1\n").effects.realShadows);
    CHECK(!effectsOff().sunShadows);
}

TEST(bloom_and_filmic_off_by_default) {
    // 0.78; 0.84: off unless asked
    const Config c = parseConfig("");
    CHECK(!c.effects.bloom && c.effects.bloomStrength == 60 && !c.effects.filmic);
    CHECK(!effectsOff().bloom && !effectsOff().filmic);
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

TEST(film_grain_and_map_fade_off_by_default) {
    // 0.83; 0.84: the map's fade off too
    CHECK(!parseConfig("").effects.filmGrain && !parseConfig("").effects.mapFade);
    CHECK(!effectsOff().filmGrain && !effectsOff().mapFade);
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
