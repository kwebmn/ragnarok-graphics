#pragma once
#include <string>

#include "core/config.hpp"

// 2.2: the Effects page made simple - a set for the card (weak, medium, strong) picks which effects are on, and nine
// groups (light, shadows, surfaces, nature, water, air, life, skills, picture) each turn theirs on or off and make
// them stronger or weaker. The driver works from what effective() gives; "custom" is every switch as written.
namespace uagfx::effect_groups {

// The groups, in the page's order; their ini words: [effects] group_<word> and group_<word>_strength. 2.2.1: Fixes -
// what mends the old picture rather than adds to it (the map's light smooth, effects not cut at the walls, the
// fade between maps), in every set.
enum Group { Fixes, Light, Shadows, Surfaces, Nature, Water, Air, Life, Skills, Picture };
const char* groupWord(int g);
bool hasStrength(int g);  // false: no slider (the living world's creatures have no strength)

struct Member {
    const char* key;  // without "effects."
    int group;
    int level;  // the weakest set it is in: 1 weak, 2 medium, 3 strong; 0 none (by hand only)
    bool EffectSettings::*on;
};
const Member* members();
int memberCount();
const Member* memberOf(const std::string& key);  // "effects.x" or "x"; nullptr: not an effect's switch

// The set's level: 0 off, 1 weak, 2 medium, 3 strong; auto by the card, `cardLevel` (see below); custom -1.
int setLevel(EffectPreset p, int cardLevel);
// The card's level for the auto set: Direct3D 11 or under 4 GB weak, under 8 GB medium, else strong - as the
// automatic setup's tiers (settings/auto_setup.cpp).
int cardLevel(bool d3d12, unsigned long long vramMb);

// What the driver draws by: the set's switches (all as written for custom), each group's off, its strength on its
// effects' own; with a set, its mode (tier) - the remaster's for strong, classic+ for the others.
EffectSettings effective(const EffectSettings& e, int cardLevel);

const char* presetToken(EffectPreset p);  // "auto", "off", "weak", "medium", "strong", "custom"
bool presetFrom(const std::string& word, EffectPreset& p);

}  // namespace uagfx::effect_groups
