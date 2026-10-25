#include "core/effect_costs.hpp"

#include <cstdio>
#include <sstream>

namespace uagfx::effect_costs {
namespace {

struct Costly {
    const char* key;   // the switch, without "effects."
    const char* pass;  // its timed pass (device.cpp kEffectNames' short names)
    bool EffectSettings::*on;
};

// The usually costliest first: what keep_fps turns off while nothing is measured yet.
const Costly kCostly[] = {
    {"grass", "grass", &EffectSettings::grass},
    {"floor_reflections", "floor", &EffectSettings::floorReflections},
    {"ao", "ao", &EffectSettings::ao},
    {"shafts", "shafts", &EffectSettings::shafts},
    {"glow", "glow", &EffectSettings::glow},
    {"bloom", "bloom", &EffectSettings::bloom},
    {"actor_shadows", "actor_shadows", &EffectSettings::actorShadows},
    {"water", "water", &EffectSettings::water},
    {"heat", "heat", &EffectSettings::heat},
    {"weather", "weather", &EffectSettings::weather},
    {"motes", "weather", &EffectSettings::motes},
    {"lamps", "lights", &EffectSettings::lamps},
    {"grade", "grade", &EffectSettings::grade},
    // 2.6.8: the low mists' pass (haze.cpp applyMist) - it runs while either is on
    {"morning_fog", "mist", &EffectSettings::morningFog},
    {"swamp", "mist", &EffectSettings::swamp},
};

// 2.5.16: what is drawn inside the world's own draws (the draw shader's extras: without them most draws take its
// lean form) - no pass of its own to time; keep_fps turns them off, the usually costliest first, once no pass is
// worth a millisecond (R-DUMAMX: an HD 6570's world 40-50 ms a frame, the passes a few)
const Costly kWorldExtras[] = {
    {"materials", "", &EffectSettings::materials},       {"relief", "", &EffectSettings::relief},
    {"sprite_light", "", &EffectSettings::spriteLight},  {"smooth_hills", "", &EffectSettings::smoothHills},
    {"ground_seams", "", &EffectSettings::groundSeams},  {"leaf_light", "", &EffectSettings::leafLight},
    {"soft_particles", "", &EffectSettings::softParticles},
};

const Costly* find(const std::string& key) {
    const std::string k = key.compare(0, 8, "effects.") == 0 ? key.substr(8) : key;
    for (const Costly& c : kCostly)
        if (k == c.key) return &c;
    for (const Costly& c : kWorldExtras)
        if (k == c.key) return &c;
    return nullptr;
}

}  // namespace

std::string format(const Costs& c) {
    std::string out;
    for (const auto& [name, ms] : c) {
        char line[96];
        std::snprintf(line, sizeof line, "%s %.3f\n", name.c_str(), ms);
        out += line;
    }
    return out;
}

Costs parse(const std::string& text) {
    Costs c;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string name;
        double ms = -1;
        if (l >> name >> ms && ms >= 0 && ms < 1000) c[name] = ms;
    }
    return c;
}

bool slowForCard(double fps, int target, double gpuMs) {
    if (target <= 0 || fps >= 0.9 * double(target)) return false;
    return gpuMs < 0.0 || gpuMs > 0.85 * 1000.0 / double(target);
}

std::string passOf(const std::string& key) {
    const Costly* k = find(key);
    return k ? k->pass : "";
}

double costOf(const Costs& c, const std::string& key) {
    const Costly* k = find(key);
    if (!k || !*k->pass) return -1;
    const auto it = c.find(k->pass);
    return it == c.end() ? -1 : it->second;
}

namespace {

// The switches before 3.0.4-beta.35's grass order: the costliest measured, the extras, the usual order
std::string pickOther(const EffectSettings& e, const Costs& c, const std::set<std::string>& dropped) {
    const Costly* best = nullptr;
    double most = 0.05;
    for (const Costly& k : kCostly) {
        if (!(e.*k.on) || dropped.count(k.key)) continue;
        const auto it = c.find(k.pass);
        if (it != c.end() && it->second > most) {
            most = it->second;
            best = &k;
        }
    }
    const auto extra = [&]() -> std::string {  // 2.5.16: the world's extras, the usually costliest first
        for (const Costly& k : kWorldExtras)
            if ((e.*k.on) && !dropped.count(k.key)) return k.key;
        return "";
    };
    // The world's draws timed (Direct3D 11's "world"): a pass worth a millisecond first, then the extras - they are
    // in the world's time - and the cheap passes last; untimed, the passes first and the extras when none is left.
    if (best && (most >= 1.0 || !c.count("world"))) return best->key;
    if (!c.empty()) {  // measured: what is not, or costs next to nothing, is not worth losing - but the extras
        const std::string x = extra();
        return !x.empty() ? x : best ? best->key : "";
    }
    for (const Costly& k : kCostly)
        if ((e.*k.on) && !dropped.count(k.key)) return k.key;
    return extra();
}

}  // namespace

// 3.0.4-beta.35 (the user: «у людей отключается трава на слабых видеокартах»; R-EQDYNE, a Radeon 780M: the grass, 4.94
// ms the costliest, went first): the grass thinned first when it is worth a millisecond (or nothing is measured), then
// the others by their cost, the grass thinned once they are gone, and turned off last
double othersCost(const EffectSettings& e, const Costs& c, const std::set<std::string>& dropped) {
    double sum = 0.0;
    std::set<std::string> passes;
    for (const Costly& k : kCostly) {
        if (std::string(k.key) == "grass" || !(e.*k.on) || dropped.count(k.key) || !*k.pass) continue;
        const auto it = c.find(k.pass);
        if (it != c.end() && passes.insert(k.pass).second) sum += it->second;
    }
    return sum;
}

// 3.0.5-beta.3 (R-UQKHFX, a Radeon 610M: the grass 20 ms of a 45 frames' frame, two and a half minutes at 15-26
// frames while 15 others went first): the thinned grass off at once when the others cannot make the frame up
std::string pickToDrop(const EffectSettings& e, const Costs& c, const std::set<std::string>& dropped, double overMs) {
    const bool grassOn = e.grass && !dropped.count("grass");
    const bool canThin = grassOn && !e.grassThin && !dropped.count("grass_thin");
    const auto grassCost = c.find("grass");
    if (canThin && (grassCost == c.end() || grassCost->second >= 1.0)) return "grass_thin";
    std::set<std::string> others = dropped;
    others.insert("grass");
    if (grassOn && !canThin && grassCost != c.end() && overMs > 0.0 && othersCost(e, c, others) < overMs)
        return "grass";
    const std::string other = pickOther(e, c, others);
    if (!other.empty()) return other;
    return canThin ? "grass_thin" : grassOn ? "grass" : "";
}

void drop(EffectSettings& e, const std::string& key) {
    if (key == "grass_thin" || key == "effects.grass_thin") {
        e.grassThin = true;
        return;
    }
    if (const Costly* k = find(key)) e.*k->on = false;
}

std::vector<std::string> turnedOn(const EffectSettings& was, const EffectSettings& now) {
    std::vector<std::string> out;
    const auto look = [&](const Costly& k) {
        if (now.*k.on && !(was.*k.on)) out.push_back(k.key);
    };
    for (const Costly& k : kCostly) look(k);
    for (const Costly& k : kWorldExtras) look(k);
    return out;
}

}  // namespace uagfx::effect_costs
