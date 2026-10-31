#include "core/config.hpp"

#include "core/ai/kinds.hpp"
#include "core/daylight.hpp"
#include "core/effect_groups.hpp"
#include "core/seasons.hpp"
#include "core/settings/schema.hpp"
#include "core/sprite_exe.hpp"
#include "core/weather.hpp"

#include <algorithm>

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace uagfx {

const NotifyBox kNotifyBoxes[] = {
    {"on", &NotifySettings::on},           {"whisper", &NotifySettings::whisper},
    {"party", &NotifySettings::party},     {"trade", &NotifySettings::trade},
    {"guild", &NotifySettings::guild},     {"friend", &NotifySettings::friendAsk},
    {"death", &NotifySettings::death},     {"attack", &NotifySettings::attack},  // 3.0.7-beta.6 from here
    {"buff_ending", &NotifySettings::buffEnding}, {"party_death", &NotifySettings::partyDeath},
    {"party_low_hp", &NotifySettings::partyLowHp}, {"party_chat", &NotifySettings::partyChat},
    {"guild_chat", &NotifySettings::guildChat},   {"mention", &NotifySettings::mention},
    {"disconnect", &NotifySettings::disconnect},  {"sound", &NotifySettings::sound},
    {"recovered", &NotifySettings::recovered},  // 3.0.7-beta.7
    {"report_answer", &NotifySettings::reportAnswer},  // 3.0.9-beta.4
};
const size_t kNotifyBoxCount = sizeof kNotifyBoxes / sizeof kNotifyBoxes[0];

namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string lower(std::string s) {
    for (char& ch : s) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool toInt(const std::string& v, int lo, int hi, int& out) {
    char* end = nullptr;
    const long n = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0' || n < lo || n > hi) return false;
    out = int(n);
    return true;
}

// 2.2: [effects] group_<word> (on or off) and group_<word>_strength; false: no group's key
bool groupKey(const std::string& key, const std::string& val, EffectSettings& e) {
    const std::string rest = key.substr(14);
    for (int g = 0; g < kEffectGroups; ++g) {
        const std::string word = effect_groups::groupWord(g);
        if (rest == word) {
            if (val == "1" || val == "0") e.groups[g] = val == "1";
            return true;
        }
        if (rest == word + "_strength") {
            toInt(val, 0, 200, e.groupStrength[g]);
            return true;
        }
    }
    for (int g = 0; g < kOldEffectGroups; ++g) {  // 2.6.22: an old file's ten groups, held until written out
        const std::string word = effect_groups::oldGroupWord(g);
        if (rest == word) {
            if (val == "1" || val == "0") e.legacyGroups[g] = val == "1";
            return true;
        }
        if (rest == word + "_strength") {
            toInt(val, 0, 200, e.legacyStrength[g]);
            return true;
        }
    }
    return false;
}

void setBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "on") out = true;
    if (v == "0" || v == "false" || v == "off") out = false;
}

// One of a few factors ("0", "2", "4", ...); 1 means off, as 0.
void setFactor(const std::string& v, int most, int& out) {
    int n = 0;
    if (!toInt(v, 0, most, n) || (n & (n - 1)) != 0) return;  // 0 or a power of two
    out = n == 1 ? 0 : n;
}

// A window side: 0 (the game's own size) or a real size.
void setSide(const std::string& v, int& out) {
    int n = 0;
    if (toInt(v, 0, 16384, n) && (n == 0 || n >= 320)) out = n;
}

bool isBool(const std::string& v) {
    return v == "1" || v == "true" || v == "on" || v == "0" || v == "false" || v == "off";
}

// One key of [ai]; enabled, budget and denoise say whether the file had a valid one (0.11's keys yield to it).
void parseAi(const std::string& k, const std::string& v, AiSettings& a, bool& enabled, bool& budget, bool& denoise) {
    if (k == "enabled") {
        setBool(v, a.enabled);
        enabled = enabled || isBool(v);
    } else if (k == "ground") {
        setBool(v, a.ground);
    } else if (k == "objects") {
        setBool(v, a.objects);
    } else if (k == "monsters") {
        setBool(v, a.monsters);
    } else if (k == "players") {
        setBool(v, a.players);
    } else if (k == "npcs") {
        setBool(v, a.npcs);
    } else if (k == "items") {
        setBool(v, a.items);
    } else if (k == "effects") {
        setBool(v, a.effects);
    } else if (k == "icons") {
        setBool(v, a.icons);
    } else if (k == "ui_pieces") {
        setBool(v, a.uiPieces);
    } else if (k == "small_pictures") {  // 1.6.3
        setBool(v, a.smallPictures);
    } else if (k == "small_side") {
        toInt(v, 16, 256, a.smallSide);
    } else if (k == "small_model") {
        if (ai::validModel(v)) a.smallModel = v;
    } else if (k.rfind("model_", 0) == 0) {  // 1.6: a group's own; 2.5.28: or its family's
        ai::Group g;
        ai::Family f;
        if (!ai::validModel(v)) return;
        if (ai::parseGroup(k.substr(6), g)) a.models[size_t(g)] = v;
        else if (ai::parseFamily(k.substr(6), f)) a.familyModels[size_t(f)] = v;
    } else if (k == "model" || k == "sprite_model") {
        if (ai::validModel(v)) (k == "model" ? a.model : a.spriteModel) = v;  // 1.5; 2.5.27: any of them
    } else if (k == "denoise") {
        denoise = toInt(v, 0, 100, a.denoise) || denoise;
    } else if (k == "scale") {
        if (v == "4" || v == "2") a.scale = v[0] - '0';
    } else if (k == "budget") {
        budget = toInt(v, 1, 20, a.budget) || budget;
    } else if (k == "min_size") {
        toInt(v, 16, 1024, a.minSize);
    } else if (k == "max_size") {
        toInt(v, 16, 1024, a.maxSize);
    } else if (k == "compress") {
        if (v == "auto" || v == "on" || v == "off") a.compress = v;
    } else if (k == "ahead") {
        setBool(v, a.ahead);
    } else if (k == "dyes") {  // 2.5.23
        setBool(v, a.dyes);
    } else if (k == "generate") {
        setBool(v, a.generate);
    }
}

}  // namespace

bool AiSettings::operator==(const AiSettings& o) const {
    return enabled == o.enabled && ground == o.ground && objects == o.objects && monsters == o.monsters &&
           players == o.players && npcs == o.npcs && items == o.items && dyes == o.dyes &&
           effects == o.effects && icons == o.icons && uiPieces == o.uiPieces && model == o.model &&
           spriteModel == o.spriteModel &&
           denoise == o.denoise && scale == o.scale && budget == o.budget && minSize == o.minSize &&
           maxSize == o.maxSize && ahead == o.ahead && generate == o.generate && compress == o.compress &&
           models == o.models && familyModels == o.familyModels && smallPictures == o.smallPictures &&
           smallSide == o.smallSide &&
           smallModel == o.smallModel;
}

EffectSettings effectsOff() {
    EffectSettings e;  // every switch off (0.39.4: the ones added since 0.27 too), the sliders as they are
    e.smoothLight = e.torch = e.torchFlame = e.smoke = e.lamps = e.fireLight = e.skillColours = e.skillsSoft = false;
    e.aura = e.fireflies = e.mapGlows = e.rays = e.lava = e.actorShadows = e.actorShadowsSpot = e.heat = false;
    e.portals = e.weather = e.glow = e.ao = e.grade = e.water = e.foliage = false;
    e.nightStrength = 0;  // 2.6.19: the night is the clock's - Pause shows none
    e.litDust = false;
    e.sharedWind = false;
    e.nightWindows = false;
    e.rainWet = false;
    e.seasons = false;
    e.spriteLight = false;
    e.worldReflections = false;
    e.smoothHills = false;
    e.ownGround = false;  // 3.0.4-beta.10
    e.ownFarModels = false;  // 3.0.4-beta.14
    e.groundSeams = false;
    e.materials = false;
    e.underwater = false;
    e.caveDrips = false;
    e.splashes = false;
    e.embers = false;
    e.leafFall = false;
    e.birds = false;
    e.butterflies = false;
    e.nightFireflies = false;
    e.swamp = false;
    e.wetWalls = false;
    e.sandDrift = false;
    e.morningFog = false;
    e.dew = false;
    e.flowers = false;
    e.treeTints = false;
    e.leafLight = false;
    e.treeVolume = false;
    e.wetGround = false;
    e.hitFlash = false;
    e.glint = false;
    e.ripples = false;
    e.softParticles = e.rimLight = e.motes = e.relief = e.floorReflections = e.shafts = e.dayCycle = false;
    e.bloom = e.regionLooks = e.footprints = e.storms = e.grass = e.skillSparks = false;
    e.mapFade = e.noGameFog = e.noServerNight = e.roundLight = false;
    // 2.4: the parts of the effects
    e.waterFoam = e.waterGlints = e.caustics = e.waterfalls = e.stepDust = e.seasonSnow = false;
    e.seasonGrass = e.pollen = e.indoorDust = e.rainSplashes = e.desertHeat = false;
    e.moonLight = e.firefliesLight = e.moonBeams = e.lightOrbs = false;  // 2.7.18; 2.7.20; 2.7.25
    e.portalNew = false;  // 2.7.48
    e.mapGlowOrbs = false;  // 3.0.7-beta.1
    e.fireflyDots = false;  // 3.0.7-beta.3
    e.shaftsLamps = false;  // 3.0.7-beta.5
    e.skyRays = e.treeSeasons = e.dragonflies = false;
    e.nightNow = 0;
    e.dayNow = -1;
    return e;
}

int nightAmount(const EffectSettings& e, int hour, int minute) {
    const int strength = std::clamp(e.nightStrength, 0, 100);
    const double t = hour + minute / 60.0;
    double f = 0.0;
    if (t >= 21.0 || t < 5.0)
        f = 1.0;
    else if (t >= 20.0)
        f = t - 20.0;  // dusk
    else if (t < 6.0)
        f = 6.0 - t;  // dawn
    return int(strength * f + 0.5);
}

void applyClock(EffectSettings& e, int localMinute, int serverMinute) {
    const int minute = daylight::minuteOf(e.clock, localMinute, serverMinute);
    e.dayNow = minute;
    e.nightNow = nightAmount(e, minute / 60, minute % 60);  // 2.6.19: the night always the clock's
    // 2.7.16: «Глибока ніч» - past the strength's whole night (night_ps darkens further above 100), whatever it is
    // but 0 % (the night off: the effects' set «off» makes it so)
    if (e.clock == daylight::kClockDeepNight && e.nightStrength > 0) e.nightNow = 200;
}

bool drawnByRenderer(const EffectSettings& e) {
    return e.smoothLight || e.torch || e.smoke || e.lamps || e.fireLight || e.fireflies || e.mapGlows || e.rays ||
           e.lava || e.actorShadows || e.heat || e.portals || e.weather || e.glow || e.ao ||
           e.grade || e.water || e.nightStrength > 0 || e.foliage ||
           e.litDust ||
           e.sharedWind ||
           e.nightWindows ||
           e.rainWet ||
           e.seasons ||
           e.spriteLight ||
           e.worldReflections ||
           e.smoothHills ||
           e.ownGround ||
           e.groundSeams ||
           e.materials ||
           e.underwater ||
           e.caveDrips ||
           e.splashes ||
           e.embers ||
           e.leafFall ||
           e.birds ||
           e.butterflies ||
           e.nightFireflies ||
           e.swamp ||
           e.wetWalls ||
           e.sandDrift ||
           e.morningFog ||
           e.dew ||
           e.flowers ||
           e.treeTints ||
           e.leafLight ||
           e.treeVolume ||
           e.wetGround ||
           e.hitFlash ||
           e.glint ||
           e.ripples ||
           e.softParticles || e.rimLight || e.motes || e.moonLight ||
           e.relief || e.floorReflections || e.shafts || e.dayCycle || e.bloom ||
           e.regionLooks || e.footprints || e.storms || e.grass || e.skillSparks ||
           e.roundLight;
}

bool EffectSettings::operator==(const EffectSettings& o) const {
    // 2.4: every switch and number the settings' schema binds to its field - the list written out by hand (1.8) left
    // out every field added since 1.9, and a change of one of them alone never reached the 64-bit renderer (the
    // seasons off in the console did nothing under DLSS); then the driver's own fields, which the schema has not
    for (const settings::IniSetting& s : settings::iniSettings()) {
        if (s.flag && this->*s.flag != o.*s.flag) return false;
        if (s.number && this->*s.number != o.*s.number) return false;
    }
    return nightNow == o.nightNow && dayNow == o.dayNow && clock == o.clock && season == o.season &&
           weatherKind == o.weatherKind && tier == o.tier && preset == o.preset && grassThin == o.grassThin &&
           std::equal(groups, groups + kEffectGroups, o.groups) &&
           std::equal(groupStrength, groupStrength + kEffectGroups, o.groupStrength) &&
           std::equal(legacyGroups, legacyGroups + kOldEffectGroups, o.legacyGroups) &&
           std::equal(legacyStrength, legacyStrength + kOldEffectGroups, o.legacyStrength);
}

AiPreset aiPresetOf(const AiSettings& a) {
    for (AiPreset p : {AiPreset::Recommended, AiPreset::All})
        if (aiPresetSettings(p, a.enabled) == a) return p;
    return AiPreset::Custom;
}

AiSettings aiPresetSettings(AiPreset p, bool enabled) {
    AiSettings a;
    a.enabled = enabled;
    if (p == AiPreset::All) a.monsters = a.players = a.npcs = a.items = a.effects = true;
    return a;
}

bool aiKind(const AiSettings& a, int kind) {
    switch (kind) {
        case 0: return a.ground;
        case 1: return a.objects;
        case 2: return a.sprites();
        case 3: return a.effects;
        case 4: return a.icons;
        default: return false;
    }
}

namespace {

std::string groupModels(const AiSettings& a) {  // 1.6
    std::string t;
    for (int f = 0; f < ai::kFamilies; ++f)  // 2.5.28: the families' first, as a file has them
        if (!a.familyModels[size_t(f)].empty())
            t += std::string(";model_") + ai::familyName(ai::Family(f)) + "=" + a.familyModels[size_t(f)];
    for (int g = 0; g < ai::kGroups; ++g)
        if (!a.models[size_t(g)].empty())
            t += std::string(";model_") + ai::groupName(ai::Group(g)) + "=" + a.models[size_t(g)];
    t += std::string(";small_pictures=") + (a.smallPictures ? "1" : "0") + ";small_side=" +
         std::to_string(a.smallSide) + ";small_model=" + a.smallModel;  // 1.6.3
    return t;
}

}  // namespace

std::string aiArgument(const AiSettings& a) {
    const auto b = [](bool v) { return v ? "1" : "0"; };
    return std::string("enabled=") + b(a.enabled) + ";ground=" + b(a.ground) + ";objects=" + b(a.objects) +
           ";monsters=" + b(a.monsters) + ";players=" + b(a.players) + ";npcs=" + b(a.npcs) + ";items=" + b(a.items) +
           ";effects=" + b(a.effects) + ";icons=" + b(a.icons) +
           ";ui_pieces=" + b(a.uiPieces) + ";model=" + a.model +
           ";sprite_model=" + a.spriteModel + ";denoise=" + std::to_string(a.denoise) +
           ";scale=" + std::to_string(a.scale) + ";budget=" + std::to_string(a.budget) +
           ";min_size=" + std::to_string(a.minSize) + ";max_size=" + std::to_string(a.maxSize) +
           ";ahead=" + b(a.ahead) + ";generate=" + b(a.generate) + ";compress=" + a.compress + groupModels(a);
}

AiSettings parseAiArgument(const std::string& word) {
    std::string text = "[ai]\n";
    for (char ch : word) text += ch == ';' ? '\n' : ch;
    return parseConfig(text).ai;
}

Config parseConfig(const std::string& text) {
    Config c;
    // 0.11's keys count only where [ai] has no valid key of its own, wherever they stand in the file.
    AiSettings legacy;
    bool oldEnabled = false, oldBudget = false, oldDenoise = false;
    bool newEnabled = false, newBudget = false, newDenoise = false;
    // 0.13-0.15's [ai] sprites: all four kinds of sprites, where none of their own keys is in the file.
    int oldSprites = -1;
    bool newSprites = false;
    bool buffsSet = false, characterSet = false;  // 3.0.11-beta.37: the new interface's parts asked for by name
    bool targetSet = false, partySet = false;     // 3.0.11-beta.38, 3.0.11-beta.39
    bool trackerSet = false;                      // 3.0.11-beta.70
    bool hotbarSet = false, chatSet = false;      // 3.0.11-beta.40, 3.0.11-beta.41
    bool castSet = false;                         // 3.0.11-beta.48
    bool itemsSet = false, statusSet = false, skillsSet = false, equipSet = false, npcSet = false;     // 3.0.11-beta.42
    bool shopSet = false;                         // 3.0.11-beta.100
    std::istringstream in(text);
    std::string line;
    std::string section;
    bool first = true;
    while (std::getline(in, line)) {
        if (first && line.rfind("\xEF\xBB\xBF", 0) == 0) line = line.substr(3);  // saved by Notepad with a BOM
        first = false;
        const size_t comment = line.find(';');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = section + "." + lower(trim(line.substr(0, eq)));
        const std::string raw = trim(line.substr(eq + 1));  // as written: a graphics card's name (0.50.2)
        const std::string val = lower(raw);
        // 2.2.2: a switch or a number of [effects] the settings' schema binds to its field - read by its entry there
        // (a number from its min to its max, else the default stays), not by a branch of its own below
        if (section == "effects") {
            if (const settings::IniSetting* s = settings::findSetting(key)) {
                if (s->flag) {
                    setBool(val, c.effects.*s->flag);
                    continue;
                }
                if (s->number) {
                    toInt(val, s->min, s->max, c.effects.*s->number);
                    continue;
                }
            }
        }
        if (key == "renderer.backend") {
            // 2.2: dgVoodoo is gone - the uaRO renderer always; the old line is read and left alone
        } else if (key == "renderer.api") {
            if (val == "d3d12") c.api = Api::D3d12;
            if (val == "d3d11") c.api = Api::D3d11;
            if (val == "auto") c.api = Api::Auto;
            if (val == "system") c.api = Api::System;  // 2.12
        } else if (key == "renderer.bridge") {
            setBool(val, c.bridge);
        } else if (key == "renderer.render_thread") {  // 2.5.12
            setBool(val, c.renderThread);
        } else if (key == "report.auto") {
            setBool(val, c.reportAuto);
        } else if (key == "pad.enabled") {  // 2.9
            setBool(val, c.padEnabled);
        } else if (key == "launcher.direct_start") {  // 2.11
            setBool(val, c.directStart);
        } else if (key == "client.cps") {  // 3.0.1-beta.5
            setBool(val, c.clientCps);
        } else if (key == "client.bink") {
            setBool(val, c.clientBink);
        } else if (key == "client.ijl") {  // 3.0.1-beta.6
            setBool(val, c.clientIjl);
        } else if (key == "client.mss") {  // 3.0.1-beta.7
            setBool(val, c.clientMss);
        } else if (key == "client.roext") {  // 3.0.1-beta.8: ROExt's work in the driver
            setBool(val, c.clientRoext);
        } else if (key == "client.updates") {  // 3.0.2-beta.5: the client's graphics updates
            setBool(val, c.clientUpdates);
        } else if (key == "roext.window_lock") {
            setBool(val, c.roext.windowLock);
        } else if (key == "roext.window_on_top") {
            setBool(val, c.roext.windowOnTop);
        } else if (key == "roext.window_x") {
            toInt(val, -100000, 100000, c.roext.windowX);
        } else if (key == "roext.window_y") {
            toInt(val, -100000, 100000, c.roext.windowY);
        } else if (key == "roext.window_width") {
            toInt(val, 0, 100000, c.roext.windowWidth);
        } else if (key == "roext.window_height") {
            toInt(val, 0, 100000, c.roext.windowHeight);
        } else if (key == "roext.mouse_buttons") {
            setBool(val, c.roext.mouseButtons);
        } else if (key == "roext.alt_f4") {
            setBool(val, c.roext.altF4);
        } else if (key == "roext.minimize_key") {
            setBool(val, c.roext.minimizeKey);
        } else if (key == "roext.free_cpu") {
            toInt(val, 0, 1000, c.roext.freeCpu);
        } else if (key == "roext.code_page") {
            toInt(val, -1, 65535, c.roext.codePage);
        } else if (key == "windows.title_name") {  // 3.0.3-beta.2: several game windows
            setBool(val, c.windowTitleName);
        } else if (key == "windows.remember_place") {
            setBool(val, c.windowRememberPlace);
        } else if (key == "windows.others_light") {  // 3.0.3-beta.3
            setBool(val, c.windowOthersLight);
        } else if (key == "windows.others_music") {
            setBool(val, c.windowOthersMusic);
        } else if (key == "windows.others_sounds") {  // 3.0.3-beta.9
            setBool(val, c.windowOthersSounds);
        } else if (key == "windows.start_count") {  // 3.0.4-beta.4
            toInt(val, 1, 8, c.windowStartCount);
        } else if (key == "windows.switch_keys") {  // 3.0.3-beta.6
            if (val == "all") c.windowKeys = WindowKeys::All;
            if (val == "back") c.windowKeys = WindowKeys::Back;
            if (val == "off") c.windowKeys = WindowKeys::Off;
        } else if (key == "panel.show") {  // 3.0.3-beta.8: the game windows' panel
            setBool(val, c.panelShow);
        } else if (key == "interface.minimap") {  // 3.0.11-beta.1: the new interface's minimap prototype
            setBool(val, c.newMinimap);
        } else if (key == "interface.minimap_npcs") {  // 3.0.11-beta.6: its hints
            setBool(val, c.minimapNpcs);
        } else if (key == "interface.minimap_warps") {
            setBool(val, c.minimapWarps);
        } else if (key == "interface.minimap_reach") {  // 3.0.11-beta.65: where one can walk, an experiment
            setBool(val, c.minimapReach);
        } else if (key == "interface.minimap_hd") {  // 3.0.11-beta.72: the maps' pictures four times bigger
            setBool(val, c.minimapHd);
        } else if (key == "interface.pack_icons") {  // 3.0.11-beta.74: the panels' icons from the AI pack
            setBool(val, c.packIcons);
        } else if (key == "interface.lock_layout") {  // 3.0.11-beta.77: the parts kept where they are
            setBool(val, c.lockLayout);
        } else if (key == "interface.hotbar_per_character") {  // 3.0.11-beta.78: each character's own bar
            setBool(val, c.hotbarPerCharacter);
        } else if (key == "interface.pad_hints") {  // 3.0.11-beta.86: the controller's buttons on the interface
            setBool(val, c.padHints);
        } else if (key == "interface.target_ring") {  // 3.0.11-beta.87: the target's ring on the ground
            if (val == "off") c.targetRing = TargetRing::Off;
            if (val == "pad") c.targetRing = TargetRing::Pad;
            if (val == "always") c.targetRing = TargetRing::Always;
        } else if (key == "interface.mob_hp") {  // 3.0.11-beta.95: the monsters' HP bars
            if (val == "off") c.mobHp = MobHp::Off;
            if (val == "target") c.mobHp = MobHp::Target;
            if (val == "wounded") c.mobHp = MobHp::Wounded;
            if (val == "all") c.mobHp = MobHp::All;
        } else if (key == "interface.pad_tiles") {  // 3.0.11-beta.90: Start opens the windows' tile
            setBool(val, c.padTiles);
        } else if (key == "interface.pad_walk") {  // 3.0.11-beta.88: the left stick without the mouse
            if (val == "mouse") c.padWalk = PadWalk::Mouse;
            if (val == "arrow") c.padWalk = PadWalk::Arrow;
        } else if (key == "interface.walk_arrow") {  // 3.0.11-beta.103: the look of the stick's arrow
            for (const WalkLook w : {WalkLook::Off, WalkLook::Chevrons, WalkLook::Slim, WalkLook::Concept,
                                     WalkLook::Ring, WalkLook::Dots, WalkLook::Cell})
                if (val == walkLookToken(w)) c.walkArrow = w;
        } else if (key == "interface.minimap_mobs") {
            setBool(val, c.minimapMobs);
        } else if (key == "interface.minimap_way") {  // 3.0.11-beta.33: the way on the ground
            if (val == "steps") c.minimapWay = WayOnGround::Steps;
            if (val == "line") c.minimapWay = WayOnGround::Line;
            if (val == "off") c.minimapWay = WayOnGround::Off;
        } else if (key == "interface.buffs") {  // 3.0.11-beta.35: the buffs' column
            buffsSet = val == "off" || val == "column" || val == "row";
            if (val == "off") c.buffs = BuffsLook::Off;
            if (val == "column") c.buffs = BuffsLook::Column;
            if (val == "row") c.buffs = BuffsLook::Row;
        } else if (key == "interface.buffs_overflow") {  // 3.0.11-beta.63: what the buffs' column does not hold
            if (val == "more") c.buffsOverflow = BuffsOverflow::More;
            if (val == "column") c.buffsOverflow = BuffsOverflow::Column;
        } else if (key == "interface.items") {  // 3.0.11-beta.42: «Речі»
            itemsSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.itemsWindow);
        } else if (key == "interface.status") {  // 3.0.11-beta.42: «Характеристики»
            statusSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.statusWindow);
        } else if (key == "interface.skills") {  // 3.0.11-beta.96: «Навички»
            skillsSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.skillsWindow);
        } else if (key == "interface.equip") {  // 3.0.11-beta.97: «Спорядження»
            equipSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.equipWindow);
        } else if (key == "interface.npc_talk") {  // 3.0.11-beta.98: the NPC talk
            npcSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.npcWindow);
        } else if (key == "interface.shop") {  // 3.0.11-beta.100: the NPCs' shops
            shopSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.shopWindow);
        } else if (key == "interface.dim_world") {  // 3.0.11-beta.49: the world dimmed under the windows (off: .55)
            setBool(val, c.dimWorld);
        } else if (key == "interface.cast") {  // 3.0.11-beta.48: the cast's label over the hero
            castSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.castLabel);
        } else if (key == "interface.animations") {  // 3.0.11-beta.46: the new interface's motion
            setBool(val, c.animations);
        } else if (key == "interface.size") {  // 3.0.11-beta.54: the new interface's size (auto or per cent)
            int n = 0;
            if (val == "auto") c.uiSize = 0;
            else if (toInt(val, -100000, 100000, n)) c.uiSize = std::clamp(n, 50, 150);
        } else if (key == "interface.text_size") {  // 3.0.11-beta.67: the reading windows' text, per cent
            int n = 0;
            if (toInt(val, -100000, 100000, n)) c.textSize = std::clamp(n, 80, 150);
        } else if (key == "interface.low_hp_edge") {  // 3.0.11-beta.50: the screen's edges red at low HP
            int n = 0;
            if (toInt(val, -100000, 100000, n)) c.lowHpEdge = std::clamp(n, 0, 100);
        } else if (key == "interface.panel_opacity" || key == "interface.window_opacity") {  // 3.0.11-beta.47
            int n = 0;
            if (toInt(val, -100000, 100000, n)) {
                if (key == "interface.panel_opacity") c.hudOpacity = std::clamp(n, 50, 100);
                else c.windowOpacity = std::clamp(n, 80, 100);
            }
        } else if (key == "interface.chat") {  // 3.0.11-beta.41: the chat
            chatSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.chat);
        } else if (key == "interface.hotbar") {  // 3.0.11-beta.40: the hotkeys' bar
            hotbarSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.hotbar);
        } else if (key == "interface.party") {  // 3.0.11-beta.39: the party's panel
            partySet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.partyPanel);
        } else if (key == "interface.tracker") {  // 3.0.11-beta.70: the pinned quests, way and items
            trackerSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.tracker);
        } else if (key == "interface.target") {  // 3.0.11-beta.38: the target's card
            targetSet = val == "0" || val == "1" || val == "on" || val == "off" || val == "true" || val == "false";
            setBool(val, c.targetCard);
        } else if (key == "interface.character") {  // 3.0.11-beta.36: the character block
            characterSet = val == "off" || val == "bars" || val == "rings";
            if (val == "off") c.character = CharacterLook::Off;
            if (val == "bars") c.character = CharacterLook::Bars;
            if (val == "rings") c.character = CharacterLook::Rings;
        } else if (key == "interface.reference") {  // 3.0.11-beta.7: «Довідник»
            setBool(val, c.referenceWindow);
        } else if (key == "panel.hidden") {
            setBool(val, c.panelHidden);
        } else if (key == "panel.layout") {
            if (val == "row") c.panelLayout = PanelLayout::Row;
            if (val == "column") c.panelLayout = PanelLayout::Column;
            if (val == "icons") c.panelLayout = PanelLayout::Icons;  // 3.0.3-beta.11
        } else if (key == "panel.size" || key == "panel.opacity") {
            int n = 0;
            if (toInt(val, -100000, 100000, n)) {
                if (key == "panel.size") c.panelSize = std::clamp(n, 75, 150);
                else c.panelOpacity = std::clamp(n, 40, 100);
            }
        } else if (key == "panel.place") {
            c.panelPlace = val;
        } else if (key == "camera.indoor") {  // 3.0.4-beta.5
            setBool(val, c.cameraIndoor);
        } else if (key == "camera.zoom_limit") {  // 3.0.4-beta.17
            setBool(val, c.cameraZoomLimit);
        } else if (key == "buffs.timers") {  // 3.0.4-beta.6
            setBool(val, c.buffTimers);
        } else if (key.compare(0, 7, "notify.") == 0) {  // 3.0.4-beta.3
            const std::string k = key.substr(7);
            for (size_t i = 0; i < kNotifyBoxCount; ++i)
                if (k == kNotifyBoxes[i].key) setBool(val, c.notify.*kNotifyBoxes[i].field);
            if (k == "buff_seconds")  // 3.0.7-beta.6
                c.notify.buffSeconds = std::clamp(std::atoi(val.c_str()), kBuffSecondsLeast, kBuffSecondsMost);
            else if (k == "party_hp")
                c.notify.partyHp = std::clamp(std::atoi(val.c_str()), kPartyHpLeast, kPartyHpMost);
            else if (k == "recovered_percent")  // 3.0.7-beta.7
                c.notify.recoveredPercent = std::clamp(std::atoi(val.c_str()), kRecoveredLeast, kRecoveredMost);
        } else if (key == "utility.mode") {  // 2.15: the Developer mode's keys
            c.developer = lower(val) == "developer";
        } else if (key == "renderer.safe_mode") {  // 2.12
            setBool(val, c.safeMode);
        } else if (key == "stats.send") {  // 2.2.1
            setBool(val, c.statsSend);
        } else if (key == "stats.id") {
            if (val.size() >= 8 && val.size() <= 32 && val.find_first_not_of("0123456789abcdef") == std::string::npos)
                c.statsId = val;
        } else if (key == "renderer.gpu") {
            c.gpu = val == "auto" ? "" : raw;
        } else if (key == "renderer.bridge_present") {
            if (val == "auto" || val == "window" || val == "composition") c.bridgePresent = val;
        } else if (key == "renderer.remix") {
            setBool(val, c.remix);
        } else if (key == "renderer.bridge_wine") {  // 3.0.4-beta.8
            setBool(val, c.bridgeWine);
        } else if (key == "renderer.bridge_dpi_scale") {
            setBool(val, c.bridgeDpiScale);
        } else if (key == "display.mode") {
            if (val == "auto") c.mode = DisplayMode::Auto;
            if (val == "window") c.mode = DisplayMode::Window;
            if (val == "borderless") c.mode = DisplayMode::Borderless;
        } else if (key == "display.high_resolution") {
            setBool(val, c.highResolution);
        } else if (key == "display.ui_filter") {
            if (val == "sharp") c.uiFilter = UiFilter::Sharp;
            if (val == "smooth") c.uiFilter = UiFilter::Smooth;
        } else if (key == "display.window_art") {
            if (val == "xbr") c.windowArt = WindowArt::Xbr;
            if (val == "ai") c.windowArt = WindowArt::Ai;
            if (val == "pixels") c.windowArt = WindowArt::Pixels;
        } else if (key == "display.crisp_text") {
            setBool(val, c.crispText);
        } else if (key == "quality.render_scale") {
            toInt(val, 50, 200, c.renderScale);  // 0.58: from 50 % (drawn smaller, made larger sharply)
        } else if (key == "quality.world_scale") {
            toInt(val, 50, 100, c.worldScale);  // 2.5.16
        } else if (key == "quality.full_colour") {
            setBool(val, c.fullColour);  // 2.5.18
        } else if (key == "quality.super_light") {
            setBool(val, c.superLight);  // 2.5.20
        } else if (key == "quality.sharpen") {
            toInt(val, 0, 100, c.sharpen);
        } else if (key == "quality.msaa") {
            setFactor(val, 8, c.msaa);
        } else if (key == "quality.alpha_to_coverage") {
            setBool(val, c.alphaToCoverage);
        } else if (key == "quality.dlss") {
            if (val == "off" || val == "dlaa" || val == "quality" || val == "balanced" || val == "performance" ||
                val == "ultra" || val == "custom")
                c.dlss = val;
        } else if (key == "quality.dlss_scale") {  // 2.4.2
            toInt(val, 33, 100, c.dlssScale);
        } else if (key == "quality.dlss_width") {
            toInt(val, 0, 16384, c.dlssWidth);
        } else if (key == "quality.dlss_height") {
            toInt(val, 0, 16384, c.dlssHeight);
        } else if (key == "quality.dlss_model") {
            if (val == "auto" || val == "k" || val == "j" || val == "l" || val == "m") c.dlssModel = val;
        } else if (key == "quality.taa") {
            setBool(val, c.taa);
        } else if (key == "quality.dlss_jitter") {
            if (val == "normal" || val == "inverted") c.dlssJitterInverted = val == "inverted";
        } else if (key == "quality.dlss_motion") {
            if (val == "normal" || val == "inverted") c.dlssMotionInverted = val == "inverted";
        } else if (key == "quality.anisotropy") {
            setFactor(val, 16, c.anisotropy);
        } else if (key == "quality.ai_textures") {
            setBool(val, legacy.enabled);
            oldEnabled = oldEnabled || isBool(val);
        } else if (key == "quality.ai_budget") {
            oldBudget = toInt(val, 1, 20, legacy.budget) || oldBudget;
        } else if (key == "quality.ai_denoise") {
            oldDenoise = toInt(val, 0, 100, legacy.denoise) || oldDenoise;
        } else if (key == "ai.sprites") {
            if (isBool(val)) oldSprites = val == "1" || val == "true" || val == "on" ? 1 : 0;
        } else if (key.compare(0, 3, "ai.") == 0) {
            const std::string k = key.substr(3);
            newSprites = newSprites || k == "monsters" || k == "players" || k == "npcs" || k == "items";
            parseAi(k, val, c.ai, newEnabled, newBudget, newDenoise);
        } else if (key == "effects.sun_shadows" || key == "effects.sun_shadows_strength" ||
                   key == "effects.real_shadows") {
            // 2.1.4: the moving shadows are gone (the map's painted ones are enough); 2.2: their code too
        } else if (key == "effects.preset") {  // 2.2
            effect_groups::presetFrom(val, c.effects.preset);
        } else if (key.compare(0, 14, "effects.group_") == 0 && groupKey(key, val, c.effects)) {  // 2.2
        } else if (key == "effects.clock") {  // 2.6.11
            static const char* const kWords[daylight::kClocks] = {
                "auto", "server", "dawn", "morning", "day", "sunset", "night", "deep_night"};  // 2.6.19, 2.7.16
            for (int k = 0; k < daylight::kClocks; ++k)
                if (val == kWords[k]) c.effects.clock = k;
        } else if (key == "effects.season") {  // 2.6.22
            static const char* const kWords[seasons::kSeasons] = {"auto", "spring", "summer", "autumn", "winter"};
            for (int k = 0; k < seasons::kSeasons; ++k)
                if (val == kWords[k]) c.effects.season = k;
        } else if (key == "effects.weather_kind") {  // 2.6.22
            static const char* const kWords[weather::kWeathers] = {"auto", "clear", "rain", "snow", "leaves", "sand"};
            for (int k = 0; k < weather::kWeathers; ++k)
                if (val == kWords[k]) c.effects.weatherKind = k;
        } else if (key == "effects.tier") {  // 1.8
            if (val == "auto") c.effects.tier = 0;
            if (val == "classic") c.effects.tier = 1;
            if (val == "remaster") c.effects.tier = 2;
        } else if (key == "effects.full_crowns") {
            // 2.2: the old crowns are gone (the new trees grow whole ones) - read and left alone
        } else if (key == "display.monitor") {
            toInt(val, 0, 16, c.monitor);
        } else if (key == "display.window_width") {
            setSide(val, c.windowWidth);
        } else if (key == "display.window_height") {
            setSide(val, c.windowHeight);
        } else if (key == "frames.vsync") {
            setBool(val, c.vsync);
        } else if (key == "frames.frame_generation") {
            if (val == "off") c.frameGeneration = 0;
            if (val == "2x" || val == "3x" || val == "4x") c.frameGeneration = val[0] - '0';
        } else if (key == "display.hdr") {
            setBool(val, c.hdr);
        } else if (key == "display.ui_native") {
            setBool(val, c.uiNative);
        } else if (key == "frames.fps_counter") {
            setBool(val, c.fpsCounter);
        } else if (key == "frames.fps_limit") {
            toInt(val, 0, 1000, c.fpsLimit);
        } else if (key == "frames.save_power") {
            setBool(val, c.savePower);
        } else if (key == "frames.battery_saver") {
            setBool(val, c.batterySaver);  // 2.5.20
        } else if (key == "frames.battery_fps") {
            toInt(val, 15, 60, c.batteryFps);
        } else if (key == "frames.keep_fps") {
            toInt(val, 0, 240, c.keepFps);
        } else if (key == "frames.allow_tearing") {
            setBool(val, c.allowTearing);
        } else if (key == "frames.reflex") {  // 2.5.26
            setBool(val, c.reflex);
        } else if (key == "debug.state_cache") {
            setBool(val, c.stateCache);
        } else if (key == "debug.fast_pick") {
            setBool(val, c.fastPick);
        } else if (key == "debug.fast_ftol") {
            setBool(val, c.fastFtol);  // 2.5.20
        } else if (key == "debug.fast_textures") {
            setBool(val, c.fastTextures);
        } else if (key == "sound.rate") {  // 0.91
            if (val == "0" || val == "44100" || val == "48000") c.sound.rate = std::atoi(val.c_str());
        } else if (key == "sound.voices") {
            setBool(val, c.sound.voices);
        } else if (key == "sound.background") {
            toInt(val, 0, 100, c.sound.background);
        } else if (key == "sound.limiter") {
            setBool(val, c.sound.limiter);
        } else if (key == "sound.stereo") {  // 0.92
            toInt(val, 0, 100, c.sound.stereo);
        } else if (key == "sound.stereo_flip") {
            setBool(val, c.sound.stereoFlip);
        } else if (key == "sound.steal") {
            setBool(val, c.sound.steal);
        } else if (key == "sound.crossfade") {
            setBool(val, c.sound.crossfade);
        } else if (key == "sound.engine") {  // 1.1
            setBool(val, c.sound.engine);
        } else if (key == "sound.reverb") {
            toInt(val, 0, 100, c.sound.reverb);
        } else if (key == "sound.headphones") {  // 1.2
            setBool(val, c.sound.headphones);
        } else if (key == "sound.duck") {  // 1.3
            setBool(val, c.sound.duck);
        } else if (key == "sound.ambience") {  // 3.0.4-beta.7
            setBool(val, c.sound.ambience);
        } else if (key == "sound.ambience_volume") {
            toInt(val, 0, 100, c.sound.ambienceVolume);
        } else if (key == "sound.output") {  // 3.0.4-beta.1: an endpoint id, as written
            c.sound.output = val == "auto" ? "" : raw;
        } else if (key == "debug.merge_draws") {
            setBool(val, c.mergeDraws);
        } else if (key == "debug.shader_variants") {  // 2.5.16
            setBool(val, c.shaderVariants);
        } else if (key == "debug.profile") {
            setBool(val, c.profile);
        } else if (key == "debug.xray") {  // 2.3
            setBool(val, c.xray);
        } else if (key == "debug.seams_view") {  // 2.7.8
            if (val == "0" || val == "1" || val == "2") c.seamsView = val[0] - '0';
        } else if (key == "debug.weak_card") {  // 3.0.4-beta.35
            setBool(val, c.weakCard);
        } else if (key == "debug.log") {
            if (val == "normal") c.logLevel = LogLevel::Info;
            if (val == "verbose") c.logLevel = LogLevel::Debug;
        }
    }
    if (!newSprites && oldSprites >= 0) c.ai.monsters = c.ai.players = c.ai.npcs = c.ai.items = oldSprites == 1;
    if (!newEnabled && oldEnabled) c.ai.enabled = legacy.enabled;
    if (!newBudget && oldBudget) c.ai.budget = legacy.budget;
    if (!newDenoise && oldDenoise) c.ai.denoise = legacy.denoise;
    c.aiNow = c.ai;
    // 3.0.11-beta.37 (R-TWU5W8 «не появились ни касты ни новый блок персонажа»): the new minimap on - the new
    // interface's parts with it, unless the file names them (the minimap stands over the game's buff icons)
    if (c.newMinimap && !buffsSet) c.buffs = BuffsLook::Column;
    if (c.newMinimap && !characterSet) c.character = CharacterLook::Bars;
    if (c.newMinimap && !targetSet) c.targetCard = true;  // 3.0.11-beta.38
    if (c.newMinimap && !partySet) c.partyPanel = true;   // 3.0.11-beta.39
    if (c.newMinimap && !trackerSet) c.tracker = true;    // 3.0.11-beta.70
    if (c.newMinimap && !hotbarSet) c.hotbar = true;      // 3.0.11-beta.40
    if (c.newMinimap && !castSet) c.castLabel = true;     // 3.0.11-beta.48
    if (c.newMinimap && !chatSet) c.chat = true;          // 3.0.11-beta.41
    if (c.newMinimap && !itemsSet) c.itemsWindow = true;  // 3.0.11-beta.42
    if (c.newMinimap && !statusSet) c.statusWindow = true;
    if (c.newMinimap && !skillsSet) c.skillsWindow = true;  // 3.0.11-beta.96
    if (c.newMinimap && !equipSet) c.equipWindow = true;    // 3.0.11-beta.97
    if (c.newMinimap && !npcSet) c.npcWindow = true;        // 3.0.11-beta.98
    if (c.newMinimap && !shopSet) c.shopWindow = true;      // 3.0.11-beta.100
    return c;
}

uint8_t aiKindsShown(const AiSettings& a) {
    if (!a.on()) return 0;
    return uint8_t((a.ground ? 1 : 0) | (a.objects ? 2 : 0) | (a.sprites() ? 4 : 0) | (a.effects ? 8 : 0) |
                   (a.icons ? 16 : 0));
}

uint8_t aiSpritesShown(const AiSettings& a) {
    if (!a.on()) return 0;
    using sprite_exe::Source;
    const auto bit = [](Source s) { return 1 << int(s); };
    return uint8_t((a.monsters ? bit(Source::Monster) : 0) | (a.players ? bit(Source::Player) : 0) |
                   (a.npcs ? bit(Source::Npc) : 0) | (a.items ? bit(Source::Item) : 0));
}

bool aiCellShown(uint8_t source, uint8_t kinds, uint8_t sprites) {
    const auto kind = [&](ai::Kind k) { return (kinds >> int(k) & 1) != 0; };
    switch (sprite_exe::Source(source)) {
        case sprite_exe::Source::Monster:
        case sprite_exe::Source::Player:
        case sprite_exe::Source::Npc:
        case sprite_exe::Source::Item: return (sprites >> source & 1) != 0;
        case sprite_exe::Source::Effect: return kind(ai::Kind::Effects);
        case sprite_exe::Source::Interface: return kind(ai::Kind::Icons);
        case sprite_exe::Source::Unknown: break;
    }
    return kind(ai::Kind::Sprites);
}

bool aiWaits(const std::string& key, const AiSettings& started, const AiSettings& now) {
    const auto on = [&](const AiSettings& a) {
        if (key == "ai.enabled") return a.enabled;
        if (key == "ai.ground") return a.ground && a.on();
        if (key == "ai.objects") return a.objects && a.on();
        if (key == "ai.effects") return a.effects && a.on();
        if (key == "ai.icons") return a.icons && a.on();
        if (key == "ai.monsters") return a.monsters && a.on();
        if (key == "ai.players") return a.players && a.on();
        if (key == "ai.npcs") return a.npcs && a.on();
        if (key == "ai.items") return a.items && a.on();
        return false;
    };
    return on(now) && !on(started);
}


const char* apiToken(Api a) {
    return a == Api::D3d11 ? "d3d11" : a == Api::Auto ? "auto" : a == Api::System ? "system" : "d3d12";
}

const char* displayModeToken(DisplayMode m) {
    switch (m) {
        case DisplayMode::Window: return "window";
        case DisplayMode::Borderless: return "borderless";
        case DisplayMode::Auto: break;
    }
    return "auto";
}

const char* uiFilterToken(UiFilter f) { return f == UiFilter::Smooth ? "smooth" : "sharp"; }

const char* windowKeysToken(WindowKeys k) {
    return k == WindowKeys::Back ? "back" : k == WindowKeys::Off ? "off" : "all";
}

const char* padWalkToken(PadWalk w) { return w == PadWalk::Arrow ? "arrow" : "mouse"; }

const char* walkLookToken(WalkLook w) {
    switch (w) {
        case WalkLook::Off: return "off";
        case WalkLook::Slim: return "slim";
        case WalkLook::Concept: return "concept";
        case WalkLook::Ring: return "ring";
        case WalkLook::Dots: return "dots";
        case WalkLook::Cell: return "cell";
        case WalkLook::Chevrons: break;
    }
    return "chevrons";
}

const char* targetRingToken(TargetRing r) {
    return r == TargetRing::Off ? "off" : r == TargetRing::Always ? "always" : "pad";
}

const char* mobHpToken(MobHp m) {
    return m == MobHp::Off ? "off" : m == MobHp::Target ? "target" : m == MobHp::All ? "all" : "wounded";
}

const char* wayOnGroundToken(WayOnGround w) {
    return w == WayOnGround::Line ? "line" : w == WayOnGround::Off ? "off" : "steps";
}

const char* buffsLookToken(BuffsLook b) {
    return b == BuffsLook::Column ? "column" : b == BuffsLook::Row ? "row" : "off";
}

const char* buffsOverflowToken(BuffsOverflow b) { return b == BuffsOverflow::Column ? "column" : "more"; }

const char* characterLookToken(CharacterLook c) {
    return c == CharacterLook::Bars ? "bars" : c == CharacterLook::Rings ? "rings" : "off";
}

const char* panelLayoutToken(PanelLayout l) {
    return l == PanelLayout::Icons ? "icons" : l == PanelLayout::Column ? "column" : "row";
}

const char* windowArtToken(WindowArt a) {
    return a == WindowArt::Ai ? "ai" : a == WindowArt::Pixels ? "pixels" : "xbr";
}

}  // namespace uagfx
