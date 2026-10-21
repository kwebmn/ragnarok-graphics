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

TEST(critters_fire_of_a_flame_is_its_tip_in_the_world) {
    // 2.6.12: a flame the game drew, back from the screen into the world four fifths up from its base - where the
    // embers rise; the camera here looks along the world's z from (10, -5, 20)
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

TEST(critters_no_butterflies_nor_fireflies_without_insects_birds_stay) {
    // 2.7.5: a place in winter (living::Place::insects off) - no butterflies by day, no fireflies at night; birds
    critters::World w;
    living::Place cold = living::placeOf("prt_fild08", true, false);
    cold.insects = false;
    w.setPlace(cold);
    critters::Switches on;
    on.butterflies = on.nightFireflies = on.birds = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.016f;
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Butterfly), 0);
    f.day = 0.0f;
    f.night = 1.0f;
    w.update(on, f);
    CHECK_EQ(count(w.sprites(), critters::Firefly), 0);
}

TEST(critters_falling_leaves_seen_against_the_green) {
    // 2.7.30 (the tester in Payon's forest: «Листя з дерев - не увидел»): the leaves falling from the crowns were 0.38
    // units across (some 6 pixels of a 4K picture) and dark green over the green forest - now twice as big, and a
    // summer's light ones: yellow-green or yellow, never the dark green
    critters::World w;
    w.setPlace(living::placeOf("pay_fild01", true, false));
    critters::Switches on;
    on.leaves = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.1f;
    for (int k = 0; k < 12; ++k)
        f.crowns.push_back(critters::Crown{critters::Point{float(k) * 8.0f, -30.0f, 4.0f}, 6.0f});
    for (int i = 0; i < 30; ++i) {
        f.now += 0.1;
        w.update(on, f);
    }
    int leaves = 0;
    for (const critters::Sprite& s : w.sprites())
        if (s.kind == critters::Leaf) {
            ++leaves;
            CHECK(s.size >= 0.7f);
            CHECK(s.p2 == 1.0f || s.p2 == 4.0f);
        }
    CHECK(leaves > 5);
}

TEST(critters_splashes_rise_where_the_hero_wades) {
    // 2.7.35: the game has no swimming - where one walks in the water one wades, the steps (the round shadows'
    // middles) on the bed: 7 units under the surface is Comodo's middle depth, so a splash; 25 under is no place to
    // wade (the special maps: the airship, the waiting hall), and the dry shore none either
    auto drops = [](float depth) {
        critters::World w;
        w.setPlace(living::placeOf("comodo", true, false));
        critters::Switches on;
        on.splashes = true;
        critters::Frame f;
        f.centreKnown = true;
        f.dt = 0.05f;
        f.waterKnown = true;
        f.waterLevel = 15.0f;  // up is -y: the bed `depth` under it
        f.steps.push_back(critters::Point{0.0f, 15.0f + depth, 0.0f});
        f.now = 1.0;
        w.update(on, f);
        f.steps.clear();
        f.now += 0.05;
        w.update(on, f);
        int n = 0;
        for (const critters::Sprite& s : w.sprites()) n += s.kind == critters::Droplet ? 1 : 0;
        return n;
    };
    CHECK(drops(1.0f) > 0);
    CHECK(drops(7.0f) > 0);
    CHECK(drops(18.0f) > 0);
    CHECK_EQ(drops(25.0f), 0);
    CHECK_EQ(drops(-2.0f), 0);
}

TEST(critters_birds_never_settle_on_the_water) {
    // 2.7.34 (the tester in Alberta: «уточек с воды убери, лишнее»): a flock settles where the map's ground stands
    // over the water (Frame::ground), at that ground's height - Alberta's pier had them on the harbour's water
    critters::World w;
    w.setPlace(living::placeOf("alberta", true, false));
    critters::Switches on;
    on.birds = true;
    critters::Frame f;
    f.centreKnown = true;
    f.dt = 0.1f;
    f.waterKnown = true;
    f.waterLevel = 0.0f;  // up is -y: the ground at -1.5 over the water, at +2 under it
    f.ground = [](const critters::Point& p, float& y) {
        y = p.x >= 0.0f ? -1.5f : 2.0f;
        return true;
    };
    int birds = 0;
    for (int i = 0; i < 600 && birds == 0; ++i) {
        f.now += 0.1;
        w.update(on, f);
        for (const critters::Sprite& s : w.sprites())
            if (s.kind == critters::BirdGround) {
                ++birds;
                CHECK(s.at.x >= 0.0f);
                CHECK(s.at.y == -1.5f);
            }
    }
    CHECK(birds > 0);
    critters::World sea;  // nothing but water about: no flock at all
    sea.setPlace(living::placeOf("alberta", true, false));
    f.ground = [](const critters::Point&, float& y) {
        y = 2.0f;
        return true;
    };
    int any = 0;
    for (int i = 0; i < 600; ++i) {
        f.now += 0.1;
        sea.update(on, f);
        for (const critters::Sprite& s : sea.sprites()) any += s.kind == critters::BirdGround ? 1 : 0;
    }
    CHECK_EQ(any, 0);
}


TEST(critters_breath_shows_in_the_snow) {
    // 2.7.42 (the tester in Lutie: «пара не вижу» on 2.7.40 and 2.7.41): a character standing in the snow breathes a
    // puff every 2.6 s, at its head; 2.7.44 (the tester: «пар идет в районе ног»): 15 units over its feet, not 6.2
    critters::World w;
    w.setPlace(living::placeOf("xmas", true, false));
    critters::Switches on;
    on.breath = true;
    critters::Frame f;
    f.centreKnown = true;
    f.centre = critters::Point{100.0f, 0.0f, 100.0f};
    f.feet.push_back(critters::Point{100.0f, 0.0f, 100.0f});
    f.dt = 0.05f;
    int puffs = 0, frames = 0;
    for (int i = 0; i < 120; ++i) {  // six seconds
        f.now += 0.05;
        w.update(on, f);
        bool any = false;
        for (const critters::Sprite& s : w.sprites())
            if (s.kind == critters::Puff) {
                any = true;
                CHECK(s.at.y < -13.0f);  // up is -y: at the head, not the legs
                CHECK(s.alpha > 0.0f);
            }
        frames += any ? 1 : 0;
        puffs += any ? 1 : 0;
    }
    CHECK(frames > 20);  // seen a good part of the time
}
