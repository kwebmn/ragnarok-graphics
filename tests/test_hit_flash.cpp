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

TEST(hit_flash_finds_the_monster_on_a_hill) {
    // 2.7.10 (the tester at prt_fild08 200 200, a hill: «вспышку при ударе не вижу»): the game passes a hit's sound at
    // the monster's x and z but at the player's height - on a slope its point lands far above or below the monster's
    // feet. It goes to the monster drawn last frame whose box takes its x, the feet nearest it.
    hit_flash::Monsters m;
    m.note(hit_flash::Box{370, 430, 220, 305});  // the one hit, its feet at 305
    m.note(hit_flash::Box{600, 660, 220, 305});  // another, aside
    m.frame();
    float sx = 401, sy = 190;  // the hit's point at the player's height: 115 pixels above the feet
    CHECK(m.aim(sx, sy) && sx == 400 && sy == 305);
    hit_flash::Flashes f;
    f.add(sx, sy, 0);
    CHECK(f.strength(370, 430, 220, 305, 0) == 1.0f);  // drawn this frame where it was
    CHECK(f.strength(600, 660, 220, 305, 0) == 0.0f);
    // two in the column: the feet nearer the point
    hit_flash::Monsters two;
    two.note(hit_flash::Box{370, 430, 120, 200});
    two.note(hit_flash::Box{372, 428, 330, 410});
    two.frame();
    float ax = 400, ay = 380;
    CHECK(two.aim(ax, ay) && ay == 410);
    // none in its column, or one far above it: the point stays where it was
    float bx = 900, by = 300;
    CHECK(!m.aim(bx, by) && bx == 900 && by == 300);
    hit_flash::Monsters far;
    far.note(hit_flash::Box{370, 430, -400, -300});
    far.frame();
    float cx = 400, cy = 300;
    CHECK(!far.aim(cx, cy) && cx == 400 && cy == 300);
    // only the last frame's monsters count
    hit_flash::Monsters gone;
    gone.note(hit_flash::Box{370, 430, 220, 305});
    gone.frame();
    gone.frame();  // a frame without it
    float dx = 400, dy = 200;
    CHECK(!gone.aim(dx, dy));
}
