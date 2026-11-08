#pragma once
#include <string>
#include <vector>

namespace uagfx::mobbattle {

// 3.0.11-beta.279 (the owner's feature 5): the monster's card under the cursor - how the character and the monster
// fare against each other by the server's pre-renewal rules (src/map/battle.c): a hit lands at 80 + HIT - FLEE per
// cent (5-100, battle.conf min_hitrate/max_hitrate), a monster's HIT its level + DEX and its FLEE its level + AGI; the
// character's perfect dodge first; a monster's blow ATK1..ATK2-1 (no base ATK: enable_baseatk 9 = players and
// homunculi) less the character's DEF per cent and then its VIT DEF (DEF2/2 .. DEF2/2 + DEF2 (DEF2 - 15) / 150 - 1);
// the character's own blow its ATK both parts less the monster's DEF per cent and VIT .. VIT + (VIT / 20)^2 - 1 (no
// element, size or cards: «≈»). The numbers from db/mob_db*.txt (tools/mob_battle.py -> core/mob_battle.inc).
// 3.0.16-beta.32 (the owner: «делай» - the comparison with the monsters): its size (0 small, 1 medium, 2 large) and
// element (its type + its level x 20, as mob_db) - a weapon's ATK by its size (db/size_fix.txt), the blow by the
// element (db/attr_fix.txt: a neutral blow on a Ghost 25 % at levels 1-2, none at 3-4)
struct Mob {
    int id, level, atk1, atk2, def, mdef, agi, vit, dex, attackDelay, size, element;
};
const Mob* find(int id);  // null: not a monster of the server's

struct Own {  // the status window's numbers
    int hit = 0, flee = 0, perfectDodge = 0, def = 0, def2 = 0, atk = 0, atk2 = 0;
    // 3.0.16-beta.32: the stats' part of `atk` (the rest the weapon's, sized) and the weapon's kind (statmath's: the
    // size_fix columns) - -1 not known: no size fix
    int batk = -1, weapon = -1;
};
int sizeFix(int weapon, int size);  // per cent, the server's db/size_fix.txt; 100 not known
int neutralFix(int element);        // per cent, a neutral blow on that element (db/attr_fix.txt's first row)
struct Fight {
    int hitChance = 0;      // the character's on it, %
    int itsHitChance = 0;   // its on the character after the perfect dodge, %
    int itsLeast = 0, itsMost = 0;  // its blow on the character
    double itsPerSecond = 0;        // its blows' damage a second, the misses counted
    int mine = 0;                   // the character's blow, about
    int hitsToKill = 0;             // 0 not known (no HP)
};
Fight fight(const Mob& m, const Own& own, int hp);

struct Texts {
    // «Ви влучаєте {1} %», «Він влучає {1} %», «Його удар {1}–{2} · {3}/с», «Ваш удар ≈{1} · ударів {2}»
    std::string hit, its, blow, mine;
};
std::vector<std::string> lines(const Fight& f, const Texts& t);

}  // namespace uagfx::mobbattle
