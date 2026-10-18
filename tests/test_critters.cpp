#include "core/critters.hpp"
#include "core/living.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

int count(const std::vector<critters::Sprite>& s, critters::Kind k) {
    int n = 0;
    for (const critters::Sprite& x : s) n += x.kind == k;
    return n;
}

}  // namespace

TEST(critters_butterflies_by_day_on_a_field_none_at_night) {
    critters::World w;
    w.setPlace(living::placeOf("prt_fild08", true, false));
    critters::Switches on;
    on.butterflies = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.016f;
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Butterfly), 6);
    f.night = 1.0f;
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Butterfly), 0);
}

TEST(critters_dragonflies_have_their_own_switch) {
    // 2.4: the dragonflies over the water a box of their own under the butterflies
    critters::World w;
    w.setPlace(living::placeOf("prt_fild08", true, false));
    critters::Switches on;
    on.butterflies = true;
    on.dragonflies = true;
    critters::Frame f;
    f.centreKnown = true;
    f.waterKnown = true;
    f.dt = 0.016f;
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Dragonfly), 3);
    on.dragonflies = false;
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Dragonfly), 0);
    CHECK_EQ(count(w.sprites(), critters::Butterfly), 6);
}

TEST(critters_a_flock_flies_off_when_someone_comes_near) {
    critters::World w;
    w.setPlace(living::placeOf("prontera", true, false));
    critters::Switches on;
    on.birds = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.05f;
    w.update(on, f);
    int ground = count(w.sprites(), critters::BirdGround);
    CHECK(ground >= 5);
    critters::Point flockAt;
    for (const critters::Sprite& s : w.sprites())
        if (s.kind == critters::BirdGround) flockAt = s.at;
    f.feet.push_back(flockAt);
    for (int k = 0; k < 20; ++k) {
        f.now += f.dt;
        w.update(on, f);
    }
    CHECK_EQ(count(w.sprites(), critters::BirdGround), 0);
    CHECK(count(w.sprites(), critters::BirdFlying) >= 5);
}

TEST(critters_drips_only_in_damp_caves) {
    CHECK(living::placeOf("pay_dun00", false, false).cave);
    CHECK(!living::placeOf("moc_pryd01", false, false).cave);
    CHECK(living::placeOf("iz_dun00", false, false).underwater);
    CHECK(living::placeOf("xmas_fild01", true, false).snow);
    CHECK(living::placeOf("moc_fild01", true, false).sand);
    CHECK_EQ(living::placeOf("payon", true, false).flowers, 1);
}
