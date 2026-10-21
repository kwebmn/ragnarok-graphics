#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"

// 2.2: the Effects page made simple - a set for the card (weak, medium, strong) picks which effects are on, and nine
// groups (light, shadows, surfaces, nature, water, air, life, skills, picture) each turn theirs on or off and make
// them stronger or weaker. The driver works from what effective() gives; "custom" is every switch as written.
namespace uagfx::effect_groups {

// The groups, in the page's order; their ini words: [effects] group_<word> and group_<word>_strength. 2.2.1: Fixes -
// what mends the old picture rather than adds to it (the map's light smooth, effects not cut at the walls, the
// fade between maps), in every set. 2.6.22: fourteen, by what the eye is on (the user: «непонятно что где искать») -
// the time of day and the year, the weather, then the world from the characters out to the sky, the whole screen,
// the fixes; their words none of the ten before (fixes, light, shadows, surfaces, nature, water, air, life, skills,
// picture), which an old file's lines keep (EffectSettings::legacyGroups).
enum Group {
    Time,          // day, night and the seasons: what the Clock and the Season at the top change
    Weather,       // rain, snow, sand: what the Weather at the top brings
    Characters,    // characters and monsters: their shadows, volume, light, prints, appearing, hits, aura
    SkillEffects,  // the skills' look
    Fire,          // torches, embers, chimney smoke, lava, heat haze
    Lights,        // lamps, the maps' lights and fireflies, portals, glows, lit air, forest beams
    Ground,        // the ground and the walls: relief, materials, seams, hills, corner shadows, floors, cards
    Plants,        // grass and trees
    Waters,        // the water and what lives in it
    Creatures,     // butterflies, birds, fireflies
    Caves,         // caves and dungeons: wet walls, drips, dust in the light
    Sky,           // the air: motes, the one wind (2.7.17: the sky past the edge and the land there gone)
    Screen,        // the whole picture's colours
    GameFixes,     // what mends the game's old picture
};
const char* groupWord(int g);
const char* oldGroupWord(int g);  // 2.6.22: the ten before, for an old file's lines (0-9)
bool hasStrength(int g);  // false: no slider (the creatures, the caves... have no strength)

struct Member {
    const char* key;  // without "effects."
    int group;
    int level;  // the weakest set it is in: 1 weak, 2 medium, 3 strong; 0 none (by hand only)
    bool EffectSettings::*on;
};
const Member* members();
int memberCount();
const Member* memberOf(const std::string& key);  // "effects.x" or "x"; nullptr: not an effect's switch

// 2.6.22: an old file's groups (the ten before) - any off or not at 100 %. effective() and shown() hold them by the
// old grouping; settleLegacy() gives the lines that write them into the effects themselves - the set made the
// player's own when one of them is off (as an effect switched under a set), the strengths into the effects', the old
// lines taken out - the same picture after as before; none when there is nothing old.
bool hasLegacy(const EffectSettings& e);
std::vector<settings::IniEdit> settleLegacy(const EffectSettings& e, int cardLevel);
std::vector<settings::IniEdit> oldGroupLinesOut();  // the old lines alone taken out (on, 100 %: nothing to keep)

// The set's level: 0 off, 1 weak, 2 medium, 3 strong; auto by the card, `cardLevel` (see below); custom -1.
int setLevel(EffectPreset p, int cardLevel);
// The card's level for the auto set: Direct3D 11 or under 4 GB weak, under 8 GB medium, else strong - as the
// automatic setup's tiers (settings/auto_setup.cpp).
int cardLevel(bool d3d12, unsigned long long vramMb);

// What the driver draws by: the set's switches (all as written for custom), each group's off, its strength on its
// effects' own; with a set, its mode (tier) - the remaster's for strong, classic+ for the others.
EffectSettings effective(const EffectSettings& e, int cardLevel);

// What the Effects page and the console show: each switch and the mode as the set has them (custom: as written);
// the groups and the strengths as written - they stay over the switches.
EffectSettings shown(const EffectSettings& e, int cardLevel);
// 2.5.20: the light modes' cap (core/light_mode.hpp) on what effective() gave: only the switches of the sets up to
// `level` stay on (0 none, 1 the weak set's) - one the player turned off stays off, one of no set goes off.
EffectSettings atMost(const EffectSettings& effectiveSettings, int level);

// A set made the player's own - an effect switched by hand under a set, or custom chosen: the [effects] lines to
// write ("torch", "1"), every switch as the set has it now, its mode, then preset = custom; `except` (a key about to
// be written anyway, without "effects.": a switch, "tier" or "preset") left out. None for custom: the switches are
// the player's already.
std::vector<std::pair<std::string, std::string>> materialize(const EffectSettings& e, int cardLevel,
                                                             const std::string& except = "");

const char* presetToken(EffectPreset p);  // "auto", "off", "weak", "medium", "strong", "custom"
bool presetFrom(const std::string& word, EffectPreset& p);

}  // namespace uagfx::effect_groups
