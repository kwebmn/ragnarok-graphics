#include "core/effect_groups.hpp"

#include <algorithm>

namespace uagfx::effect_groups {
namespace {

using E = EffectSettings;

// Every switch of the Effects page: its group and the weakest set it is in. The weak set keeps what costs a card
// little (no pass of its own over the picture); the medium adds the passes and the living world; the strong the
// remaster's own world (materials, new trees, reflections, light). 0: the player's alone - a look the player turned
// down, night by the clock beside the day.
const Member kMembers[] = {
    {"smooth_light", Fixes, 1, &E::smoothLight},
    {"soft_particles", Fixes, 1, &E::softParticles},
    {"map_fade", Fixes, 1, &E::mapFade},
    {"no_game_fog", Fixes, 1, &E::noGameFog},  // 2.6.10
    {"no_server_night", Fixes, 1, &E::noServerNight},  // 2.6.13

    {"torch", Light, 1, &E::torch},
    {"smoke", Light, 1, &E::smoke},
    {"lamps", Light, 1, &E::lamps},
    {"fire_light", Light, 1, &E::fireLight},
    {"fireflies", Light, 1, &E::fireflies},
    {"map_glows", Light, 1, &E::mapGlows},
    {"lava", Light, 1, &E::lava},
    {"portals", Light, 1, &E::portals},
    {"rays", Light, 2, &E::rays},
    {"glow", Light, 2, &E::glow},
    {"round_light", Light, 2, &E::roundLight},
    {"night_windows", Light, 3, &E::nightWindows},

    {"actor_shadows", Shadows, 1, &E::actorShadows},
    {"ao", Shadows, 2, &E::ao},
    {"rim_light", Shadows, 2, &E::rimLight},
    {"sprite_light", Shadows, 3, &E::spriteLight},

    {"relief", Surfaces, 2, &E::relief},
    {"floor_reflections", Surfaces, 2, &E::floorReflections},
    {"wet_ground", Surfaces, 2, &E::wetGround},
    {"wet_walls", Surfaces, 2, &E::wetWalls},
    {"glint", Surfaces, 2, &E::glint},
    {"materials", Surfaces, 3, &E::materials},
    {"ground_seams", Surfaces, 3, &E::groundSeams},
    {"smooth_hills", Surfaces, 3, &E::smoothHills},

    {"grass", Nature, 2, &E::grass},
    {"foliage", Nature, 2, &E::foliage},
    {"tree_volume", Nature, 2, &E::treeVolume},
    {"leaf_light", Nature, 2, &E::leafLight},
    {"tree_tints", Nature, 2, &E::treeTints},
    {"gusts", Nature, 2, &E::gusts},
    {"dapple", Nature, 2, &E::dapple},
    {"flowers", Nature, 2, &E::flowers},
    {"dew", Nature, 3, &E::dew},
    {"leaf_fall", Nature, 3, &E::leafFall},
    {"seasons", Nature, 3, &E::seasons},
    {"shared_wind", Nature, 3, &E::sharedWind},


    {"water", Water, 1, &E::water},
    {"ripples", Water, 1, &E::ripples},
    {"shore_foam", Water, 2, &E::shoreFoam},
    {"splashes", Water, 2, &E::splashes},
    {"underwater", Water, 2, &E::underwater},
    {"swamp", Water, 2, &E::swamp},
    {"fish", Water, 3, &E::fish},
    {"world_reflections", Water, 3, &E::worldReflections},

    {"weather", Air, 1, &E::weather},
    {"heat", Air, 2, &E::heat},
    {"storms", Air, 2, &E::storms},
    {"sky", Air, 2, &E::sky},
    {"day_cycle", Air, 2, &E::dayCycle},
    {"motes", Air, 2, &E::motes},
    {"morning_fog", Air, 2, &E::morningFog},
    {"sand_drift", Air, 2, &E::sandDrift},
    {"snow_sparkle", Air, 2, &E::snowSparkle},
    {"cave_drips", Air, 2, &E::caveDrips},
    {"shafts", Air, 3, &E::shafts},
    {"lit_dust", Air, 3, &E::litDust},
    {"rain_wet", Air, 3, &E::rainWet},
    {"edge_land", Air, 3, &E::edgeLand},
    {"night", Air, 0, &E::night},

    {"footprints", Life, 1, &E::footprints},
    {"butterflies", Life, 2, &E::butterflies},
    {"birds", Life, 2, &E::birds},
    {"night_fireflies", Life, 0, &E::nightFireflies},  // 2.6.11: off by default, in no set
    {"moths", Life, 0, &E::moths},
    {"embers", Life, 2, &E::embers},
    {"breath", Life, 2, &E::breath},

    {"aura", Skills, 1, &E::aura},
    {"skills_soft", Skills, 1, &E::skillsSoft},
    {"appear", Skills, 1, &E::appear},
    {"hit_flash", Skills, 1, &E::hitFlash},
    {"skill_sparks", Skills, 2, &E::skillSparks},

    {"grade", Picture, 1, &E::grade},
    {"region_looks", Picture, 2, &E::regionLooks},
    {"bloom", Picture, 3, &E::bloom},
    {"flare", Picture, 3, &E::flare},
};

// A group's strength works on these of its effects (each up to its own most).
struct Strength {
    int group;
    int E::*value;
    int most;
};
const Strength kStrengths[] = {
    {Light, &E::torchBrightness, 200},   {Light, &E::torchGlow, 200},          {Light, &E::torchLight, 200},
    {Light, &E::lampsLight, 200},        {Light, &E::fireLightStrength, 200},  {Light, &E::firefliesBrightness, 200},
    {Light, &E::mapGlowsBrightness, 200}, {Light, &E::raysBrightness, 200},    {Light, &E::lavaBrightness, 200},
    {Light, &E::portalsStrength, 200},   {Light, &E::glowStrength, 200},       {Light, &E::glowLight, 100},
    {Shadows, &E::actorShadowsStrength, 100}, {Shadows, &E::aoStrength, 100},  {Shadows, &E::spriteLightStrength, 200},
    {Shadows, &E::rimStrength, 200},
    {Surfaces, &E::reliefStrength, 200}, {Surfaces, &E::materialsStrength, 200},
    {Surfaces, &E::smoothHillsStrength, 200}, {Surfaces, &E::floorReflectStrength, 100},
    {Nature, &E::grassAmount, 100},      {Nature, &E::foliageStrength, 200},
    {Water, &E::waterWaves, 100},        {Water, &E::waterReflections, 100},
    {Air, &E::heatStrength, 200},
    {Air, &E::weatherAmount, 200},       {Air, &E::skyStrength, 100},          {Air, &E::shaftsStrength, 200},
    {Air, &E::motesAmount, 200},
    {Skills, &E::auraBrightness, 100},   {Skills, &E::skillBrightness, 100},   {Skills, &E::skillSparksAmount, 200},
    {Picture, &E::bloomStrength, 200},   {Picture, &E::regionLooksStrength, 100},

};

}  // namespace

const char* groupWord(int g) {
    static const char* const kWords[kEffectGroups] = {"fixes", "light", "shadows", "surfaces", "nature",
                                                      "water", "air",   "life",    "skills",   "picture"};
    return g >= 0 && g < kEffectGroups ? kWords[g] : "";
}

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
    for (const Member& m : kMembers)
        if (!in.groups[m.group]) e.*m.on = false;
    for (const Strength& s : kStrengths) {
        const int k = std::clamp(in.groupStrength[s.group], 0, 200);
        e.*s.value = std::clamp(e.*s.value * k / 100, 0, s.most);
    }
    return e;
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
