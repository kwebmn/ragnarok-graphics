#include <algorithm>
#include <cmath>

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

TEST(critters_night_fireflies_over_the_grass_from_dusk_to_dawn) {
    // 2.6.11: where the butterflies fly by day, fireflies glow and blink at night - by the hour (the clock's dark),
    // not by the night effect's darkness
    critters::World w;
    w.setPlace(living::placeOf("prt_fild08", true, false));
    critters::Switches on;
    on.nightFireflies = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.05f;
    f.day = 1.0f;  // noon
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Firefly), 0);
    f.day = 0.0f;  // midnight
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Firefly), 14);
    bool low = true;
    for (const critters::Sprite& s : w.sprites())
        if (s.kind == critters::Firefly) low = low && s.at.y < f.centre.y && s.at.y > f.centre.y - 3.0f;
    CHECK(low);  // over the grass, low (up is -y)
    float dim = 1.0f, bright = 0.0f;  // the first one over six seconds: dark a while, lit a while
    for (int k = 0; k < 120; ++k) {
        f.now += f.dt;
        w.update(on, f);
        for (const critters::Sprite& s : w.sprites())
            if (s.kind == critters::Firefly) {
                dim = std::min(dim, s.p1);
                bright = std::max(bright, s.p1);
                break;
            }
    }
    CHECK(dim < 0.05f && bright > 0.8f);
    on.nightFireflies = false;
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Firefly), 0);
    critters::World dungeon;  // no meadow, no fireflies
    dungeon.setPlace(living::placeOf("moc_pryd01", false, false));
    on.nightFireflies = true;
    dungeon.update(on, f);
    CHECK_EQ(count(dungeon.sprites(), critters::Firefly), 0);
}

TEST(critters_moths_round_the_fires_at_night) {
    // 2.6.11: a few pale moths circle each fire near the player at night - anywhere a fire burns
    critters::World w;
    w.setPlace(living::placeOf("prt_sewb1", false, false));
    critters::Switches on;
    on.moths = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.016f;
    f.day = 0.0f;
    const critters::Point fire{5.0f, -3.0f, 5.0f};
    f.fires = {fire};
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Moth), 3);
    bool round = true;
    for (const critters::Sprite& s : w.sprites())
        if (s.kind == critters::Moth) {
            const float r = std::hypot(s.at.x - fire.x, s.at.z - fire.z);
            round = round && r > 0.4f && r < 2.6f && s.at.y > fire.y - 1.8f && s.at.y < fire.y + 1.0f;
        }
    CHECK(round);
    f.day = 1.0f;  // by day none
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Moth), 0);
    f.day = 0.0f;
    f.fires.clear();  // no fire, no moths
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Moth), 0);
    f.fires = {critters::Point{100.0f, -3.0f, 100.0f}};  // far off: none
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Moth), 0);
    f.fires.clear();  // six fires about: the four nearest have theirs
    for (int k = 0; k < 6; ++k) f.fires.push_back(critters::Point{float(k) * 6.0f, -3.0f, 0.0f});
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Moth), 12);
}
