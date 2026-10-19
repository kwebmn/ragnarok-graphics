#include <algorithm>
#include <cmath>

#include "core/critters.hpp"
#include "core/game_camera.hpp"
#include "core/living.hpp"
#include "core/torch.hpp"
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
            round = round && r > 0.8f && r < 3.0f && s.at.y > fire.y - 1.8f && s.at.y < fire.y + 1.0f;
            round = round && s.size > 0.8f;  // 2.6.12: at 0.55 a speck by the flame, taken for a spark (R-B27V4Y)
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

TEST(critters_moths_keep_to_the_cameras_side_of_their_fire) {
    // 2.6.12: a torch hangs on a wall - the moths circle out in front of it, on the camera's side, not in the wall
    critters::World w;
    w.setPlace(living::placeOf("moc_pryd01", false, false));
    critters::Switches on;
    on.moths = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.05f;
    f.day = 0.0f;
    const critters::Point fire{0.0f, -3.0f, 0.0f};
    f.fires = {fire};
    f.toCamera = critters::Point{0.0f, 0.0f, -1.0f};
    float behind = -1e9f, sum = 0.0f;
    int n = 0;
    for (int frame = 0; frame < 400; ++frame) {
        f.now = double(frame) * 0.05;
        w.update(on, f);
        for (const critters::Sprite& s : w.sprites())
            if (s.kind == critters::Moth) {
                behind = std::max(behind, s.at.z - fire.z);
                sum += s.at.z - fire.z;
                ++n;
            }
    }
    CHECK_EQ(n, 1200);
    CHECK(behind < 0.8f);           // never deep behind the flame
    CHECK(sum / float(n) < -0.8f);  // out in front of it
}

TEST(critters_fire_of_a_flame_is_its_tip_in_the_world) {
    // 2.6.12: a flame the game drew, back from the screen into the world four fifths up from its base - where the
    // moths circle and the embers rise; the camera here looks along the world's z from (10, -5, 20)
    const camera::Projection p = camera::projectionFor(1920, 1080);
    const float viewToWorld[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 10, -5, 20};
    torch::Flame fl;
    fl.baseX = p.cx;
    fl.baseY = p.cy;
    fl.upY = -50.0f;
    fl.rhw = 0.01f;  // 100 units away
    critters::Point at;
    CHECK(critters::fireOf(fl, p, viewToWorld, at));
    CHECK(std::fabs(at.x - 10.0f) < 1e-3f);
    CHECK(std::fabs(at.y - (-5.0f - 40.0f * 100.0f / p.f1)) < 1e-3f);
    CHECK(std::fabs(at.z - 120.0f) < 1e-3f);
    fl.rhw = 0.0f;  // no depth: nowhere
    CHECK(!critters::fireOf(fl, p, viewToWorld, at));
}
