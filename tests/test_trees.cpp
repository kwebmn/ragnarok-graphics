#include <cmath>

#include "core/trees.hpp"
#include "test.hpp"

using namespace uagfx;
using trees::Season;

TEST(trees_seasons_by_the_map_and_the_month) {
    // 1.7: rime in Lutie all year, autumn in September to November where the trees turn
    CHECK(trees::seasonOf("xmas_fild01", 6) == Season::Rime);
    CHECK(trees::seasonOf("prt_fild08", 10) == Season::Autumn);
    CHECK(trees::seasonOf("prt_fild08", 8) == Season::Green);
    CHECK(trees::seasonOf("prt_fild08", 12) == Season::Green);
    CHECK(trees::seasonOf("moc_fild12", 10) == Season::Green);  // the desert's palms do not turn
    CHECK(trees::evergreen("um_fild01") && trees::evergreen("niflheim") && trees::evergreen("gl_church"));
    CHECK(!trees::evergreen("pay_fild01") && !trees::evergreen("prontera"));
    CHECK(trees::monthNow() >= 1 && trees::monthNow() <= 12);
}

TEST(trees_shade_is_smooth_and_each_tree_its_own) {
    const trees::Shade a = trees::shadeAt(100.0f, 200.0f, Season::Green);
    const trees::Shade near = trees::shadeAt(100.5f, 200.0f, Season::Green);  // the same crown: nearly the same
    CHECK(std::fabs(a.tint[0] - near.tint[0]) < 0.02f && std::fabs(a.tint[2] - near.tint[2]) < 0.02f);
    bool differ = false;  // trees apart differ
    for (int i = 1; i < 20 && !differ; ++i) {
        const trees::Shade far = trees::shadeAt(100.0f + 37.0f * float(i), 200.0f, Season::Green);
        differ = std::fabs(far.tint[1] - a.tint[1]) > 0.01f;
    }
    CHECK(differ);
    int turned = 0;
    for (int i = 0; i < 400; ++i) {
        const trees::Shade s = trees::shadeAt(13.0f * float(i), 7.0f * float(i % 37), Season::Autumn);
        for (const float t : s.tint) CHECK(t > 0.8f && t < 1.25f);  // a shade, not another colour
        CHECK(s.autumnAmount >= 0.0f && s.autumnAmount <= 0.85f + 1e-6f && s.rime == 0.0f);
        turned += s.autumnAmount > 0.0f ? 1 : 0;
    }
    CHECK(turned > 0 && turned < 400);  // some trees turned, not all
    CHECK(a.autumnAmount == 0.0f && a.rime == 0.0f);  // green: none of it
    CHECK(trees::shadeAt(0.0f, 0.0f, Season::Rime).rime >= 0.45f);
}

TEST(trees_light_through_the_leaves_low_sun_most_none_at_night) {
    CHECK(std::fabs(trees::throughLight(0.55f, 0.0f) - 0.45f) < 1e-5f);  // noon
    CHECK(std::fabs(trees::throughLight(1.6f, 0.0f) - 1.0f) < 1e-5f);    // sunset
    CHECK(trees::throughLight(1.0f, 0.0f) > trees::throughLight(0.6f, 0.0f));
    CHECK(trees::throughLight(1.6f, 1.0f) == 0.0f);
}
