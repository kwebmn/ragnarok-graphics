#include "core/gear_compare.hpp"

#include <algorithm>
#include <iterator>

namespace uagfx::gearcompare {
namespace {

#include "core/gear_stats.inc"

constexpr int kWeaponPerLevel[5] = {0, 2, 3, 5, 7};
constexpr int kArmourPerLevel100 = 70, kCanon = 10, kOverPercent = 50;

int refined(int perLevel, int refine) {
    refine = std::max(0, refine);
    if (refine <= kCanon) return refine * perLevel;
    return kCanon * perLevel + (refine - kCanon) * (perLevel * kOverPercent / 100);
}

std::string fill(std::string s, const std::string& a, const std::string& b = "") {
    const std::string args[2] = {a, b};
    for (int i = 0; i < 2; ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        const size_t at = s.find(key);
        if (at != std::string::npos) s.replace(at, key.size(), args[i]);
    }
    return s;
}

std::string signedOf(int v) {
    return (v > 0 ? "+" : v < 0 ? "\xE2\x88\x92" : "\xC2\xB1") + std::to_string(v < 0 ? -v : v);
}

std::string dec(int hundredths) {  // 7.7 DEF shown whole as the game shows it (it drops the hundredths)
    return std::to_string(hundredths / 100);
}

}  // namespace

const Gear* find(int id) {
    const auto at =
        std::lower_bound(std::begin(kGear), std::end(kGear), id, [](const Gear& g, int v) { return g.id < v; });
    return at != std::end(kGear) && at->id == id ? &*at : nullptr;
}

int atkOf(const Gear& g, int refine) {
    return g.atk + refined(kWeaponPerLevel[std::clamp(g.weaponLevel, 0, 4)], refine);
}

int defOf100(const Gear& g, int refine) { return g.def * 100 + refined(kArmourPerLevel100, refine); }

std::string line(int id, int refine, const std::vector<Worn>& worn, const Texts& t) {
    const Gear* g = find(id);
    if (!g) return "";
    const bool weapon = g->type == 4;
    const auto value = [&](const Gear& x, int r) { return weapon ? atkOf(x, r) * 100 : defOf100(x, r); };
    const std::string what = weapon ? "ATK " : "DEF ";
    const int mine = value(*g, refine);
    for (const Worn& w : worn) {
        const Gear* o = find(w.id);
        if (!o || o->type != g->type || !(o->location & g->location)) continue;
        const int theirs = value(*o, w.refine);
        std::string s = what + dec(theirs) + " \xE2\x86\x92 " + dec(mine) + " (" + signedOf(mine / 100 - theirs / 100) +
                        ")";
        if (o->slots != g->slots)
            s += " \xC2\xB7 " + t.slots + " " + std::to_string(o->slots) + " \xE2\x86\x92 " +
                 std::to_string(g->slots);
        return fill(t.instead, w.name, s);
    }
    return fill(t.none, what + dec(mine));
}

}  // namespace uagfx::gearcompare
