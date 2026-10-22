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
           weatherKind == o.weatherKind && tier == o.tier && preset == o.preset &&
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

const char* windowArtToken(WindowArt a) {
    return a == WindowArt::Ai ? "ai" : a == WindowArt::Pixels ? "pixels" : "xbr";
}

}  // namespace uagfx
