#include "core/mob_battle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace uagfx::mobbattle {
namespace {

#include "core/mob_battle.inc"

std::string fill(std::string s, const std::string& a, const std::string& b = "", const std::string& c = "") {
    const std::string args[3] = {a, b, c};
    for (int i = 0; i < 3; ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        for (size_t at = s.find(key); at != std::string::npos; at = s.find(key, at + args[i].size()))
            s.replace(at, key.size(), args[i]);
    }
    return s;
}

}  // namespace

const Mob* find(int id) {
    const auto at =
        std::lower_bound(std::begin(kMobs), std::end(kMobs), id, [](const Mob& m, int v) { return m.id < v; });
    return at != std::end(kMobs) && at->id == id ? &*at : nullptr;
}

Fight fight(const Mob& m, const Own& own, int hp) {
    Fight f;
    f.hitChance = std::clamp(80 + own.hit - (m.level + m.agi), 5, 100);
    const int its = std::clamp(80 + (m.level + m.dex) - own.flee, 5, 100);
    f.itsHitChance = int(std::lround(its * (100 - std::clamp(own.perfectDodge, 0, 100)) / 100.0));
    const int def = std::clamp(own.def, 0, 100);
    const int vitLeast = own.def2 / 2, vitMost = vitLeast + std::max(0, own.def2 * (own.def2 - 15) / 150 - 1);
    const int most = std::max(m.atk1, m.atk2 - 1);
    f.itsLeast = std::max(1, m.atk1 * (100 - def) / 100 - vitMost);
    f.itsMost = std::max(1, most * (100 - def) / 100 - vitLeast);
    if (m.attackDelay > 0)
        f.itsPerSecond = (f.itsLeast + f.itsMost) / 2.0 * f.itsHitChance / 100.0 * 1000.0 / m.attackDelay;
    const int mobDef = std::clamp(m.def, 0, 100), mobVit = m.vit + (m.vit / 20) * (m.vit / 20) / 2;
    f.mine = std::max(1, (own.atk + own.atk2) * (100 - mobDef) / 100 - mobVit);
    if (hp > 0) f.hitsToKill = (hp + f.mine - 1) / f.mine;
    return f;
}

std::vector<std::string> lines(const Fight& f, const Texts& t) {
    char perSecond[24];
    std::snprintf(perSecond, sizeof perSecond, "%.0f", f.itsPerSecond);
    return {fill(t.hit, std::to_string(f.hitChance)), fill(t.its, std::to_string(f.itsHitChance)),
            fill(t.blow, std::to_string(f.itsLeast), std::to_string(f.itsMost), perSecond),
            fill(t.mine, std::to_string(f.mine), f.hitsToKill ? std::to_string(f.hitsToKill) : "?")};
}

}  // namespace uagfx::mobbattle
