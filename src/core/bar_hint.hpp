#pragma once
#include <string>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::barhint {

// 3.0.11-beta.68 (the concept's «Ваша панель: Fire Bolt ×2…» - the owner: «отличная идея, нужна»; «множник стихії:
// это влияние стихии, а не прогноз итогового урона»): the player's hotbar skills against a monster - each damaging
// skill with an element of its own (the server's db/skill_db.txt, core/skill_elements.inc) and the server's
// multiplier for that element on the monster's (refpages::elementDamage, its attr_fix). No Windows here.

int elementOf(const std::string& skill);  // its name («MG_FIREBOLT»): 0-9; -1 no damage, the weapon's element, none

struct Skill {
    std::string id;    // its name («MG_FIREBOLT»)
    std::string name;  // as shown («Fire Bolt»)
};
struct Hit {
    std::string name;
    int percent = 100;  // its element's multiplier on the monster, per cent (below 0: it heals the monster)
};
// Each skill once, the most first (those alike as on the bar); none of an element of its own left out
std::vector<Hit> against(const std::vector<Skill>& bar, int element, int level);
// «Fire Bolt ×2 · Storm Gust ×1,5 · Jupitel Thunder ×1» - the first `most`; one that heals it «… лікує»; "" none
std::string line(const std::vector<Hit>& hits, settings::Lang l, size_t most = 4);

}  // namespace uagfx::barhint
