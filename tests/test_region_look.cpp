#include "core/region_look.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(region_look_by_the_map_and_its_strength) {
    // 0.79: each region its colours; half the strength half the change; an unknown map the game's.
    const region_look::Look nif = region_look::of("nif_fild01", 100);
    CHECK(std::string(nif.region) == "Niflheim" && nif.saturation < 0.7f && nif.tint[2] > 1.0f);
    CHECK(std::string(region_look::of("gef_fild07", 100).region) == "Geffen");
    CHECK(std::string(region_look::of("gl_cas01", 100).region) == "Glast Heim");
    CHECK(std::string(region_look::of("payon", 100).region) == "Payon");
    const region_look::Look half = region_look::of("moc_fild01", 50);
    CHECK(half.tint[2] > 0.93f && half.tint[2] < 0.95f);
    const region_look::Look none = region_look::of("1@tower", 100);
    CHECK(std::string(none.region).empty() && none.saturation == 1.0f && none.tint[0] == 1.0f);
    CHECK(region_look::of("prontera", 0).saturation == 1.0f);
}
