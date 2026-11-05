// 3.0.11-beta.264 (the «Характеристики» concept, stage 1): the server's formulas (core/stat_math) on the numbers of
// the concept's examples - the knight of level 98 with a two-handed sword, the novice of level 1
#include <cmath>

#include "core/stat_math.hpp"
#include "test.hpp"

using namespace uagfx;
namespace sm = uagfx::statmath;

TEST(stat_math_points) {
    CHECK_EQ(sm::pointCost(1), 2);
    CHECK_EQ(sm::pointCost(10), 2);
    CHECK_EQ(sm::pointCost(11), 3);
    CHECK_EQ(sm::pointCost(98), 11);
    CHECK_EQ(sm::pointsBetween(1, 99), 628);
    CHECK_EQ(sm::pointsBetween(70, 73), 26);
    CHECK_EQ(sm::pointsAt(1), 48);
    CHECK(sm::pointsAt(99) > sm::pointsAt(98) && sm::pointsAt(100000) == 0);
}

TEST(stat_math_class_numbers) {
    const sm::Class knight = sm::classOf(7);
    CHECK(knight.known && knight.weight == 28000 && knight.hpFactor == 150 && knight.spFactor == 300);
    CHECK_EQ(knight.delays[3], 550);  // the two-handed sword
    CHECK(!sm::classOf(99999).known);
    CHECK(sm::upper(4008) && !sm::upper(7) && sm::baby(4030) && !sm::baby(4008));
    // the Swordman's job levels: 2 STR, 6 VIT, 10 DEX
    CHECK(sm::jobBonus(1, 10, sm::Str) == 1 && sm::jobBonus(1, 10, sm::Vit) == 1 && sm::jobBonus(1, 10, sm::Dex) == 1);
    CHECK_EQ(sm::jobBonus(1, 1, sm::Str), 0);
}

TEST(stat_math_battle_numbers) {
    const int stats[sm::kStats] = {66, 76, 32, 1, 43, 2};
    CHECK_EQ(sm::atk(stats, 3), 110);   // melee: STR 66 + 6² + DEX 43/5 + LUK 2/5
    CHECK_EQ(sm::atk(stats, 11), 72);   // a bow: DEX 43 + 4² + STR 66/5
    CHECK(sm::matkMin(99) == 295 && sm::matkMax(99) == 460);
    CHECK(sm::hit(98, 43) == 141 && sm::flee(98, 76) == 174);
    CHECK(sm::crit10(2) == 16 && sm::perfectDodge10(1) == 11);
    CHECK_EQ(sm::softMdef(1, 32), 17);
    CHECK(sm::vitDefLeast(32) == 16 && sm::vitDefMost(32) == 18);
}

TEST(stat_math_aspd) {
    CHECK_EQ(sm::amotion(7, 3, 70, 40), 374);  // the concept's knight: ASPD 162
    CHECK_EQ(sm::amotion(7, 3, 76, 43), 360);
    CHECK_EQ(sm::aspd10(360), 1640);
    CHECK(std::fabs(sm::hitsPerSecond(360) - 1.3889) < 0.001);
    CHECK_EQ(sm::amotion(7, 10, 50, 50), 0);       // a rod: not the knight's
    CHECK_EQ(sm::amotion(7, 0, 200, 200), 100);    // never past ASPD 190
}

TEST(stat_math_hp_sp_weight_cast) {
    CHECK_EQ(sm::maxHp(0, 1, 1), 40);   // a new novice
    CHECK_EQ(sm::maxSp(0, 1, 1), 11);
    CHECK_EQ(sm::maxHp(7, 98, 32), 10319);
    CHECK_EQ(sm::maxHp(3, 38, 1), 610);  // R-VZ69Q2: the server's archer
    CHECK_EQ(sm::maxHp(4008, 99, 1), 10065);  // a Lord Knight: a quarter more
    CHECK(sm::maxHp(4030, 50, 1) < sm::maxHp(7, 50, 1));  // a baby's less
    CHECK_EQ(sm::weightMost(7, 66), 4780);
    CHECK_EQ(sm::weightMost(3, 1), 2630);  // R-VZ69Q2: the base STR's
    CHECK(sm::hpRegen(32, 10319) == 1 + 6 + 51 && sm::spRegen(130, 1000) == 1 + 21 + 10 + 9);
    CHECK(sm::castPercent(43) == 71 && sm::castPercent(150) == 0 && sm::castPercent(200) == 0);
    CHECK_EQ(sm::delayPercent(75), 50);
}

// 3.0.11-beta.276 (feature 7): the delay after a skill - Lord of Vermilion's (85) 5 s, Fire Bolt's (19) by level,
// cut by DEX as uaRO's server does it
#include "core/skill_delay.hpp"
TEST(skill_delay_by_level_and_dex) {
    CHECK(skilldelay::afterCast(85, 1) == 5000 && skilldelay::afterCast(19, 3) == 1400);
    CHECK(skilldelay::afterCast(19, 20) == 2800 && skilldelay::afterCast(1, 1) == 0);
    CHECK(skilldelay::withDex(1500, 75) == 750 && skilldelay::withDex(1500, 160) == 0);
    CHECK(skilldelay::left(1000, 2000, 2000) == 0.5f && skilldelay::left(1000, 2000, 3500) == 0.0f);
}
