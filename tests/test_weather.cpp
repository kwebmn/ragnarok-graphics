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
