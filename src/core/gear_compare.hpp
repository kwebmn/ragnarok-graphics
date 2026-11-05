#pragma once
#include <string>
#include <vector>

namespace uagfx::gearcompare {

// 3.0.11-beta.285 (the owner's feature 13): a weapon or an armour of the bag against the one worn where it goes, by
// the server's numbers (db/item_db*.txt - tools/gear_stats.py -> core/gear_stats.inc) and its refine rules
// (db/refine_db.txt, src/map/status.c): a weapon's refine ATK 2, 3, 5, 7 a level by its weapon level, an armour's 0.7
// DEF a level (the server keeps hundredths); past +10 half as much (refine_over_bonus_percent 50). Cards and scripts
// not counted.
struct Gear {
    int id, type, atk, def, slots, location, weaponLevel, levelNeeded;
};
const Gear* find(int id);

int atkOf(const Gear& g, int refine);    // a weapon's ATK with its refine
int defOf100(const Gear& g, int refine); // an armour's DEF with its refine, in hundredths

struct Worn {
    int id = 0, refine = 0;
    std::string name;  // as shown, «+4 Knife»
};
struct Texts {
    std::string instead, none, slots;  // «Замість {1}: {2}», «Нічого не вдягнуто: {1}», «слоти»
};
// The tip's line: «Замість +4 Knife: ATK 25 → 31 (+6) · слоти 3 → 1»; "" none (not gear, or no numbers)
std::string line(int id, int refine, const std::vector<Worn>& worn, const Texts& t);

}  // namespace uagfx::gearcompare
