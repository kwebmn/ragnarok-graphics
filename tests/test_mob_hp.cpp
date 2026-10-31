// 3.0.11-beta.95 (the owner: «Полоски HP мобов - всем показывай»; the concept «Контроллер в интерфейсе uaRO»,
// «Полоски HP монстров»): who shows an HP bar over its head (core/mob_hp) - the target, the wounded for a while after
// their last change, every one known or none; a percent gone out of sight is forgotten (no bar rather than a stale
// one); green, red below a quarter; the target's wider
#include <cmath>
#include <vector>

#include "core/mob_hp.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(mob_hp_who_shows_a_bar) {
    mobhp::Table t;
    t.seen(1, 100, 0);
    t.seen(2, 57, 1000);
    t.seen(3, 20, 1000);
    CHECK(t.shown(MobHp::Off, 1, 1500).empty());
    std::vector<mobhp::Shown> v = t.shown(MobHp::Target, 1, 1500);
    CHECK(v.size() == 1 && v[0].id == 1 && v[0].hp == 100 && v[0].target);
    v = t.shown(MobHp::Wounded, 1, 1500);  // the target (whole) first, then the wounded
    CHECK(v.size() == 3 && v[0].id == 1 && v[0].target && !v[1].target && !v[2].target);
    CHECK(t.shown(MobHp::Wounded, 0, 1000 + mobhp::kWoundedMs).empty());  // their last change 8 s ago: no more
    CHECK_EQ(t.shown(MobHp::All, 0, 60000).size(), 3u);  // every one known, whole or not
}

TEST(mob_hp_a_whole_monster_and_an_unknown_target_show_none) {
    mobhp::Table t;
    t.seen(4, 100, 0);
    CHECK(t.shown(MobHp::Wounded, 0, 100).empty());  // whole and not the target
    CHECK(t.shown(MobHp::Target, 9, 100).empty());   // the target's HP not known: no bar rather than a wrong one
    t.seen(4, 70, 200);
    t.seen(4, 100, 300);  // healed whole
    CHECK(t.shown(MobHp::Wounded, 0, 400).empty());
}

TEST(mob_hp_forgotten_out_of_sight_and_on_a_new_map) {
    mobhp::Table t;
    t.seen(5, 40, 0);
    t.gone(5);  // 0x0080: out of sight or dead - its percent may go stale meanwhile
    CHECK_EQ(t.hp(5), -1);
    CHECK(t.shown(MobHp::All, 5, 10).empty());
    t.seen(6, 30, 0);
    t.clear();
    CHECK_EQ(t.hp(6), -1);
}

TEST(mob_hp_bars_colour_place_and_words) {
    const uint32_t low = mobhp::fillColour(24), fine = mobhp::fillColour(80);
    CHECK(low != mobhp::fillColour(25));
    CHECK(((low >> 16) & 0xFF) > ((low >> 8) & 0xFF));    // red below a quarter
    CHECK(((fine >> 8) & 0xFF) > ((fine >> 16) & 0xFF));  // green
    const float box[4] = {900, 400, 960, 480};  // a sprite's box in the game's pixels: left, top, right, bottom
    const mobhp::Bar b = mobhp::barOver(box, false, 1080), t = mobhp::barOver(box, true, 1080);
    CHECK(b.y + b.h < box[1] && std::fabs(b.x + b.w / 2 - 930) < 0.5f);  // over its head, centred
    CHECK(t.w > b.w && t.h >= b.h);  // the target's wider
    CHECK(mobhp::barOver(box, false, 540).w < b.w);  // with the game's size
    CHECK(mobhp::percentText(57) == "57%");
}

// 3.0.11-beta.99 (the owner: «hp для мобов должно быть доступно и в обычном режиме»): whose bar is the target's (wider,
// with its per cent) - with the controller its target, else the one the character fights; with the mouse the monster
// under the cursor while its per cent is known, else the one the character fights (not only while the target's card
// shows it)
TEST(mob_hp_the_target_with_the_mouse_and_with_the_controller) {
    mobhp::Table t;
    t.seen(7, 100, 0);  // under the cursor, whole
    t.seen(9, 40, 0);   // the one the character fights
    CHECK_EQ(mobhp::pointed(t, true, 5, 7, 9), 5u);   // the controller's target
    CHECK_EQ(mobhp::pointed(t, true, 0, 7, 9), 9u);   // the controller with none: the one fought
    CHECK_EQ(mobhp::pointed(t, false, 5, 7, 9), 7u);  // the mouse: the one under the cursor
    CHECK_EQ(mobhp::pointed(t, false, 0, 8, 9), 9u);  // its per cent not known yet: the one fought
    CHECK_EQ(mobhp::pointed(t, false, 0, 0, 9), 9u);
    CHECK_EQ(mobhp::pointed(t, false, 0, 0, 0), 0u);
    const std::vector<mobhp::Shown> v = t.shown(MobHp::Target, mobhp::pointed(t, false, 0, 7, 9), 100);
    CHECK(v.size() == 1 && v[0].id == 7 && v[0].hp == 100 && v[0].target);  // whole, under the cursor: its bar
}
