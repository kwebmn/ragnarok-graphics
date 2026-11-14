#include "core/stat_window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/char_block.hpp"
#include "core/mob_battle.hpp"
#include "core/stat_math.hpp"
#include "core/ui_style.hpp"

namespace uagfx::statwin {
namespace {

// The concept's sizes at scale 1 (v17_windows): 760 wide; the title 46; the table's header 30, its rows 38; the
// battle's rows 26; the line under them 42
constexpr float kW = 760, kTitleH = 46, kHeaderH = 30, kRowH = 38, kBattleRowH = 26, kSide = 18;
// 3.0.11-beta.266 (stage 2): the panel «Як рахується» under them - its title and kExplainLines lines
constexpr float kExplainTitleH = 30, kExplainLineH = 21, kExplainPad = 10;
constexpr int kBattleRows = 14;
constexpr float kTableW = 410;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kGreen = 0x5FD08A;
// The table's columns: their right edges from the table's left (the stat's name left aligned at 0)
constexpr float kBaseRight = 120, kBonusRight = 190, kTotalRight = 262, kCostRight = 330, kPlusX = 360;

std::string fill(std::string form, const std::vector<std::string>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        for (size_t at = form.find(key); at != std::string::npos; at = form.find(key, at + values[i].size()))
            form.replace(at, key.size(), values[i]);
    }
    return form;
}

std::string decimal(double v, char sep) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    std::string s = buf;
    for (char& c : s)
        if (c == '.') c = sep;
    return s;
}

// 3.0.11-beta.202 (the tester: «різний час касту при наведенні на дексу і у списку»): the cast's per cent to a tenth, a
// whole one without its «,0» - the list's and the hint's alike
std::string tenth(double v, char sep) {
    std::string s = decimal(v, sep);
    if (s.size() > 2 && s.compare(s.size() - 2, 2, std::string(1, sep) + "0") == 0) s.resize(s.size() - 2);
    return s;
}

int total(const Data& d, int i) { return d.base[i] + d.bonus[i]; }

}  // namespace

const char* const kNames[kStats] = {"STR", "AGI", "VIT", "INT", "DEX", "LUK"};
// .298: every class's buffs the plan counts (kPlanBuffSpecs' comment in the header)
constexpr uint32_t kAxesMaces = 1u << 6 | 1u << 7 | 1u << 8 | 1u << 9;
const PlanBuffSpec kPlanBuffSpecs[kPlanBuffs] = {
    {"Improve Concentration", 3, 45, {}, 0, 0, 0, 0, 0, 0, false, 0},
    {"Blessing", 10, 0, {10, 0, 0, 10, 10, 0}, 0, 0, 0, 0, 0, 0, false, 0},
    {"Increase AGI", 12, 0, {0, 12, 0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, false, 0},
    {"Gloria", 21, 0, {0, 0, 0, 0, 0, 30}, 0, 0, 0, 0, 0, 0, false, 0},
    {"Angelus", 9, 0, {}, 0, 0, 0, 0, 50, 0, false, 0},
    {"Impositio Manus", 15, 0, {}, 0, 0, 0, 25, 0, 0, false, 0},
    {"Wind Walk", 116, 0, {}, 0, 5, 0, 0, 0, 0, false, 0},
    {"Loud Exclamation", 30, 155, {4, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, false, 0},
    {"True Sight", 115, 380, {5, 5, 5, 5, 5, 5}, 30, 0, 100, 0, 0, 0, false, 0},
    {"Fury", 86, 270, {}, 0, 0, 200, 0, 0, 0, false, 0},
    {"Two-Hand Quicken", 2, 60, {}, 0, 0, 0, 0, 0, 300, false, 1u << 3},
    {"One-Hand Quicken", 161, 495, {}, 0, 0, 0, 0, 0, 300, false, 1u << 2},
    {"Spear Quicken", 68, 258, {}, 0, 0, 0, 0, 0, 300, false, 1u << 5},
    {"Adrenaline Rush", 23, 111, {}, 0, 0, 0, 0, 0, 300, false, kAxesMaces},  // 200 cast by another
    {"Concentration Potion", 37, 0, {}, 0, 0, 0, 0, 0, 100, true, 0},
    {"Awakening Potion", 38, 0, {}, 0, 0, 0, 0, 0, 150, true, 0},
    {"Berserk Potion", 39, 0, {}, 0, 0, 0, 0, 0, 200, true, 0},
};

bool canRaise(const Data& d, int stat) {
    return stat >= 0 && stat < kStats && d.need[stat] > 0 && d.need[stat] <= d.free && d.base[stat] < 99;
}

std::vector<Row> battleRows(const Data& d, const Texts& t) {
    const auto plus = [](int a, int b) { return std::to_string(a) + " + " + std::to_string(b); };
    const double cast = std::min(100.0, double(total(d, 4)) * 100.0 / 150.0);
    const std::string castWords = tenth(cast, t.decimal);  // 3.0.11-beta.202: to a tenth, as the hint
    // 3.0.11-beta.266: ASPD with its hits a second; the delay after a skill (by DEX on uaRO), HP, SP, their
    // recovery a tick and the weight after the cast
    std::string aspd = std::to_string(d.aspd);
    if (d.aspd > 0 && d.aspd < 200 && !t.perSecond.empty())
        aspd += " \xC2\xB7 " + fill(t.perSecond, {decimal(statmath::hitsPerSecond(2000 - d.aspd * 10), t.decimal)});
    const auto known = [](int v) { return v > 0 ? std::to_string(v) : std::string("\xE2\x80\x94"); };
    const int vit = total(d, 2), intel = total(d, 3);
    const std::string regen = d.hpMost > 0 ? "+" + std::to_string(statmath::hpRegen(vit, d.hpMost)) + " / +" +
                                                 std::to_string(statmath::spRegen(intel, d.spMost))
                                           : std::string("\xE2\x80\x94");
    const std::string weight =
        d.weightMost > 0 ? std::to_string(d.weight) + " / " + std::to_string(d.weightMost) : "\xE2\x80\x94";
    const std::string castCut = cast > 0 ? "\xE2\x88\x92" + castWords + " %" : "0 %";  // −56 %, −44,7 %
    return {Row{"ATK", plus(d.atk, d.atk2)},
            Row{"MATK", std::to_string(d.matkMin) + " ~ " + std::to_string(d.matkMax)},
            Row{"HIT", std::to_string(d.hit)},
            Row{"FLEE", plus(d.flee, d.flee2)},
            Row{"CRIT", std::to_string(d.crit)},
            Row{"DEF", plus(d.def, d.def2)},
            Row{"MDEF", plus(d.mdef, d.mdef2)},
            Row{"ASPD", aspd},
            Row{t.cast, castCut},
            Row{t.delay, castCut},
            Row{"Max HP", known(d.hpMost)},
            Row{"Max SP", known(d.spMost)},
            Row{t.regen, regen},
            Row{t.weight, weight}};
}

int weaponOf(const Data& d) {
    if (d.job < 0 || d.aspd <= 0) return -1;
    for (int w = 0; w < statmath::kWeapons; ++w) {
        // .298: the speed buffs on now counted (a Concentration Potion's ASPD matched no weapon)
        const int a = statmath::amotionRated(d.job, w, total(d, 1), total(d, 4), aspdRate(d, w, false, nullptr));
        if (a > 0 && statmath::aspd10(a) / 10 == d.aspd) return w;
    }
    return -1;
}

namespace {

const char* const kWeaponNames[statmath::kWeapons] = {
    "Fist",   "Dagger", "One-Handed Sword", "Two-Handed Sword", "One-Handed Spear", "Two-Handed Spear",
    "Axe",    "Two-Handed Axe", "Mace", "Two-Handed Mace", "Rod", "Bow", "Knuckle", "Instrument", "Whip", "Book",
    "Katar",  "Revolver", "Rifle", "Shotgun", "Gatling Gun", "Grenade Launcher", "Fuuma Shuriken"};

std::string n(int v) { return std::to_string(v); }

// The server's number against the formula's: the same, or the rest from things, buffs and skills
std::string versus(int server, int ours, const Texts& t) {
    if (server <= 0) return "";
    if (server == ours) return fill(t.howSame, {n(server)});
    return fill(t.howMore, {n(server), n(ours), (server > ours ? "+" : "") + n(server - ours)});
}

std::string pct10(int tenths, char sep) { return decimal(double(tenths) / 10.0, sep); }

}  // namespace

const char* weaponName(int weapon) {
    return weapon >= 0 && weapon < statmath::kWeapons ? kWeaponNames[weapon] : "";
}

std::vector<AspdRow> aspdTable(int job, int agi, int dex) {
    std::vector<AspdRow> out;
    const statmath::Class c = statmath::classOf(job);
    if (!c.known) return out;
    for (int w = 0; w < statmath::kWeapons && int(out.size()) < kMostAspdRows; ++w) {
        if (c.delays[w] >= 2000) continue;
        AspdRow r;
        r.weapon = w;
        r.delay = c.delays[w];
        for (int i = 0; i < 4; ++i) r.at[i] = statmath::aspd10(statmath::amotion(job, w, kAgiSteps[i], dex)) / 10;
        r.mine = statmath::aspd10(statmath::amotion(job, w, agi, dex)) / 10;
        out.push_back(r);
    }
    return out;
}

Plan planFrom(const Data& d) {
    Plan p;
    std::copy(d.base, d.base + kStats, p.base);
    p.level = d.baseLevel;
    return p;
}

namespace {
int concentration(const Data& d, int stat, int base, int level) {  // AGI and DEX alone
    if ((stat != 1 && stat != 4) || level <= 0) return 0;
    const int job = d.job >= 0 ? statmath::jobBonus(d.job, d.jobLevel, stat) : 0;
    return (base + job) * (2 + level) / 100;
}

bool counted(const Data& d, const Plan* p, int buff) {  // on now (p null) or in the plan
    return p ? planBuff(d, *p, buff) : d.buffNow[buff];
}

// .298: the flat parts of the buffs counted - HIT, FLEE, CRIT tenths, ATK, VIT DEF per cent
struct Flat {
    int hit = 0, flee = 0, crit10 = 0, atk = 0, defPercent = 0;
};
Flat flatOf(const Data& d, const Plan* p) {
    Flat f;
    for (int i = 0; i < kPlanBuffs; ++i) {
        if (!counted(d, p, i)) continue;
        const PlanBuffSpec& b = kPlanBuffSpecs[i];
        f.hit += b.hit;
        f.flee += b.flee;
        f.crit10 += b.crit10;
        f.atk += b.atk;
        f.defPercent += b.defPercent;
    }
    return f;
}
}  // namespace

bool planBuffOffered(const Data& d, int buff) {
    if (buff < 0 || buff >= kPlanBuffs) return false;
    if (kPlanBuffSpecs[buff].skill == 0 || d.learned[buff] > 0 || d.buffNow[buff]) return true;
    const int w = buff == kBuffAdrenaline ? weaponOf(d) : -1;  // a blacksmith's party: axes and maces
    return w >= 0 && (kPlanBuffSpecs[buff].weapons >> w & 1);
}

bool planBuff(const Data& d, const Plan& p, int buff) {
    if (buff < 0 || buff >= kPlanBuffs || !planBuffOffered(d, buff)) return false;
    return p.buffs[buff] < 0 ? d.buffNow[buff] : p.buffs[buff] != 0;
}

int aspdRate(const Data& d, int weapon, bool planned, const Plan* p) {
    int skill = 0, potion = 0;
    for (int i = 0; i < kPlanBuffs; ++i) {
        const PlanBuffSpec& b = kPlanBuffSpecs[i];
        if (!b.aspd || !counted(d, planned ? p : nullptr, i)) continue;
        if (b.weapons && (weapon < 0 || !(b.weapons >> weapon & 1))) continue;
        int v = b.aspd;
        if (i == kBuffAdrenaline && d.learned[i] <= 0) v = 200;  // cast by a party's blacksmith
        (b.potion ? potion : skill) = std::max(b.potion ? potion : skill, v);
    }
    return 1000 - skill - potion;
}

void planTotals(const Data& d, const Plan& p, int out[kStats]) {
    const int ic = d.learned[kBuffConcentration] > 0 ? d.learned[kBuffConcentration] : 10;
    for (int i = 0; i < kStats; ++i) {
        int other = d.bonus[i];  // the things', the passive skills' and the job's: the buffs on now taken out
        for (int k = 0; k < kPlanBuffs; ++k) {
            if (d.buffNow[k]) other -= kPlanBuffSpecs[k].stats[i];
            if (planBuff(d, p, k)) other += kPlanBuffSpecs[k].stats[i];
        }
        if (d.buffNow[kBuffConcentration]) other -= concentration(d, i, d.base[i], ic);
        out[i] = p.base[i] + other + (planBuff(d, p, kBuffConcentration) ? concentration(d, i, p.base[i], ic) : 0);
    }
}

int planAvailable(const Data& d, const Plan& p) {
    const int now = statmath::pointsAt(d.baseLevel), then = statmath::pointsAt(p.level);
    return d.free + (now > 0 && then > now ? then - now : 0);
}

int planUsed(const Data& d, const Plan& p) {
    int used = 0;
    for (int i = 0; i < kStats; ++i) used += statmath::pointsBetween(d.base[i], p.base[i]);
    return used;
}

std::vector<PlanRow> planRows(const Data& d, const Plan& p, const Texts& t) {
    namespace sm = uagfx::statmath;
    int a[kStats], b[kStats];
    for (int i = 0; i < kStats; ++i) a[i] = total(d, i);
    planTotals(d, p, b);  // .297: the plan's buffs counted
    const int lv = d.baseLevel, plv = std::max(p.level, lv), w = weaponOf(d);
    std::vector<PlanRow> out;
    const auto row = [&](const std::string& name, int now, int then) {
        out.push_back(PlanRow{name, std::to_string(now), std::to_string(then), now != then});
    };
    const Flat fn = flatOf(d, nullptr), fp = flatOf(d, &p);  // .298: the buffs' own HIT, FLEE, CRIT, ATK
    row("ATK", d.atk, d.atk + sm::atk(b, w) - sm::atk(a, w) + fp.atk - fn.atk);
    row("MATK", d.matkMax, d.matkMax + sm::matkMax(b[3]) - sm::matkMax(a[3]));
    row("HIT", d.hit, d.hit + sm::hit(plv, b[4]) - sm::hit(lv, a[4]) + fp.hit - fn.hit);
    row("FLEE", d.flee, d.flee + sm::flee(plv, b[1]) - sm::flee(lv, a[1]) + fp.flee - fn.flee);
    if (w >= 0) {  // ASPD to a tenth by the formula, the server's whole one now; .298 the speed buffs counted
        const int then = sm::aspd10(sm::amotionRated(d.job, w, b[1], b[4], aspdRate(d, w, true, &p)));
        const int now = sm::aspd10(sm::amotionRated(d.job, w, a[1], a[4], aspdRate(d, w, false, nullptr)));
        out.push_back(PlanRow{"ASPD", std::to_string(d.aspd), decimal(then / 10.0, t.decimal), then != now});
    }
    const int critNow = sm::crit10(a[5]) + fn.crit10, critThen = sm::crit10(b[5]) + fp.crit10;
    out.push_back(PlanRow{"CRIT", decimal(critNow / 10.0, t.decimal) + " %", decimal(critThen / 10.0, t.decimal) + " %",
                          critNow != critThen});
    if (d.job >= 0 && d.hpMost > 0) {
        row("Max HP", d.hpMost, d.hpMost + sm::maxHp(d.job, plv, b[2]) - sm::maxHp(d.job, lv, a[2]));
        row("Max SP", d.spMost, d.spMost + sm::maxSp(d.job, plv, b[3]) - sm::maxSp(d.job, lv, a[3]));
    }
    out.push_back(PlanRow{t.cast, std::to_string(sm::castPercent(a[4])) + " %",
                          std::to_string(sm::castPercent(b[4])) + " %", a[4] != b[4]});
    if (d.job >= 0 && d.weightMost > 0)
        row(t.weight, d.weightMost, d.weightMost + sm::weightMost(d.job, p.base[0]) - sm::weightMost(d.job, d.base[0]));
    return out;
}

std::vector<MobRow> planMobs(const Data& d, const Plan& p) {
    namespace sm = uagfx::statmath;
    int a[kStats], b[kStats];
    for (int i = 0; i < kStats; ++i) a[i] = total(d, i);
    planTotals(d, p, b);  // .297: the plan's buffs counted
    const int lv = d.baseLevel, plv = std::max(p.level, lv), w = weaponOf(d);
    mobbattle::Own now{d.hit, d.flee, d.flee2, d.def, d.def2, d.atk, d.atk2};
    if (w >= 0) {  // 3.0.16-beta.32: the weapon's part of ATK sized for each monster
        now.batk = sm::atk(a, w);
        now.weapon = w;
    }
    mobbattle::Own then = now;
    then.hit += sm::hit(plv, b[4]) - sm::hit(lv, a[4]);
    then.flee += sm::flee(plv, b[1]) - sm::flee(lv, a[1]);
    then.perfectDodge += sm::perfectDodge10(b[5]) / 10 - sm::perfectDodge10(a[5]) / 10;
    const Flat fn = flatOf(d, nullptr), fp = flatOf(d, &p);  // .298: the buffs' own parts
    then.hit += fp.hit - fn.hit;
    then.flee += fp.flee - fn.flee;
    then.def2 = (d.def2 * 100 / (100 + fn.defPercent) + b[2] - a[2]) * (100 + fp.defPercent) / 100;  // Angelus
    then.atk += sm::atk(b, w) - sm::atk(a, w) + fp.atk - fn.atk;
    if (w >= 0) then.batk = sm::atk(b, w);  // as now: the stats' part alone
    std::vector<MobRow> out;
    for (const MapMob& mm : d.mapMobs) {
        const mobbattle::Mob* m = mobbattle::find(mm.id);
        if (!m) continue;
        const mobbattle::Fight f0 = mobbattle::fight(*m, now, mm.hp), f1 = mobbattle::fight(*m, then, mm.hp);
        out.push_back(MobRow{mm.name, mm.level, f0.hitChance, f1.hitChance, 100 - f0.itsHitChance,
                             100 - f1.itsHitChance, f0.hitsToKill, f1.hitsToKill});
        if (int(out.size()) >= kMobRows) break;
    }
    return out;
}

std::string plansText(const std::map<std::string, SavedPlan>& plans) {
    std::string out;
    static const char* const kHex = "0123456789abcdef";
    for (const auto& [name, sp] : plans) {
        if (name.empty()) continue;
        for (const unsigned char c : name) {
            out += kHex[c >> 4];
            out += kHex[c & 15];
        }
        out += ' ' + std::to_string(sp.plan.level);
        for (int i = 0; i < kStats; ++i) out += ' ' + std::to_string(sp.plan.base[i]);
        out += ' ' + std::to_string(sp.aspdGoal) + " b";  // .298: each buff -1 / 0 / 1 as a digit 0 / 1 / 2
        for (int i = 0; i < kPlanBuffs; ++i) out += char('0' + std::clamp(sp.plan.buffs[i], -1, 1) + 1);
        out += '\n';
    }
    return out;
}

std::map<std::string, SavedPlan> parsePlans(const std::string& text) {
    std::map<std::string, SavedPlan> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t space = line.find(' ');
        if (space == std::string::npos || space == 0 || space % 2) continue;
        std::string name;
        bool ok = true;
        for (size_t i = 0; i + 1 < space && ok; i += 2) {
            const auto digit = [&](char c) {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                ok = false;
                return 0;
            };
            name += char(digit(line[i]) * 16 + digit(line[i + 1]));
        }
        SavedPlan sp;
        int v[kStats + 3] = {};
        char buffs[64] = {};
        const int got = std::sscanf(line.c_str() + space, "%d %d %d %d %d %d %d %d %63s", &v[0], &v[1], &v[2], &v[3],
                                    &v[4], &v[5], &v[6], &v[7], buffs);
        if (!ok || got < kStats + 2 || v[0] < 1 || v[0] > 99) continue;
        if (got == kStats + 3 && buffs[0] == 'b') {  // .298: a digit a buff (.296's lines have none: as on now)
            for (int i = 0; i < kPlanBuffs && buffs[1 + i] >= '0' && buffs[1 + i] <= '2'; ++i)
                sp.plan.buffs[i] = buffs[1 + i] - '0' - 1;
        } else if (got == kStats + 3) {  // .297's: three as a base-3 number
            for (int i = 0, code = std::clamp(std::atoi(buffs), 0, 26); i < 3; ++i, code /= 3)
                sp.plan.buffs[i] = code % 3 - 1;
        }
        sp.plan.level = v[0];
        for (int i = 0; i < kStats; ++i) sp.plan.base[i] = std::clamp(v[1 + i], 1, 99);
        sp.aspdGoal = std::clamp(v[kStats + 1], 0, 190);
        out[name] = sp;
    }
    return out;
}

std::vector<std::string> planGoals(const Data& d, const Plan& p, int aspdGoal, const Texts& t) {
    namespace sm = uagfx::statmath;
    std::vector<std::string> out;
    const int most = sm::baby(d.job) ? sm::kMaxBabyStat : sm::kMaxStat;
    // .297: a stat's total with its base at `base`, the rest of the plan as it is (its buffs counted)
    const auto totalAt = [&](int stat, int base) {
        Plan q = p;
        q.base[stat] = base;
        std::array<int, kStats> v{};
        planTotals(d, q, v.data());
        return v;
    };
    const int w = weaponOf(d), dex = totalAt(4, p.base[4])[4];
    if (w < 0) {
        out.push_back(t.goalAspdNone);
    } else {
        int base = p.base[1], agi = 0;
        for (; base < 300; ++base) {
            const std::array<int, kStats> v = totalAt(1, base);
            agi = v[1];
            if (sm::aspd10(sm::amotionRated(d.job, w, agi, v[4], aspdRate(d, w, true, &p))) >= aspdGoal * 10) break;
        }
        if (base > most)
            out.push_back(fill(t.goalAspdNever, {std::to_string(aspdGoal), std::to_string(agi)}));
        else
            out.push_back(fill(t.goalAspd, {std::to_string(aspdGoal), std::to_string(agi),
                                            std::to_string(sm::pointsBetween(p.base[1], std::max(base, p.base[1])))}));
    }
    int need = p.base[4];  // the base DEX for no cast
    while (need < 300 && totalAt(4, need)[4] < sm::kCastDex) ++need;
    if (dex >= sm::kCastDex)
        out.push_back(t.goalCastDone);
    else if (need > most)
        out.push_back(fill(t.goalCastNever, {std::to_string(sm::kCastDex), std::to_string(need - most)}));
    else
        out.push_back(
            fill(t.goalCast, {std::to_string(sm::kCastDex), std::to_string(sm::pointsBetween(p.base[4], need))}));
    return out;
}

int explainedItem(const Show& sh) {
    if (sh.hoverRow >= 0) return sh.hoverRow;
    if (sh.hoverPlus >= 0) return sh.hoverPlus;
    if (sh.hoverBattle >= 0) return kBattle + sh.hoverBattle;
    if (sh.pinnedBattle >= 0) return kBattle + sh.pinnedBattle;
    return sh.selected;
}

Explained explained(const Data& d, int item, const Texts& t) {
    namespace sm = uagfx::statmath;
    Explained e;
    if (item < 0 || (item >= kStats && item < kBattle) || item >= kBattle + kBattleRows) {
        e.title = t.howNone;
        return e;
    }
    int s[kStats];
    for (int i = 0; i < kStats; ++i) s[i] = total(d, i);
    const int lv = d.baseLevel;
    const std::string level = fill(t.howLevel, {n(lv)});
    auto& L = e.lines;
    const auto add = [&](const std::string& line) {
        if (!line.empty() && int(L.size()) < kExplainLines) L.push_back(line);
    };
    if (item < kStats) {  // a stat: its price, the job's part, what it goes into
        e.title = fill(t.how, {kNames[item]});
        add(explain(d, item, t));
        if (const int job = d.job >= 0 ? sm::jobBonus(d.job, d.jobLevel, item) : 0; job > 0)
            add(fill(t.howJob, {n(job)}));
        add(t.uses[item]);
        return e;
    }
    const int row = item - kBattle;
    const std::vector<Row> rows = battleRows(d, t);
    e.title = fill(t.how, {rows[size_t(row)].name});
    const int weapon = weaponOf(d);
    switch (row) {
        case 0: {  // ATK: the stats' part, the weapon's, the next point's
            const bool far = sm::ranged(weapon);
            const int a = far ? s[4] : s[0], b = far ? s[0] : s[4], ours = sm::atk(s, weapon);
            add(std::string(far ? "DEX " : "STR ") + n(a) + " + (" + n(a) + "/10)\xC2\xB2 " + n((a / 10) * (a / 10)) +
                " + " + (far ? "STR " : "DEX ") + n(b) + "/5 " + n(b / 5) + " + LUK " + n(s[5]) + "/5 " + n(s[5] / 5) +
                " = " + n(ours));
            add(fill(t.howWeapon, {n(std::max(0, d.atk - ours)), n(d.atk2)}));
            int next[kStats];
            std::copy(s, s + kStats, next);
            ++next[far ? 4 : 0];
            add(fill(t.howNext, {far ? "DEX" : "STR", "ATK +" + n(sm::atk(next, weapon) - ours)}));
            break;
        }
        case 1:  // MATK
            add("min INT " + n(s[3]) + " + (" + n(s[3]) + "/7)\xC2\xB2 = " + n(sm::matkMin(s[3])) +
                " \xC2\xB7 max INT " + n(s[3]) + " + (" + n(s[3]) + "/5)\xC2\xB2 = " + n(sm::matkMax(s[3])));
            add(versus(d.matkMax, sm::matkMax(s[3]), t));
            add(fill(t.howNext, {"INT", "MATK +" + n(sm::matkMin(s[3] + 1) - sm::matkMin(s[3])) + " ~ +" +
                                            n(sm::matkMax(s[3] + 1) - sm::matkMax(s[3]))}));
            break;
        case 2:  // HIT
            add("HIT = " + level + " + DEX " + n(s[4]) + " = " + n(sm::hit(lv, s[4])));
            add(versus(d.hit, sm::hit(lv, s[4]), t));
            add(fill(t.howNext, {"DEX", "HIT +1"}));
            break;
        case 3:  // FLEE and the perfect dodge
            add("FLEE = " + level + " + AGI " + n(s[1]) + " = " + n(sm::flee(lv, s[1])));
            add(versus(d.flee, sm::flee(lv, s[1]), t));
            add(fill(t.howPerfect, {n(d.flee2), pct10(sm::perfectDodge10(s[5]), t.decimal)}));
            break;
        case 4:  // CRIT
            add("CRIT = 1 + LUK " + n(s[5]) + " \xC3\x97 0" + std::string(1, t.decimal) + "3 = " +
                pct10(sm::crit10(s[5]), t.decimal) + " %");
            add(fill(t.howNext, {"LUK", "CRIT +0" + std::string(1, t.decimal) + "3 %"}));
            break;
        case 5:  // DEF: the armour's per cent, VIT's taken off
            add(fill(t.howDef, {n(d.def), n(sm::vitDefLeast(s[2])), n(sm::vitDefMost(s[2]))}));
            add("VIT " + n(s[2]) + ": VIT/2 .. VIT/2 + VIT \xC3\x97 (VIT \xE2\x88\x92 15)/150 \xE2\x88\x92 1");
            break;
        case 6:  // MDEF
            add(fill(t.howMdef, {n(d.mdef), n(sm::softMdef(s[3], s[2]))}));
            add(versus(d.mdef2, sm::softMdef(s[3], s[2]), t));
            break;
        case 7: {  // ASPD: the class's delay for the weapon, AGI and DEX, the next whole ASPD
            if (weapon < 0) {
                add(t.howAspdUnknown);
                break;
            }
            const sm::Class c = sm::classOf(d.job);
            const int base = c.delays[weapon], cut = base * (4 * s[1] + s[4]) / 1000;
            const int mo = sm::amotion(d.job, weapon, s[1], s[4]);
            const char* cls = charblock::jobName(d.job);
            add(fill(t.howAspdWeapon, {*cls ? cls : "#" + n(d.job), kWeaponNames[weapon], n(base)}));
            add("\xE2\x88\x92 " + n(base) + " \xC3\x97 (4 \xC3\x97 AGI " + n(s[1]) + " + DEX " + n(s[4]) +
                ") / 1000 = " + n(cut) + " \xE2\x86\x92 " + n(mo) + " \xC2\xB7 ASPD = 200 \xE2\x88\x92 " + n(mo) +
                "/10 = " + pct10(sm::aspd10(mo), t.decimal));
            add(fill(t.howHits, {decimal(sm::hitsPerSecond(mo), t.decimal)}));
            const int want = d.aspd + 1;
            int agi = s[1];
            while (agi < 300 && sm::aspd10(sm::amotion(d.job, weapon, agi, s[4])) / 10 < want) ++agi;
            const int baseAgi = d.base[1] + (agi - s[1]);
            add(fill(baseAgi > sm::kMaxStat ? t.howAspdNever : t.howAspdGoal, {n(want), n(agi)}));
            break;
        }
        case 8:  // the cast
            add("\xC3\x97 (150 \xE2\x88\x92 DEX " + n(s[4]) + ") / 150 = " + n(sm::castPercent(s[4])) + " %");
            add(s[4] >= sm::kCastDex ? t.howCastNone : fill(t.howCast, {n(sm::kCastDex - s[4])}));
            break;
        case 9:  // the delay after a skill
            add("\xC3\x97 (150 \xE2\x88\x92 DEX " + n(s[4]) + ") / 150 = " + n(sm::delayPercent(s[4])) + " %");
            add(t.howDelay);
            break;
        case 10: {  // Max HP
            if (d.job < 0) break;
            const int base = sm::maxHp(d.job, lv, 0) - 35, ours = sm::maxHp(d.job, lv, s[2]);
            add(level + ": " + n(base) + (sm::upper(d.job) ? " (\xC3\x97 1" + std::string(1, t.decimal) + "25)" : "") +
                " \xC3\x97 (1 + VIT " + n(s[2]) + "/100) + 35 = " + n(ours));
            add(versus(d.hpMost, ours, t));
            add(fill(t.howNext, {"VIT", "Max HP +" + n(sm::maxHp(d.job, lv, s[2] + 1) - ours)}));
            break;
        }
        case 11: {  // Max SP
            if (d.job < 0) break;
            const int base = sm::maxSp(d.job, lv, 0), ours = sm::maxSp(d.job, lv, s[3]);
            add(level + ": " + n(base) + " \xC3\x97 (1 + INT " + n(s[3]) + "/100) = " + n(ours));
            add(versus(d.spMost, ours, t));
            add(fill(t.howNext, {"INT", "Max SP +" + n(sm::maxSp(d.job, lv, s[3] + 1) - ours)}));
            break;
        }
        case 12:  // the recovery a tick
            add("HP 1 + VIT " + n(s[2]) + "/5 + " + n(d.hpMost) + "/200 = " + n(sm::hpRegen(s[2], d.hpMost)) +
                " \xC2\xB7 SP 1 + INT " + n(s[3]) + "/6 + " + n(d.spMost) + "/100 = " + n(sm::spRegen(s[3], d.spMost)));
            add(t.howRegen);
            break;
        case 13: {  // the weight
            if (d.job < 0) break;
            const int cls = sm::classOf(d.job).weight / 10, ours = sm::weightMost(d.job, d.base[0]);
            add(n(cls) + " + 30 \xC3\x97 STR " + n(d.base[0]) + " = " + n(ours));
            add(versus(d.weightMost, ours, t));
            const int most = d.weightMost > 0 ? d.weightMost : ours;
            add(fill(t.howWeight, {n(most * 70 / 100), n(most * 90 / 100)}));
            break;
        }
        default: break;
    }
    return e;
}

std::string explain(const Data& d, int i, const Texts& t) {
    if (i < 0 || i >= kStats) return t.hint;
    const int base = d.base[i];
    if (base >= 99) return fill(t.top, {kNames[i], std::to_string(base)});
    std::string line = fill(t.cost, {kNames[i], std::to_string(base), std::to_string(base + 1),
                                     std::to_string(d.need[i]), std::to_string(d.free)});
    if (d.need[i] > d.free) line += " \xE2\x80\x94 " + fill(t.shortBy, {std::to_string(d.need[i] - d.free)});
    const int v = total(d, i);
    std::string effect;
    switch (i) {
        case 0: {  // a melee hit's base: STR + (STR / 10)²
            const int grow = 1 + ((v + 1) / 10) * ((v + 1) / 10) - (v / 10) * (v / 10);
            effect = fill(t.effects[0], {std::to_string(grow)});
            break;
        }
        case 1: effect = fill(t.effects[1], {std::to_string(d.flee + 1)}); break;
        case 2: effect = t.effects[2]; break;
        case 3: {  // MATK: INT + (INT / 7)² ~ INT + (INT / 5)²
            const int low = 1 + ((v + 1) / 7) * ((v + 1) / 7) - (v / 7) * (v / 7);
            const int high = 1 + ((v + 1) / 5) * ((v + 1) / 5) - (v / 5) * (v / 5);
            effect = fill(t.effects[3], {std::to_string(low), std::to_string(high)});
            break;
        }
        case 4: {  // HIT +1; the cast a 150th shorter a point - 3.0.11-beta.202: from now to the next point's
            const double now = std::min(100.0, double(v) * 100.0 / 150.0);
            const double next = std::min(100.0, double(v + 1) * 100.0 / 150.0);
            effect = fill(t.effects[4], {std::to_string(d.hit + 1), tenth(now, t.decimal), tenth(next, t.decimal)});
            break;
        }
        default: effect = t.effects[5]; break;
    }
    return effect.empty() ? line : line + " \xC2\xB7 " + effect;
}

Layout layout(int x, int y, float scale, const Data& d, const Texts& t, const Measure& m, int tab, int refJob) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.tab = tab >= 0 && tab < kTabs ? tab : 0;
    L.refJob = refJob >= 0 ? refJob : d.job;
    const float w = kW * s, side = kSide * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    const std::string chip = fill(t.free, {std::to_string(d.free)});
    const float chipW = m(chip, 11.5f * s, true) + 18 * s;
    L.freeChip = panel::Box{L.close.x - 14 * s - chipW, (kTitleH * s - 22 * s) / 2, chipW, 22 * s};
    // 3.0.11-beta.267 (stage 3): the tabs after the title and its key
    float tx = side + m(t.title, 15 * s, true) + 2 + 8 * s + m(t.hotkey, 11 * s, false) + 2 + 22 * s;
    for (int i = 0; i < kTabs; ++i) {
        const float tw = m(t.tabs[i], 12 * s, true) + 24 * s;
        L.tabs[i] = panel::Box{tx, (kTitleH * s - 26 * s) / 2, tw, 26 * s};
        tx += tw + 6 * s;
    }
    L.table = panel::Box{side, kTitleH * s + 4 * s, kTableW * s, kHeaderH * s + float(kStats) * kRowH * s};
    L.header = panel::Box{L.table.x, L.table.y, L.table.w, kHeaderH * s};
    for (int i = 0; i < kStats; ++i) {
        const panel::Box r{L.table.x, L.table.y + kHeaderH * s + float(i) * kRowH * s, L.table.w, kRowH * s};
        L.rows.push_back(r);
        L.pluses.push_back(panel::Box{r.x + kPlusX * s, r.y + (r.h - 24 * s) / 2, 24 * s, 24 * s});
    }
    const float bx = L.table.x + L.table.w + 26 * s;
    L.battle = panel::Box{bx, L.table.y, w - side - bx, kHeaderH * s + float(kBattleRows) * kBattleRowH * s};
    for (int i = 0; i < kBattleRows; ++i)  // 3.0.11-beta.266: each row pointed at or clicked
        L.battleRows.push_back(panel::Box{bx - 6 * s, L.battle.y + kHeaderH * s + float(i) * kBattleRowH * s,
                                          L.battle.w + 12 * s, kBattleRowH * s});
    const float bottom = std::max(L.table.y + L.table.h, L.battle.y + L.battle.h) + 8 * s;
    L.line = panel::Box{0, bottom, w, (kExplainTitleH + float(kExplainLines) * kExplainLineH + kExplainPad) * s};
    // «Довідка»: the class's row with ‹ ›, the ASPD's table (kMostAspdRows at most), the stat's price, the job's
    const float rowH = 22 * s, headH = 26 * s, gap = 12 * s;
    L.classRow = panel::Box{side, kTitleH * s + 6 * s, w - 2 * side, 30 * s};
    L.prev = panel::Box{L.classRow.x + 150 * s, L.classRow.y + 3 * s, 24 * s, 24 * s};
    L.next = panel::Box{L.classRow.x + 360 * s, L.classRow.y + 3 * s, 24 * s, 24 * s};
    L.aspd = panel::Box{side, L.classRow.y + L.classRow.h + gap, w - 2 * side, headH * 2 + kMostAspdRows * rowH};
    L.cost = panel::Box{side, L.aspd.y + L.aspd.h + gap, w - 2 * side, headH * 2 + rowH};
    L.bonus = panel::Box{side, L.cost.y + L.cost.h + gap, w - 2 * side, headH + float(kStats) * 20 * s};
    const float refBottom = L.bonus.y + L.bonus.h + 10 * s;
    // «Планування» (.268): the level's row, six stats' rows at the left with − and +, the results at the right
    L.planLevelRow = panel::Box{side, kTitleH * s + 6 * s, kTableW * s, 30 * s};
    L.levelPrev = panel::Box{side + 150 * s, L.planLevelRow.y + 3 * s, 24 * s, 24 * s};
    L.levelNext = panel::Box{side + 250 * s, L.planLevelRow.y + 3 * s, 24 * s, 24 * s};
    const float py = L.planLevelRow.y + L.planLevelRow.h + 34 * s;
    for (int i = 0; i < kStats; ++i) {
        const panel::Box r{side, py + float(i) * kRowH * s, kTableW * s, kRowH * s};
        L.planRowsBoxes.push_back(r);
        L.planMinus.push_back(panel::Box{r.x + 90 * s, r.y + (r.h - 24 * s) / 2, 24 * s, 24 * s});
        L.planPlus.push_back(panel::Box{r.x + 250 * s, r.y + (r.h - 24 * s) / 2, 24 * s, 24 * s});
    }
    L.planResults = panel::Box{bx, L.planLevelRow.y, w - side - bx, kHeaderH * s + 11 * kBattleRowH * s};
    L.planReset = panel::Box{side, py + float(kStats) * kRowH * s + 16 * s, 120 * s, 32 * s};
    // .297: the buffs the plan counts, a chip each (Improve Concentration only when learned or on)
    // .298: every class's - as many rows as they take
    L.buffsRow = panel::Box{side, L.planReset.y + L.planReset.h + 12 * s, w - 2 * side, 28 * s};
    const float chipLeft = side + m(t.planBuffs, 12 * s, false) + 14 * s;
    float chipX = chipLeft, chipY = L.buffsRow.y + 2 * s;
    for (int i = 0; i < kPlanBuffs; ++i) {
        if (!planBuffOffered(d, i)) continue;
        const std::string label = std::string("\xE2\x9C\x93 ") + kPlanBuffSpecs[i].name +
                                  (d.buffNow[i] ? " \xC2\xB7 " + t.planBuffNow : std::string());
        const float cw = m(label, 11 * s, true) + 22 * s;
        if (chipX > chipLeft && chipX + cw > w - side) {
            chipX = chipLeft;
            chipY += 28 * s;
        }
        L.buffChips[i] = panel::Box{chipX, chipY, cw, 24 * s};
        chipX += cw + 6 * s;
    }
    L.buffsRow.h = chipY + 26 * s - L.buffsRow.y;
    L.goals = panel::Box{side, L.buffsRow.y + L.buffsRow.h + 16 * s, w - 2 * side, 26 * s + 3 * 22 * s};  // .269
    L.goalPrev = panel::Box{side + 200 * s, L.goals.y + 1 * s, 24 * s, 24 * s};
    L.goalNext = panel::Box{side + 280 * s, L.goals.y + 1 * s, 24 * s, 24 * s};
    // .296: the map's monsters under the goals
    L.mobs = panel::Box{side, L.goals.y + L.goals.h + 12 * s, w - 2 * side, 26 * s + float(kMobRows + 1) * 22 * s};
    L.glass = panel::Box{0, 0, w, std::max({L.line.y + L.line.h, refBottom, L.mobs.y + L.mobs.h + 10 * s})};
    if (L.tab != kPlanTab) {  // .268: the plan's parts only in «Планування»
        L.planRowsBoxes.clear();
        L.planMinus.clear();
        L.planPlus.clear();
        L.levelPrev = L.levelNext = L.planReset = L.goalPrev = L.goalNext = panel::Box{};
        for (panel::Box& b : L.buffChips) b = panel::Box{};  // .297
    }
    if (L.tab != 0) {  // «Планування», «Довідка»: none of «Персонаж»'s rows to point at
        L.rows.clear();
        L.pluses.clear();
        L.battleRows.clear();
    }
    L.w = int(std::ceil(w));
    L.wide = L.w;
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < kTabs; ++i)  // 3.0.11-beta.267
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, i};
    for (size_t i = 0; i < L.planMinus.size(); ++i) {  // .268
        if (L.planMinus[i].contains(x, y)) return Hit{Part::PlanMinus, int(i)};
        if (L.planPlus[i].contains(x, y)) return Hit{Part::PlanPlus, int(i)};
    }
    if (L.tab == kPlanTab && L.levelPrev.contains(x, y)) return Hit{Part::PlanLevel, 0};
    if (L.tab == kPlanTab && L.levelNext.contains(x, y)) return Hit{Part::PlanLevel, 1};
    if (L.tab == kPlanTab && L.planReset.contains(x, y)) return Hit{Part::PlanReset, -1};
    if (L.tab == kPlanTab && L.goalPrev.contains(x, y)) return Hit{Part::Goal, 0};  // .269
    if (L.tab == kPlanTab && L.goalNext.contains(x, y)) return Hit{Part::Goal, 1};
    for (int i = 0; i < kPlanBuffs; ++i)  // .297
        if (L.tab == kPlanTab && L.buffChips[i].w > 0 && L.buffChips[i].contains(x, y)) return Hit{Part::PlanBuff, i};
    if (L.tab == kRefTab && L.prev.contains(x, y)) return Hit{Part::Arrow, 0};
    if (L.tab == kRefTab && L.next.contains(x, y)) return Hit{Part::Arrow, 1};
    for (size_t i = 0; i < L.pluses.size(); ++i)
        if (L.pluses[i].contains(x, y)) return Hit{Part::Plus, int(i)};
    for (size_t i = 0; i < L.rows.size(); ++i)
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, int(i)};
    for (size_t i = 0; i < L.battleRows.size(); ++i)
        if (L.battleRows[i].contains(x, y)) return Hit{Part::Battle, int(i)};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

namespace {

void planWords(std::vector<Words>& out, const Layout& L, const Data& d, const Show& sh, const Texts& t) {
    namespace sm = uagfx::statmath;
    const float s = L.s;
    const Plan& p = sh.plan;
    const auto word = [&](const panel::Box& b, const std::string& text, uint32_t rgb, float px, bool bold, Align a) {
        out.push_back(Words{b, text, rgb, px, bold, a});
    };
    const panel::Box& lr = L.planLevelRow;
    word(panel::Box{lr.x, lr.y, 150 * s, lr.h}, t.planLevel, kMuted, 12.5f * s, false, Align::Left);
    word(L.levelPrev, "\xE2\x80\xB9", kText, 14 * s, true, Align::Center);
    word(panel::Box{L.levelPrev.x + L.levelPrev.w, lr.y, L.levelNext.x - L.levelPrev.x - L.levelPrev.w, lr.h},
         std::to_string(p.level), p.level > d.baseLevel ? kGold : kText, 13 * s, true, Align::Center);
    word(L.levelNext, "\xE2\x80\xBA", kText, 14 * s, true, Align::Center);
    const int avail = planAvailable(d, p), used = planUsed(d, p);
    std::string points = fill(t.planPoints, {std::to_string(d.free), std::to_string(avail - d.free),
                                             std::to_string(p.level), std::to_string(avail), std::to_string(used)});
    if (used > avail) points += " \xE2\x80\x94 " + fill(t.planShort, {std::to_string(used - avail)});
    word(panel::Box{lr.x, lr.y + lr.h, L.planResults.x - lr.x - 10 * s, 30 * s}, points,
         used > avail ? 0xE07A6A : kFaint, 11.5f * s, false, Align::Left);
    for (int i = 0; i < kStats && i < int(L.planRowsBoxes.size()); ++i) {
        const panel::Box& r = L.planRowsBoxes[size_t(i)];
        const bool moved = p.base[i] != d.base[i];
        word(panel::Box{r.x + 6 * s, r.y, 80 * s, r.h}, kNames[i], moved ? kText : kMuted, 13 * s, true, Align::Left);
        word(L.planMinus[size_t(i)], "\xE2\x88\x92", kText, 14 * s, true, Align::Center);
        const std::string v = moved ? std::to_string(d.base[i]) + " \xE2\x86\x92 " + std::to_string(p.base[i])
                                    : std::to_string(d.base[i]);
        word(panel::Box{r.x + 120 * s, r.y, 124 * s, r.h}, v, moved ? kText : 0xC9D1DE, 13 * s, moved, Align::Center);
        word(L.planPlus[size_t(i)], "+", kGreen, 14 * s, true, Align::Center);
        if (moved)
            word(panel::Box{r.x + 286 * s, r.y, 100 * s, r.h},
                 std::to_string(sm::pointsBetween(d.base[i], p.base[i])), kFaint, 11.5f * s, false, Align::Left);
    }
    word(L.planReset, t.planReset, kText, 12.5f * s, false, Align::Center);
    word(panel::Box{L.planReset.x + L.planReset.w + 12 * s, L.planReset.y, L.planResults.x - L.planReset.x -
                        L.planReset.w - 20 * s, L.planReset.h},
         t.planNote, kFaint, 11 * s, false, Align::Left);
    // .297: the buffs the plan counts
    word(panel::Box{L.buffsRow.x, L.buffsRow.y, L.buffsRow.w, 28 * s}, t.planBuffs, kMuted, 12 * s, false,
         Align::Left);
    for (int i = 0; i < kPlanBuffs; ++i) {
        if (L.buffChips[i].w <= 0) continue;
        const bool on = planBuff(d, p, i);
        word(L.buffChips[i],
             std::string(on ? "\xE2\x9C\x93 " : "") + kPlanBuffSpecs[i].name +
                 (d.buffNow[i] ? " \xC2\xB7 " + t.planBuffNow : std::string()),
             on ? kGold : kMuted, 11 * s, on, Align::Center);
    }
    // .269: the goals - ASPD aimed at (‹ ›), no cast
    const int goal = sh.aspdGoal > 0 ? sh.aspdGoal : std::min(sm::kMaxAspd, d.aspd + 5);
    word(panel::Box{L.goals.x, L.goals.y, 190 * s, 26 * s}, t.goals, kText, 13 * s, true, Align::Left);
    word(L.goalPrev, "\xE2\x80\xB9", kText, 14 * s, true, Align::Center);
    word(panel::Box{L.goalPrev.x + L.goalPrev.w, L.goals.y, L.goalNext.x - L.goalPrev.x - L.goalPrev.w, 26 * s},
         "ASPD " + std::to_string(goal), kGold, 12.5f * s, true, Align::Center);
    word(L.goalNext, "\xE2\x80\xBA", kText, 14 * s, true, Align::Center);
    const std::vector<std::string> goals = planGoals(d, p, goal, t);
    for (size_t i = 0; i < goals.size(); ++i)
        word(panel::Box{L.goals.x, L.goals.y + 26 * s + float(i) * 22 * s, L.goals.w, 22 * s}, goals[i], 0xC9D1DE,
             12 * s, false, Align::Left);
    // .296: the map's monsters - now and with the plan
    const panel::Box& mb = L.mobs;
    const float rowH = 22 * s, nameW = mb.w * 0.40f, colW = (mb.w - nameW) / 3;
    word(panel::Box{mb.x, mb.y, nameW, 26 * s}, fill(t.mobs, {d.map}), kText, 13 * s, true, Align::Left);
    const std::string heads[3] = {t.mobsHit, t.mobsDodge, t.mobsHits};
    for (int c = 0; c < 3; ++c)
        word(panel::Box{mb.x + nameW + float(c) * colW, mb.y, colW, 26 * s}, heads[c], kFaint, 11.5f * s, false,
             Align::Right);
    const std::vector<MobRow> mobs = planMobs(d, p);
    if (mobs.empty())
        word(panel::Box{mb.x, mb.y + 26 * s, mb.w, rowH}, t.mobsNone, kFaint, 12 * s, false, Align::Left);
    const auto value = [&](int now, int then, bool lowerBetter, const char* unit) {
        const auto one = [&](int v) { return v > 0 ? std::to_string(v) + unit : std::string("\xE2\x80\x94"); };
        if (now == then) return std::make_pair(one(now), kText);
        const bool better = lowerBetter ? (then > 0 && (now == 0 || then < now)) : then > now;
        return std::make_pair(one(now) + " \xE2\x86\x92 " + one(then), better ? kGreen : 0xE07A6Au);
    };
    for (size_t i = 0; i < mobs.size(); ++i) {
        const MobRow& r = mobs[i];
        const float y = mb.y + 26 * s + float(i) * rowH;
        word(panel::Box{mb.x, y, nameW, rowH}, r.name + "  Lv " + std::to_string(r.level), 0xC9D1DE, 12 * s, false,
             Align::Left);
        const std::pair<std::string, uint32_t> cells[3] = {value(r.hitNow, r.hitThen, false, " %"),
                                                           value(r.dodgeNow, r.dodgeThen, false, " %"),
                                                           value(r.hitsNow, r.hitsThen, true, "")};
        for (int c = 0; c < 3; ++c)
            word(panel::Box{mb.x + nameW + float(c) * colW, y, colW, rowH}, cells[c].first, cells[c].second, 12 * s,
                 cells[c].second != kText, Align::Right);
    }
    const panel::Box& res = L.planResults;
    word(panel::Box{res.x, res.y, res.w, kHeaderH * s}, t.planResult, kText, 13 * s, true, Align::Left);
    const std::vector<PlanRow> rows = planRows(d, p, t);
    for (size_t i = 0; i < rows.size(); ++i) {
        const float y = res.y + kHeaderH * s + float(i) * kBattleRowH * s;
        word(panel::Box{res.x, y, res.w * 0.42f, kBattleRowH * s}, rows[i].name, 0xC9D1DE, 12.5f * s, false,
             Align::Left);
        const std::string v = rows[i].changed ? rows[i].now + " \xE2\x86\x92 " + rows[i].then : rows[i].now;
        word(panel::Box{res.x + res.w * 0.42f, y, res.w * 0.58f, kBattleRowH * s}, v, rows[i].changed ? kGreen : kText,
             12.5f * s, rows[i].changed, Align::Right);
    }
}

// 3.0.11-beta.267 (stage 3): «Довідка» - the class (‹ ›), its weapons' ASPD at AGI 1 / 50 / 80 / 99 and at the
// character's own AGI (the character's DEX), what a stat costs up to 50 ... 99, the job levels giving each stat
void referenceWords(std::vector<Words>& out, const Layout& L, const Data& d, const Texts& t) {
    namespace sm = uagfx::statmath;
    const float s = L.s, rowH = 22 * s, headH = 26 * s;
    const int job = L.refJob, agi = total(d, 1), dex = total(d, 4);
    const char* cls = job >= 0 ? charblock::jobName(job) : "";
    const auto word = [&](float x, float y, float w, float h, const std::string& text, uint32_t rgb, float px,
                          bool bold, Align a) { out.push_back(Words{panel::Box{x, y, w, h}, text, rgb, px, bold, a}); };
    const panel::Box& c = L.classRow;
    word(c.x, c.y, 150 * s, c.h, t.refClass, kMuted, 12.5f * s, false, Align::Left);
    word(L.prev.x, L.prev.y, L.prev.w, L.prev.h, "\xE2\x80\xB9", kText, 14 * s, true, Align::Center);
    word(L.prev.x + L.prev.w, c.y, L.next.x - L.prev.x - L.prev.w, c.h,
         *cls ? std::string(cls) : (job >= 0 ? "#" + std::to_string(job) : std::string("\xE2\x80\x94")),
         job == d.job ? kGold : kText, 13 * s, true, Align::Center);
    word(L.next.x, L.next.y, L.next.w, L.next.h, "\xE2\x80\xBA", kText, 14 * s, true, Align::Center);
    // ASPD: Зброя | Затримка | AGI 1 50 80 99 | your AGI
    const panel::Box& a = L.aspd;
    word(a.x, a.y, a.w, headH, fill(t.refAspd, {std::to_string(dex)}), kText, 13 * s, true, Align::Left);
    const float c0 = a.x, c1 = a.x + 300 * s, step = 62 * s;
    const float hy = a.y + headH;
    word(c0, hy, 200 * s, headH, t.refWeapon, kFaint, 11.5f * s, false, Align::Left);
    word(c1 - 80 * s, hy, 80 * s, headH, t.refDelay, kFaint, 11.5f * s, false, Align::Right);
    for (int i = 0; i < 4; ++i)
        word(c1 + float(i) * step, hy, step, headH, "AGI " + std::to_string(kAgiSteps[i]), kFaint, 11.5f * s, false,
             Align::Right);
    word(c1 + 4 * step, hy, step + 30 * s, headH, fill(t.refYourAgi, {std::to_string(agi)}), kGold, 11.5f * s, true,
         Align::Right);
    const int worn = job == d.job ? weaponOf(d) : -1;
    const std::vector<AspdRow> rows = aspdTable(job, agi, dex);
    for (size_t i = 0; i < rows.size(); ++i) {
        const AspdRow& r = rows[i];
        const float y = hy + headH + float(i) * rowH;
        const uint32_t ink = r.weapon == worn ? kGold : kText;
        word(c0, y, 260 * s, rowH, weaponName(r.weapon), ink, 12 * s, r.weapon == worn, Align::Left);
        word(c1 - 80 * s, y, 80 * s, rowH, std::to_string(r.delay), kMuted, 12 * s, false, Align::Right);
        for (int k = 0; k < 4; ++k)
            word(c1 + float(k) * step, y, step, rowH, std::to_string(r.at[k]), 0xC9D1DE, 12 * s, false, Align::Right);
        word(c1 + 4 * step, y, step + 30 * s, rowH, std::to_string(r.mine), kGold, 12 * s, true, Align::Right);
    }
    // the stat's price from 1
    const panel::Box& p = L.cost;
    word(p.x, p.y, p.w, headH, t.refCost, kText, 13 * s, true, Align::Left);
    word(c0, p.y + headH, 200 * s, headH, t.refCostFrom, kFaint, 11.5f * s, false, Align::Left);
    word(c0, p.y + 2 * headH, 200 * s, rowH, t.refCostPoints, kMuted, 12 * s, false, Align::Left);
    for (int i = 0; i < 5; ++i) {
        const float x = c1 - 80 * s + float(i) * step;
        word(x, p.y + headH, step, headH, std::to_string(kCostSteps[i]), kFaint, 11.5f * s, false, Align::Right);
        word(x, p.y + 2 * headH, step, rowH, std::to_string(sm::pointsBetween(1, kCostSteps[i])), kText, 12 * s, true,
             Align::Right);
    }
    // the job levels' stats
    const panel::Box& b = L.bonus;
    word(b.x, b.y, b.w, headH, t.refBonus, kText, 13 * s, true, Align::Left);
    for (int i = 0; i < kStats; ++i) {
        const std::vector<int> lv = sm::bonusLevels(job, i);
        std::string list;
        int now = 0;
        for (int l : lv) {
            list += (list.empty() ? "" : ", ") + std::to_string(l);
            if (job == d.job && l <= d.jobLevel) ++now;
        }
        const float y = b.y + headH + float(i) * 20 * s;
        word(c0, y, 60 * s, 20 * s, kNames[i], kText, 12 * s, true, Align::Left);
        word(c0 + 60 * s, y, b.w - 60 * s, 20 * s,
             lv.empty() ? t.refBonusNone
                        : list + (job == d.job ? "  " + fill(t.refBonusNow,
                                                              {std::to_string(now), std::to_string(lv.size())})
                                               : std::string()),
             lv.empty() ? kFaint : 0xC9D1DE, 12 * s, false, Align::Left);
    }
}

}  // namespace

std::vector<Words> words(const Layout& L, const Data& d, const Show& sh, const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s, side = kSide * s;
    const float titleW = m(t.title, 15 * s, true) + 2;
    out.push_back(Words{panel::Box{side, 0, titleW, L.title.h}, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{panel::Box{side + titleW + 8 * s, 0, m(t.hotkey, 11 * s, false) + 2, L.title.h}, t.hotkey,
                        kFaint, 11 * s, false, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});
    out.push_back(Words{L.freeChip, fill(t.free, {std::to_string(d.free)}), d.free > 0 ? kGold : kFaint, 11.5f * s,
                        true, Align::Center});
    for (int i = 0; i < kTabs; ++i)  // 3.0.11-beta.267: the tabs
        out.push_back(Words{L.tabs[i], t.tabs[i], i == L.tab ? kGold : kMuted, 12 * s, i == L.tab, Align::Center});
    if (L.tab == kPlanTab) {
        planWords(out, L, d, sh, t);
        return out;
    }
    if (L.tab == kRefTab) {
        referenceWords(out, L, d, t);
        return out;
    }
    const float hx = L.header.x;
    const float rights[4] = {kBaseRight, kBonusRight, kTotalRight, kCostRight};
    out.push_back(Words{panel::Box{hx + 6 * s, L.header.y, 70 * s, L.header.h}, t.columns[0], kFaint, 11.5f * s,
                        false, Align::Left});
    for (int c = 0; c < 4; ++c)
        out.push_back(Words{panel::Box{hx + (rights[c] - 64) * s, L.header.y, 64 * s, L.header.h}, t.columns[c + 1],
                            kFaint, 11.5f * s, false, Align::Right});
    for (int i = 0; i < kStats; ++i) {
        const panel::Box& r = L.rows[size_t(i)];
        const auto cell = [&](float right, const std::string& text, uint32_t rgb, bool bold) {
            out.push_back(Words{panel::Box{r.x + (right - 64) * s, r.y, 64 * s, r.h}, text, rgb, 13 * s, bold,
                                Align::Right});
        };
        out.push_back(Words{panel::Box{r.x + 6 * s, r.y, 60 * s, r.h}, kNames[i], kText, 13 * s, true, Align::Left});
        cell(kBaseRight, std::to_string(d.base[i]), 0xC9D1DE, false);
        if (d.bonus[i]) cell(kBonusRight, (d.bonus[i] > 0 ? "+" : "") + std::to_string(d.bonus[i]), kGreen, false);
        cell(kTotalRight, std::to_string(d.base[i] + d.bonus[i]), kText, true);
        if (d.base[i] >= 99)
            cell(kCostRight, t.maxed, kFaint, false);
        else if (d.need[i] > 0)
            cell(kCostRight, std::to_string(d.need[i]), canRaise(d, i) ? 0xC9D1DE : kFaint, false);
        if (d.base[i] < 99)
            out.push_back(Words{L.pluses[size_t(i)], "+", canRaise(d, i) ? kText : kFaint, 15 * s, true,
                                Align::Center});
    }
    out.push_back(Words{panel::Box{L.battle.x, L.battle.y, L.battle.w, kHeaderH * s}, t.battle, kText, 13 * s, true,
                        Align::Left});
    const std::vector<Row> rows = battleRows(d, t);
    for (size_t i = 0; i < rows.size(); ++i) {
        const float y = L.battle.y + kHeaderH * s + float(i) * kBattleRowH * s;
        out.push_back(Words{panel::Box{L.battle.x, y, L.battle.w / 2, kBattleRowH * s}, rows[i].name, 0xC9D1DE,
                            12.5f * s, false, Align::Left});
        out.push_back(Words{panel::Box{L.battle.x + L.battle.w / 2, y, L.battle.w / 2, kBattleRowH * s}, rows[i].value,
                            kText, 12.5f * s, true, Align::Right});
    }
    // 3.0.11-beta.266: the panel «Як рахується» - the item's title, its formulas on the character's numbers
    const Explained e = explained(d, explainedItem(sh), t);
    const float px = L.line.y + 4 * s + kExplainPad * s / 2;
    out.push_back(Words{panel::Box{side, px, L.glass.w - 2 * side, kExplainTitleH * s - 6 * s}, e.title,
                        e.lines.empty() ? kFaint : kGold, 12.5f * s, !e.lines.empty(), Align::Left});
    for (size_t i = 0; i < e.lines.size(); ++i)
        out.push_back(Words{panel::Box{side, px + kExplainTitleH * s - 6 * s + float(i) * kExplainLineH * s,
                                       L.glass.w - 2 * side, kExplainLineH * s},
                            e.lines[i], i == 0 ? kText : 0xC9D1DE, 12 * s, false, Align::Left});
    return out;
}

}  // namespace uagfx::statwin
