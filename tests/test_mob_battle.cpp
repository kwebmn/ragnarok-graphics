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
