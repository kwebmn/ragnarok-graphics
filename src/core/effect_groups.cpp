#include "core/effect_groups.hpp"

#include <algorithm>

#include "core/settings/schema.hpp"

namespace uagfx::effect_groups {
namespace {

using E = EffectSettings;

// Every switch of the Effects page: its group and the weakest set it is in. The weak set keeps what costs a card
// little (no pass of its own over the picture); the medium adds the passes and the living world; the strong the
// remaster's own world (materials, new trees, reflections, light). 0: the player's alone - a look the player turned
// down, night by the clock beside the day. 2.6.22: in the order the page shows them, by the new groups.
const Member kMembers[] = {
    {"day_cycle", Time, 2, &E::dayCycle},
    {"night_windows", Time, 3, &E::nightWindows},
    {"morning_fog", Time, 2, &E::morningFog},
    {"no_server_night", Time, 1, &E::noServerNight},  // 2.6.13
    {"seasons", Time, 3, &E::seasons},

    {"weather", Weather, 1, &E::weather},
    {"storms", Weather, 2, &E::storms},
    {"wet_ground", Weather, 2, &E::wetGround},
    {"rain_wet", Weather, 3, &E::rainWet},
    {"sand_drift", Weather, 2, &E::sandDrift},
    {"snow_sparkle", Weather, 2, &E::snowSparkle},

    {"actor_shadows", Characters, 1, &E::actorShadows},
    {"sprite_light", Characters, 3, &E::spriteLight},
    {"round_light", Characters, 2, &E::roundLight},
    {"rim_light", Characters, 2, &E::rimLight},
    {"footprints", Characters, 1, &E::footprints},
    {"breath", Characters, 2, &E::breath},
    {"appear", Characters, 1, &E::appear},
    {"hit_flash", Characters, 1, &E::hitFlash},
    {"aura", Characters, 1, &E::aura},

    {"skills_soft", SkillEffects, 1, &E::skillsSoft},
    {"skill_sparks", SkillEffects, 2, &E::skillSparks},
    {"fire_light", SkillEffects, 1, &E::fireLight},

    {"torch", Fire, 1, &E::torch},
    {"embers", Fire, 2, &E::embers},
    {"smoke", Fire, 1, &E::smoke},
    {"lava", Fire, 1, &E::lava},
    {"heat", Fire, 2, &E::heat},

    {"lamps", Lights, 1, &E::lamps},
    {"map_glows", Lights, 1, &E::mapGlows},
    {"fireflies", Lights, 1, &E::fireflies},
    {"portals", Lights, 1, &E::portals},
    {"glow", Lights, 2, &E::glow},
    {"shafts", Lights, 3, &E::shafts},
    {"rays", Lights, 2, &E::rays},

    {"relief", Ground, 2, &E::relief},
    {"materials", Ground, 3, &E::materials},
    {"ground_seams", Ground, 3, &E::groundSeams},
    {"smooth_hills", Ground, 3, &E::smoothHills},
    {"ao", Ground, 2, &E::ao},
    {"floor_reflections", Ground, 2, &E::floorReflections},
    {"glint", Ground, 2, &E::glint},

    {"grass", Plants, 2, &E::grass},
    {"flowers", Plants, 2, &E::flowers},
    {"dew", Plants, 3, &E::dew},
    {"foliage", Plants, 2, &E::foliage},
    {"gusts", Plants, 2, &E::gusts},
    {"tree_volume", Plants, 2, &E::treeVolume},
    {"leaf_light", Plants, 2, &E::leafLight},
    {"tree_tints", Plants, 2, &E::treeTints},
    {"leaf_fall", Plants, 3, &E::leafFall},

    {"water", Waters, 1, &E::water},
    {"ripples", Waters, 1, &E::ripples},
    {"splashes", Waters, 2, &E::splashes},
    {"underwater", Waters, 2, &E::underwater},
    {"swamp", Waters, 2, &E::swamp},
    {"world_reflections", Waters, 3, &E::worldReflections},

    {"butterflies", Creatures, 2, &E::butterflies},
    {"birds", Creatures, 2, &E::birds},
    {"night_fireflies", Creatures, 0, &E::nightFireflies},  // 2.6.11: off by default, in no set

    {"wet_walls", Caves, 2, &E::wetWalls},
    {"cave_drips", Caves, 2, &E::caveDrips},
    {"lit_dust", Caves, 3, &E::litDust},

    {"sky", Sky, 2, &E::sky},
    {"edge_land", Sky, 3, &E::edgeLand},
    {"flare", Sky, 3, &E::flare},
    {"motes", Sky, 2, &E::motes},
    {"shared_wind", Sky, 3, &E::sharedWind},

    {"grade", Screen, 1, &E::grade},
    {"region_looks", Screen, 2, &E::regionLooks},
    {"bloom", Screen, 3, &E::bloom},

    {"smooth_light", GameFixes, 1, &E::smoothLight},
    {"soft_particles", GameFixes, 1, &E::softParticles},
    {"map_fade", GameFixes, 1, &E::mapFade},
    {"no_game_fog", GameFixes, 1, &E::noGameFog},  // 2.6.10
};

// A group's strength works on these of its effects (each up to its own most).
struct Strength {
    int group;
    int E::*value;
    int most;
};
const Strength kStrengths[] = {
    {Characters, &E::actorShadowsStrength, 100}, {Characters, &E::spriteLightStrength, 200},
    {Characters, &E::rimStrength, 200},          {Characters, &E::auraBrightness, 100},
    {SkillEffects, &E::skillBrightness, 100},    {SkillEffects, &E::skillSparksAmount, 200},
    {SkillEffects, &E::fireLightStrength, 200},
    {Fire, &E::torchBrightness, 200},  {Fire, &E::torchGlow, 200},    {Fire, &E::torchLight, 200},
    {Fire, &E::lavaBrightness, 200},   {Fire, &E::heatStrength, 200},
    {Lights, &E::lampsLight, 200},     {Lights, &E::firefliesBrightness, 200}, {Lights, &E::mapGlowsBrightness, 200},
    {Lights, &E::raysBrightness, 200}, {Lights, &E::portalsStrength, 200},     {Lights, &E::glowStrength, 200},
    {Lights, &E::glowLight, 100},      {Lights, &E::shaftsStrength, 200},
    {Ground, &E::aoStrength, 100},     {Ground, &E::reliefStrength, 200},      {Ground, &E::materialsStrength, 200},
    {Ground, &E::smoothHillsStrength, 200}, {Ground, &E::floorReflectStrength, 100},
    {Plants, &E::grassAmount, 100},    {Plants, &E::foliageStrength, 200},
    {Waters, &E::waterWaves, 100},     {Waters, &E::waterReflections, 100},
    {Weather, &E::weatherAmount, 200},
    {Sky, &E::skyStrength, 100},       {Sky, &E::motesAmount, 200},
    {Screen, &E::bloomStrength, 200},  {Screen, &E::regionLooksStrength, 100},
};

// 2.6.22: the ten groups of 2.2-2.6.21 - an old file's group_<word> lines are held by them (effective) until they are
// written into the effects (settleLegacy): each switch's group then, each strength's.
const char* const kOldWords[kOldEffectGroups] = {"fixes", "light", "shadows", "surfaces", "nature",
                                                 "water", "air",   "life",    "skills",   "picture"};
enum OldGroup {
    OldFixes, OldLight, OldShadows, OldSurfaces, OldNature, OldWater, OldAir, OldLife, OldSkills, OldPicture
};
struct OldMember {
    int group;
    bool E::*on;
};
const OldMember kOldMembers[] = {
    {OldFixes, &E::smoothLight},    {OldFixes, &E::softParticles},  {OldFixes, &E::mapFade},
    {OldFixes, &E::noGameFog},      {OldFixes, &E::noServerNight},
    {OldLight, &E::torch},          {OldLight, &E::smoke},          {OldLight, &E::lamps},
    {OldLight, &E::fireLight},      {OldLight, &E::fireflies},      {OldLight, &E::mapGlows},
    {OldLight, &E::lava},           {OldLight, &E::portals},        {OldLight, &E::rays},
    {OldLight, &E::glow},           {OldLight, &E::roundLight},     {OldLight, &E::nightWindows},
    {OldShadows, &E::actorShadows}, {OldShadows, &E::ao},           {OldShadows, &E::rimLight},
    {OldShadows, &E::spriteLight},
    {OldSurfaces, &E::relief},      {OldSurfaces, &E::floorReflections}, {OldSurfaces, &E::wetGround},
    {OldSurfaces, &E::wetWalls},    {OldSurfaces, &E::glint},       {OldSurfaces, &E::materials},
    {OldSurfaces, &E::groundSeams}, {OldSurfaces, &E::smoothHills},
    {OldNature, &E::grass},         {OldNature, &E::foliage},       {OldNature, &E::treeVolume},
    {OldNature, &E::leafLight},     {OldNature, &E::treeTints},     {OldNature, &E::gusts},
    {OldNature, &E::flowers},       {OldNature, &E::dew},
    {OldNature, &E::leafFall},      {OldNature, &E::seasons},       {OldNature, &E::sharedWind},
    {OldWater, &E::water},          {OldWater, &E::ripples},
    {OldWater, &E::splashes},       {OldWater, &E::underwater},     {OldWater, &E::swamp},
    {OldWater, &E::worldReflections},
    {OldAir, &E::weather},          {OldAir, &E::heat},             {OldAir, &E::storms},
    {OldAir, &E::sky},              {OldAir, &E::dayCycle},         {OldAir, &E::motes},
    {OldAir, &E::morningFog},       {OldAir, &E::sandDrift},        {OldAir, &E::snowSparkle},
    {OldAir, &E::caveDrips},        {OldAir, &E::shafts},           {OldAir, &E::litDust},
    {OldAir, &E::rainWet},          {OldAir, &E::edgeLand},
    {OldLife, &E::footprints},      {OldLife, &E::butterflies},     {OldLife, &E::birds},
    {OldLife, &E::nightFireflies},  {OldLife, &E::embers},          {OldLife, &E::breath},
    {OldSkills, &E::aura},          {OldSkills, &E::skillsSoft},    {OldSkills, &E::appear},
    {OldSkills, &E::hitFlash},      {OldSkills, &E::skillSparks},
    {OldPicture, &E::grade},        {OldPicture, &E::regionLooks},  {OldPicture, &E::bloom},
    {OldPicture, &E::flare},
};
const Strength kOldStrengths[] = {
    {OldLight, &E::torchBrightness, 200},   {OldLight, &E::torchGlow, 200},         {OldLight, &E::torchLight, 200},
    {OldLight, &E::lampsLight, 200},        {OldLight, &E::fireLightStrength, 200},
    {OldLight, &E::firefliesBrightness, 200},
    {OldLight, &E::mapGlowsBrightness, 200}, {OldLight, &E::raysBrightness, 200},   {OldLight, &E::lavaBrightness, 200},
    {OldLight, &E::portalsStrength, 200},   {OldLight, &E::glowStrength, 200},      {OldLight, &E::glowLight, 100},
    {OldShadows, &E::actorShadowsStrength, 100}, {OldShadows, &E::aoStrength, 100},
    {OldShadows, &E::spriteLightStrength, 200},  {OldShadows, &E::rimStrength, 200},
    {OldSurfaces, &E::reliefStrength, 200}, {OldSurfaces, &E::materialsStrength, 200},
    {OldSurfaces, &E::smoothHillsStrength, 200}, {OldSurfaces, &E::floorReflectStrength, 100},
    {OldNature, &E::grassAmount, 100},      {OldNature, &E::foliageStrength, 200},
    {OldWater, &E::waterWaves, 100},        {OldWater, &E::waterReflections, 100},
    {OldAir, &E::heatStrength, 200},        {OldAir, &E::weatherAmount, 200},       {OldAir, &E::skyStrength, 100},
    {OldAir, &E::shaftsStrength, 200},      {OldAir, &E::motesAmount, 200},
    {OldSkills, &E::auraBrightness, 100},   {OldSkills, &E::skillBrightness, 100},
    {OldSkills, &E::skillSparksAmount, 200},
    {OldPicture, &E::bloomStrength, 200},   {OldPicture, &E::regionLooksStrength, 100},
};

void scale(EffectSettings& e, const Strength& s, int percent) {
    const int k = std::clamp(percent, 0, 200);
    e.*s.value = std::clamp(e.*s.value * k / 100, 0, s.most);
}

}  // namespace

const char* groupWord(int g) {
    static const char* const kWords[kEffectGroups] = {"time",   "weather", "characters", "skill_effects", "fire",
                                                      "lights", "ground",  "plants",     "waters",        "creatures",
                                                      "caves",  "sky",     "screen",     "game_fixes"};
    return g >= 0 && g < kEffectGroups ? kWords[g] : "";
}

const char* oldGroupWord(int g) { return g >= 0 && g < kOldEffectGroups ? kOldWords[g] : ""; }

const Member* members() { return kMembers; }
int memberCount() { return int(sizeof kMembers / sizeof kMembers[0]); }

const Member* memberOf(const std::string& key) {
    const std::string k = key.compare(0, 8, "effects.") == 0 ? key.substr(8) : key;
    for (const Member& m : kMembers)
        if (k == m.key) return &m;
    return nullptr;
}

bool hasStrength(int g) {
    for (const Strength& s : kStrengths)
        if (s.group == g) return true;
    return false;
}

int setLevel(EffectPreset p, int card) {
    switch (p) {
        case EffectPreset::Auto: return std::clamp(card, 1, 3);
        case EffectPreset::Off: return 0;
        case EffectPreset::Weak: return 1;
        case EffectPreset::Medium: return 2;
        case EffectPreset::Strong: return 3;
        case EffectPreset::Custom: break;
    }
    return -1;
}

int cardLevel(bool d3d12, unsigned long long vramMb) {
    if (!d3d12 || vramMb < 4096) return 1;
    return vramMb < 8192 ? 2 : 3;
}

EffectSettings effective(const EffectSettings& in, int card) {
    EffectSettings e = in;
    const int level = setLevel(in.preset, card);
    if (level >= 0) {
        for (const Member& m : kMembers) e.*m.on = m.level > 0 && m.level <= level;
        e.tier = level >= 3 ? 2 : 1;
    }
    if (level == 0) e.nightStrength = 0;  // 2.6.19: the effects off - the clock's night too, the game's own picture
    for (const OldMember& o : kOldMembers)  // 2.6.22: an old file's groups, as they were grouped then
        if (!in.legacyGroups[o.group]) e.*o.on = false;
    for (const Member& m : kMembers)
        if (!in.groups[m.group]) e.*m.on = false;
    for (const Strength& s : kOldStrengths) scale(e, s, in.legacyStrength[s.group]);
    for (const Strength& s : kStrengths) scale(e, s, in.groupStrength[s.group]);
    return e;
}

bool hasLegacy(const EffectSettings& e) {
    for (int g = 0; g < kOldEffectGroups; ++g)
        if (!e.legacyGroups[g] || e.legacyStrength[g] != 100) return true;
    return false;
}

std::vector<settings::IniEdit> settleLegacy(const EffectSettings& e, int card) {
    std::vector<settings::IniEdit> out;
    if (!hasLegacy(e)) return out;
    const int level = setLevel(e.preset, card);
    bool anyOff = false;
    for (bool on : e.legacyGroups) anyOff = anyOff || !on;
    if (anyOff && level > 0) {  // a set: made the player's own with the old groups' effects off (shown() holds them)
        for (const auto& [k, v] : materialize(e, card)) out.push_back({"effects", k, v});
    } else if (anyOff && level < 0) {  // custom: those effects off themselves (the set off: nothing on to keep off)
        for (const OldMember& o : kOldMembers)
            if (!e.legacyGroups[o.group])
                for (const Member& m : kMembers)
                    if (m.on == o.on) out.push_back({"effects", m.key, "0"});
    }
    for (const Strength& s : kOldStrengths) {  // the old strengths into the effects' own numbers
        if (e.legacyStrength[s.group] == 100) continue;
        EffectSettings scaled = e;
        scale(scaled, s, e.legacyStrength[s.group]);
        for (const settings::IniSetting& row : settings::iniSettings())
            if (row.number == s.value) out.push_back({"effects", row.key, std::to_string(scaled.*s.value)});
    }
    for (const settings::IniEdit& gone : oldGroupLinesOut()) out.push_back(gone);
    return out;
}

std::vector<settings::IniEdit> oldGroupLinesOut() {
    std::vector<settings::IniEdit> out;
    for (const std::string& word : {std::string(), std::string("_strength")})
        for (int g = 0; g < kOldEffectGroups; ++g) {
            settings::IniEdit gone{"effects", std::string("group_") + kOldWords[g] + word, ""};
            gone.remove = true;
            out.push_back(gone);
        }
    return out;
}

EffectSettings atMost(const EffectSettings& e, int level) {
    EffectSettings out = e;
    for (const Member& m : kMembers) out.*m.on = out.*m.on && m.level > 0 && m.level <= level;
    if (level < 3 && out.tier > 1) out.tier = 1;  // the remaster's mode is the strong set's
    return out;
}

EffectSettings shown(const EffectSettings& e, int card) {
    EffectSettings all = e;
    for (bool& g : all.groups) g = true;
    const EffectSettings set = effective(all, card);
    EffectSettings out = e;
    for (const Member& m : kMembers) out.*m.on = set.*m.on;
    out.tier = set.tier;
    return out;
}

std::vector<std::pair<std::string, std::string>> materialize(const EffectSettings& e, int card,
                                                             const std::string& except) {
    std::vector<std::pair<std::string, std::string>> out;
    if (setLevel(e.preset, card) < 0) return out;
    const EffectSettings s = shown(e, card);
    for (const Member& m : kMembers)
        if (except != m.key) out.emplace_back(m.key, s.*m.on ? "1" : "0");
    if (except != "tier") out.emplace_back("tier", s.tier == 2 ? "remaster" : "classic");
    if (except != "preset") out.emplace_back("preset", "custom");
    return out;
}

const char* presetToken(EffectPreset p) {
    switch (p) {
        case EffectPreset::Auto: return "auto";
        case EffectPreset::Off: return "off";
        case EffectPreset::Weak: return "weak";
        case EffectPreset::Medium: return "medium";
        case EffectPreset::Strong: return "strong";
        case EffectPreset::Custom: break;
    }
    return "custom";
}

bool presetFrom(const std::string& word, EffectPreset& p) {
    for (EffectPreset q : {EffectPreset::Auto, EffectPreset::Off, EffectPreset::Weak, EffectPreset::Medium,
                           EffectPreset::Strong, EffectPreset::Custom})
        if (word == presetToken(q)) {
            p = q;
            return true;
        }
    return false;
}

}  // namespace uagfx::effect_groups
