#include "core/hit_flash.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(hit_flash_knows_the_hits_sounds) {
    CHECK(hit_flash::hitSound("data\\wav\\_hit_sword.wav"));
    CHECK(hit_flash::hitSound("data\\wav\\PORING_DAMAGE.wav"));
    CHECK(!hit_flash::hitSound("data\\wav\\poring_attack.wav"));
    CHECK(!hit_flash::hitSound("data\\wav\\effect\\ef_heal.wav"));
    CHECK(!hit_flash::hitSound(nullptr));
}

TEST(hit_flash_over_the_monster_hit_for_a_moment) {
    hit_flash::Flashes f;
    f.add(400, 300, 0);  // its feet
    CHECK(f.strength(370, 430, 220, 305, 0) == 1.0f);
    const float later = f.strength(370, 430, 220, 305, 70);
    CHECK(later > 0.45f && later < 0.55f);
    CHECK(f.strength(600, 660, 220, 305, 70) == 0.0f);  // another monster
    CHECK(f.strength(370, 430, 400, 480, 70) == 0.0f);  // one standing lower down
    CHECK(f.strength(370, 430, 220, 305, 200) == 0.0f);  // over
}

TEST(hit_flash_onto_the_screen) {
    // The identity view: x right, y down, z ahead; the projection's centre at (320, 240).
    const float view[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    camera::Projection p;
    p.f0 = p.f1 = 100;
    p.cx = 320;
    p.cy = 240;
    float sx = 0, sy = 0;
    CHECK(hit_flash::toScreen(view, p, 10, 0, 100, sx, sy) && sx == 330 && sy == 240);
    CHECK(!hit_flash::toScreen(view, p, 0, 0, -5, sx, sy));
}
