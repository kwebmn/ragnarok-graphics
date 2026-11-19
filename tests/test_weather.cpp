#include <algorithm>
#include <cmath>

#include "core/weather.hpp"
#include "test.hpp"

using namespace uagfx;
using weather::Kind;

TEST(weather_by_the_map) {
    // 0.39: the driver's own weather (the server sends none): snow in Lutie, leaves in Payon's woods, sand in the wind
    // about Morroc, a fine rain over Niflheim and Einbroch; elsewhere none.
    CHECK(weather::of("xmas") == Kind::Snow);
    CHECK(weather::of("xmas_fild01") == Kind::Snow);
    CHECK(weather::of("payon") == Kind::Leaves);
    CHECK(weather::of("pay_fild04") == Kind::Leaves);
    CHECK(weather::of("morocc") == Kind::Sand);
    CHECK(weather::of("moc_fild12") == Kind::Sand);
    CHECK(weather::of("niflheim") == Kind::Rain);
    CHECK(weather::of("nif_fild01") == Kind::Rain);
    CHECK(weather::of("einbroch") == Kind::Rain);
    CHECK(weather::of("ein_fild06") == Kind::Rain);
    for (const char* m : {"prontera", "prt_fild08", "xmas_in", "xmas_dun01", "payon_in01", "pay_dun00", "moc_pryd01",
                          "ein_in01", "nif_in", ""})
        CHECK(weather::of(m) == Kind::None);
}

TEST(weather_chosen_like_the_clock) {
    // 2.6.22, [effects] weather_kind (the user: «и погоду также сделай»): the map's own (auto), clear skies, or one of
    // the four the player chose under every open sky - never under a roof
    CHECK(weather::chosen(weather::kWeatherAuto, "xmas") == Kind::Snow);
    CHECK(weather::chosen(weather::kWeatherAuto, "niflheim") == Kind::Rain);
    CHECK(weather::chosen(weather::kWeatherAuto, "prontera") == Kind::None);
    CHECK(weather::chosen(weather::kWeatherClear, "xmas") == Kind::None);
    CHECK(weather::chosen(weather::kWeatherClear, "niflheim") == Kind::None);
    CHECK(weather::chosen(weather::kWeatherRain, "prt_fild08") == Kind::Rain);
    CHECK(weather::chosen(weather::kWeatherSnow, "prontera") == Kind::Snow);
    CHECK(weather::chosen(weather::kWeatherLeaves, "morocc") == Kind::Leaves);
    CHECK(weather::chosen(weather::kWeatherSand, "payon") == Kind::Sand);
    for (int c = weather::kWeatherClear; c < weather::kWeathers; ++c)
        for (const char* m : {"payon_in01", "pay_dun00", "xmas_in", "moc_pryd01", "ein_in01", ""})
            CHECK(weather::chosen(c, m) == Kind::None);
    CHECK(weather::chosen(42, "payon") == Kind::Leaves);  // none of them: the map's
    CHECK(weather::chosen(-1, "xmas") == Kind::Snow);
}

TEST(weather_ours_hides_the_games_snow) {
    // 2.7.40 (the tester in Lutie: «а можно как-то снег игры убрать, там получается 2 снега»): where our snow falls
    // («Опади» on, snow over the map), the game's own (its effect 162) is not drawn - one snow; elsewhere it stays
    using weather::Kind;
    CHECK(weather::hidesGameEffect(Kind::Snow, true, 162));
    CHECK(!weather::hidesGameEffect(Kind::Snow, false, 162));  // «Опади» off: the game's snow as it was
    CHECK(!weather::hidesGameEffect(Kind::None, true, 162));   // indoors, or clear: the game's stays
    CHECK(!weather::hidesGameEffect(Kind::Rain, true, 162));
    CHECK(!weather::hidesGameEffect(Kind::Snow, true, 163));   // sakura - not snow
    CHECK(!weather::hidesGameEffect(Kind::Snow, true, 0));
}

// 3.0.5-beta.19 (the user: «делай» - the weather's particles as small quads): weather_vs's cells over the picture -
// each layer's across and down by the kind's cell side times the layer's depth (shaders.cpp weatherLayer), two more
// each way for the layer's shift
TEST(weather_quads_cover_the_picture_by_each_layers_cells) {
    const weather::QuadCells pollen = weather::quadCells(5, 1920.0f, 1080.0f);
    CHECK_EQ(pollen.across[0], 42u);  // cells of 48 game pixels: 40 across and two more
    CHECK_EQ(pollen.count[0], 42u * 25u);
    CHECK_EQ(pollen.across[1], 24u);  // 88
    CHECK_EQ(pollen.count[1], 24u * 15u);
    CHECK_EQ(pollen.across[2], 17u);  // 128
    CHECK_EQ(pollen.count[2], 17u * 11u);
    const weather::QuadCells rain = weather::quadCells(2, 1920.0f, 1080.0f);
    CHECK_EQ(rain.across[0], 148u);  // 13.2: the most of them
    CHECK(rain.count[0] + rain.count[1] + rain.count[2] < 20000u);
    const weather::QuadCells none = weather::quadCells(1, 0.0f, 0.0f);
    CHECK_EQ(none.count[0] + none.count[1] + none.count[2], 0u);
}

// 3.0.22-beta.23 (the owner 19.11 «возьми» - another server's «Sandstorm»): [effects] sandstorm - over Morroc's sand in
// the wind a storm now and then: one in each kStormEvery (40 minutes), at the same moment on every computer by the
// clock, rising over kStormRise, whole for kStormHold, gone over kStormRise; smooth, never a leap, nothing between
TEST(weather_sandstorm_now_and_then) {
    const double day = 86400.0, from = std::floor(1.7e9 / weather::kStormEvery) * weather::kStormEvery;  // 2023
    int storms = 0;
    double whole = 0.0, any = 0.0, longestWhole = 0.0, run = 0.0;
    float last = weather::stormAt(from), most = 0.0f, leap = 0.0f;
    for (double t = from + 0.5; t < from + day; t += 0.5) {
        const float s = weather::stormAt(t);
        CHECK(s >= 0.0f && s <= 1.0f);
        if (last == 0.0f && s > 0.0f) ++storms;
        leap = std::max(leap, std::fabs(s - last));
        most = std::max(most, s);
        any += s > 0.0f ? 0.5 : 0.0;
        if (s >= 0.999f) {
            whole += 0.5;
            run += 0.5;
            longestWhole = std::max(longestWhole, run);
        } else {
            run = 0.0;
        }
        last = s;
    }
    CHECK_EQ(storms, 36);  // a day's 40-minute spans
    CHECK(most == 1.0f && longestWhole >= 29.0 && longestWhole <= 31.0);
    CHECK(any > 36 * 45.0 && any < 36 * 52.0);  // some 50 s each: under 4 % of the time
    CHECK(leap < 0.1f);                          // half a second: under a tenth
    CHECK(weather::stormAt(from + 1234.5) == weather::stormAt(from + 1234.5));
}
