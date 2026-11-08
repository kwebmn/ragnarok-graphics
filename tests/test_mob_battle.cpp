#include "core/mob_battle.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(mob_battle_poring_by_the_server_rules) {
    const mobbattle::Mob* p = mobbattle::find(1002);  // Poring: level 1, ATK 7-10, DEF 0, AGI 1, VIT 1, DEX 6
    CHECK(p != nullptr);
    CHECK(mobbattle::find(1) == nullptr);
    mobbattle::Own own;
    own.hit = 10, own.flee = 20, own.perfectDodge = 1, own.def = 10, own.def2 = 4, own.atk = 30, own.atk2 = 20;
    const mobbattle::Fight f = mobbattle::fight(*p, own, 50);
    CHECK_EQ(f.hitChance, 88);      // 80 + 10 - (1 + 1)
    CHECK_EQ(f.itsHitChance, 66);   // 80 + 7 - 20 = 67, 1 % dodged first
    CHECK_EQ(f.itsLeast, 4);        // 7 * 90 % = 6, less VIT 2
    CHECK_EQ(f.itsMost, 6);         // 9 * 90 % = 8, less 2
    CHECK_EQ(f.mine, 49);           // 50 less VIT 1
    CHECK_EQ(f.hitsToKill, 2);
    const mobbattle::Texts t{"hit {1}", "its {1}", "blow {1}-{2} {3}/s", "mine {1} x{2}"};
    const auto l = mobbattle::lines(f, t);
    CHECK_EQ(l.size(), size_t(4));
    CHECK_EQ(l[0], std::string("hit 88"));
    CHECK_EQ(l[3], std::string("mine 49 x2"));
}

TEST(mob_battle_chances_clamp) {
    const mobbattle::Mob m{1, 99, 1000, 2000, 50, 0, 200, 0, 200, 1000};
    mobbattle::Own own;
    const mobbattle::Fight f = mobbattle::fight(m, own, 0);
    CHECK_EQ(f.hitChance, 5);
    CHECK_EQ(f.itsHitChance, 100);
    CHECK_EQ(f.hitsToKill, 0);
    CHECK_EQ(f.mine, 1);
}

// 3.0.16-beta.32 (the owner: «делай» - the monsters compared): the weapon's part of ATK by the monster's size
// (db/size_fix.txt), a neutral blow on a Ghost (db/attr_fix.txt)
TEST(mob_battle_size_and_element) {
    CHECK_EQ(mobbattle::sizeFix(1, 2), 50);   // a dagger on a large one
    CHECK_EQ(mobbattle::sizeFix(3, 2), 100);  // a two-handed sword on a large one
    CHECK_EQ(mobbattle::sizeFix(6, 0), 50);   // an axe on a small one
    CHECK_EQ(mobbattle::sizeFix(-1, 2), 100);
    CHECK_EQ(mobbattle::neutralFix(20), 100);  // neutral 1
    CHECK_EQ(mobbattle::neutralFix(28), 25);   // ghost 1
    CHECK_EQ(mobbattle::neutralFix(68), 0);    // ghost 3
    const mobbattle::Mob big{1, 10, 10, 20, 0, 0, 1, 0, 1, 1000, 2, 20};
    mobbattle::Own own;
    own.atk = 100, own.batk = 40, own.weapon = 1;  // 40 of the stats, 60 of a dagger: half on a large one
    CHECK_EQ(mobbattle::fight(big, own, 0).mine, 70);
    own.weapon = -1;  // not known: no size fix
    CHECK_EQ(mobbattle::fight(big, own, 0).mine, 100);
    const mobbattle::Mob ghost{1, 10, 10, 20, 0, 0, 1, 0, 1, 1000, 1, 68};
    CHECK_EQ(mobbattle::fight(ghost, own, 500).mine, 0);
    CHECK_EQ(mobbattle::fight(ghost, own, 500).hitsToKill, 0);
    CHECK(mobbattle::find(1002) && mobbattle::find(1002)->size >= 0);
}
