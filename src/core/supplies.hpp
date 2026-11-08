#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace uagfx::supplies {

// 3.0.16-beta.8 (the owner: «там где у нас написано количество стрел было бы неплохо для магов и пристов показывать
// количество гемов, а для монков например еще и сферы» - «делай»): what the skills spend from the bag, by uAthena's
// db/skill_require_db.txt (uaRO's server) - the gems, Holy Water, the Sage's and the Ninja's stones, the Alchemist's
// bottles and coats; and which skills spend or make the spirit spheres (the Gunslinger's coins are the same spheres).
// Left out: the ammo (its own pill), the Traps (ground_counts shows them on the skill's cell), the pitchers and
// Cultivation (an item by the level), Call Homunculus (one Embryo, once).
std::vector<int> itemsOf(uint16_t skill);  // the items' numbers one use takes (none: empty)
bool usesSpheres(uint16_t skill);          // spends or makes spirit spheres / coins
bool coins(uint16_t skill);                // a Gunslinger's (up to 10 coins, else 5 spheres)
// 3.0.16-beta.13 (the owner's pick 1 of «20 идей»): the spheres one use needs (skill_require_db's SpiritSphereCost,
// Finger Offensive its level), 0 none; and the items one use needs at this level (Fire Pillar's gem from level 6)
int spheresFor(uint16_t skill, int level);
std::vector<int> itemsAt(uint16_t skill, int level);

// The pills over the hotbar after the ammo's: the items the bar's skills spend, in the order the bar has them first,
// at most kMostItems; the spheres' last when a skill of them is on the bar (or some are up now)
constexpr int kMostItems = 3;
struct Pick {
    std::vector<int> items;
    bool spheres = false;
    int sphereMost = 5;  // its dots: 5, 10 with a Gunslinger's skill
};
Pick pick(const std::vector<uint16_t>& barSkills, int spheresNow);

// How low: 0 plenty, 1 ten or fewer (potionwarn::kLow), 2 none
int level(int count);

}  // namespace uagfx::supplies
