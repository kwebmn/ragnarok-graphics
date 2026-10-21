#include "core/effect_groups.hpp"
#include "core/effect_where.hpp"
#include "core/weather.hpp"
#include "test.hpp"

using namespace uagfx;
using effect_where::worksOn;

TEST(effect_where_a_cave_and_a_field) {
    // 2.6.22, the console's «Только на этой карте» (the user: «в пещере не будут мешать трава и погода»)
    const EffectSettings e;
    for (const char* k : {"effects.grass", "effects.flowers",
                          "effects.weather", "effects.storms", "effects.butterflies", "effects.morning_fog",
                          "effects.ground_seams", "effects.no_game_fog"})
        CHECK(!worksOn(k, "pay_dun00", e));
    for (const char* k : {"effects.wet_walls", "effects.cave_drips", "effects.lit_dust", "effects.torch",
                          "effects.actor_shadows", "effects.water", "effects.lamps", "effects.motes"})
        CHECK(worksOn(k, "pay_dun00", e));  // the map's file is not read: its torches, lamps, water are listed
    for (const char* k : {"effects.grass", "effects.flowers", "effects.butterflies",
                          "effects.morning_fog", "effects.footprints", "effects.seasons"})
        CHECK(worksOn(k, "prt_fild08", e));
    for (const char* k : {"effects.wet_walls", "effects.cave_drips", "effects.lit_dust", "effects.underwater",
                          "effects.swamp", "effects.lava", "effects.snow_sparkle", "effects.breath",
                          "effects.sand_drift", "effects.weather", "effects.floor_reflections"})
        CHECK(!worksOn(k, "prt_fild08", e));
}

TEST(effect_where_the_weather_chosen) {
    EffectSettings e;  // auto: the map's own
    CHECK(worksOn("effects.weather", "niflheim", e) && worksOn("effects.storms", "niflheim", e));
    CHECK(worksOn("effects.wet_ground", "niflheim", e) && worksOn("effects.floor_reflections", "niflheim", e));
    e.weatherKind = weather::kWeatherRain;
    CHECK(worksOn("effects.storms", "prt_fild08", e) && worksOn("effects.rain_wet", "prt_fild08", e));
    CHECK(!worksOn("effects.storms", "prt_in", e));  // under a roof none
    e.weatherKind = weather::kWeatherSnow;
    CHECK(worksOn("effects.weather", "prt_fild08", e) && !worksOn("effects.storms", "prt_fild08", e));
    e.weatherKind = weather::kWeatherClear;
    CHECK(!worksOn("effects.weather", "xmas", e));
}

TEST(effect_where_the_places) {
    const EffectSettings e;
    CHECK(worksOn("effects.lava", "mag_dun01", e) && !worksOn("effects.lava", "mag_dun03", e));
    CHECK(worksOn("effects.snow_sparkle", "xmas", e) && worksOn("effects.breath", "ice_dun01", e));
    CHECK(worksOn("effects.footprints", "xmas", e) && worksOn("effects.footprints", "moc_fild12", e));
    CHECK(!worksOn("effects.footprints", "prt_in", e));
    CHECK(worksOn("effects.sand_drift", "moc_fild12", e) && !worksOn("effects.morning_fog", "moc_fild12", e));
    CHECK(worksOn("effects.underwater", "iz_dun02", e) && worksOn("effects.swamp", "um_fild01", e));
    CHECK(worksOn("effects.region_looks", "payon", e));
    CHECK(!worksOn("effects.seasons", "moc_fild12", e));  // the desert has no seasons
    // no map yet: everything is listed
    for (int i = 0; i < effect_groups::memberCount(); ++i)
        CHECK(worksOn(std::string("effects.") + effect_groups::members()[i].key, "", e));
}
