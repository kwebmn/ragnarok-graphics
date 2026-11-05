#pragma once
#include <string>

namespace uagfx::statmath {

// 3.0.11-beta.264 (the «Характеристики» concept, stage 1; the owner: «надо очень хорошо описывать каждый параметр, с
// формулами под каждого персонажа»): the server's pre-renewal formulas (uAthena's status.c, uaRO has no changes of
// its own) with its per-class numbers (core/job_info.inc from db/job_db1, job_db2, statpoint) and its battle settings
// (conf/battle: ASPD 190 at most, stats 99 / 80 a baby's, the cast's DEX 150, the delay by DEX too). Divisions as the
// server's: whole numbers, the rest dropped.
constexpr int kStats = 6;  // STR AGI VIT INT DEX LUK
enum Stat { Str, Agi, Vit, Int, Dex, Luk };
constexpr int kWeapons = 23;  // job_db1's columns: 0 the fist, 1 a dagger, ... 11 a bow, ... 22 a Fuuma Shuriken
constexpr int kMaxAspd = 190, kCastDex = 150, kMaxStat = 99, kMaxBabyStat = 80;

struct Class {
    bool known = false;
    int weight = 0;  // tenths, as the server's
    int hpFactor = 0, hpMultiplicator = 0, spFactor = 0;
    int delays[kWeapons] = {};  // 2000: the class cannot use it
    std::string bonuses;        // a job level's stat: '0' none, '1' STR .. '6' LUK
};
Class classOf(int job);
bool upper(int job);  // the transcendent ones (4001-4022)
bool baby(int job);   // 4023-4045

int pointCost(int value);                 // the points value -> value + 1 costs
int pointsBetween(int from, int to);      // value from -> to
int pointsAt(int baseLevel);              // statpoint.txt: the stat points a level has in all (0: past the table)
int jobBonus(int job, int jobLevel, int stat);  // the stat's «+» from the job levels so far

bool ranged(int weapon);  // a bow, an instrument, a whip, a gun: the ATK by DEX
// the stats' part of the left number - the server's has the weapon's ATK in it too (R-VZ69Q2: 164 = 99 + a bow's 65)
int atk(const int stats[kStats], int weapon);
int matkMin(int intel);
int matkMax(int intel);
int hit(int baseLevel, int dex);
int flee(int baseLevel, int agi);
int perfectDodge10(int luk);  // tenths of a per cent: 10 + LUK
int crit10(int luk);          // tenths of a per cent: 10 + LUK * 10 / 3
int softMdef(int intel, int vit);
int vitDefLeast(int vit);  // a hit's soft DEF: VIT / 2 .. VIT / 2 + VIT (VIT - 15) / 150 - 1
int vitDefMost(int vit);
// The attack's motion (the server's amotion, ms: a hit each 2 of it) and ASPD tenths: 2000 - it; 0: not the class's
int amotion(int job, int weapon, int agi, int dex);
int aspd10(int amotion);
double hitsPerSecond(int amotion);  // 50 / (200 - ASPD)
int maxHp(int job, int baseLevel, int vit);
int maxSp(int job, int baseLevel, int intel);
int hpRegen(int vit, int maxHpValue);   // a tick's (6 s, sitting 3 s)
int spRegen(int intel, int maxSpValue); // a tick's (8 s, sitting 4 s)
int weightMost(int job, int baseStr);   // whole units, as the game shows it; the base STR, not its bonus
int castPercent(int dex);               // the cast's time left, 0-100 %
int delayPercent(int dex);              // the delay after a skill left: the same on uaRO

}  // namespace uagfx::statmath
