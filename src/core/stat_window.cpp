#include "core/stat_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/char_block.hpp"
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
        const int a = statmath::amotion(d.job, w, total(d, 1), total(d, 4));
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
    L.tab = tab == 1 ? 1 : 0;
    L.refJob = refJob >= 0 ? refJob : d.job;
    const float w = kW * s, side = kSide * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    const std::string chip = fill(t.free, {std::to_string(d.free)});
    const float chipW = m(chip, 11.5f * s, true) + 18 * s;
    L.freeChip = panel::Box{L.close.x - 14 * s - chipW, (kTitleH * s - 22 * s) / 2, chipW, 22 * s};
    // 3.0.11-beta.267 (stage 3): the tabs after the title and its key
    float tx = side + m(t.title, 15 * s, true) + 2 + 8 * s + m(t.hotkey, 11 * s, false) + 2 + 22 * s;
    for (int i = 0; i < 2; ++i) {
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
    L.glass = panel::Box{0, 0, w, std::max(L.line.y + L.line.h, refBottom)};  // one height for both tabs
    if (L.tab == 1) {  // «Довідка»: none of «Персонаж»'s rows to point at
        L.rows.clear();
        L.pluses.clear();
        L.battleRows.clear();
    }
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < 2; ++i)  // 3.0.11-beta.267
        if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, i};
    if (L.tab == 1 && L.prev.contains(x, y)) return Hit{Part::Arrow, 0};
    if (L.tab == 1 && L.next.contains(x, y)) return Hit{Part::Arrow, 1};
    for (size_t i = 0; i < L.pluses.size(); ++i)
        if (L.pluses[i].contains(x, y)) return Hit{Part::Plus, int(i)};
    for (size_t i = 0; i < L.rows.size(); ++i)
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, int(i)};
    for (size_t i = 0; i < L.battleRows.size(); ++i)
        if (L.battleRows[i].contains(x, y)) return Hit{Part::Battle, int(i)};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const Data& d, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    if (d.free > 0) panel::roundRect(out, L.freeChip, 6 * s, kGold, 0.18f, kGold, 0.5f, 1.0f * s);
    for (int i = 0; i < 2; ++i)  // 3.0.11-beta.267: the tabs, the shown one lit
        panel::roundRect(out, L.tabs[i], 13 * s, kWhite, i == L.tab ? 0.14f : sh.hoverTab == i ? 0.08f : 0.0f,
                         i == L.tab ? kGold2 : kWhite, i == L.tab ? 0.7f : 0.1f, 1.0f * s);
    if (L.tab == 1) {
        for (int i = 0; i < 2; ++i)
            panel::roundRect(out, i ? L.next : L.prev, 6 * s, kWhite, sh.hoverArrow == i ? 0.2f : 0.08f);
        for (const panel::Box* b : {&L.aspd, &L.cost, &L.bonus})
            panel::roundRect(out, panel::Box{b->x, b->y + 26 * s - 1 * s, b->w, 1 * s}, 0, kWhite, 0.1f);
        return;
    }
    panel::roundRect(out, panel::Box{L.header.x, L.header.y + L.header.h - 1 * s, L.header.w, 1 * s}, 0, kWhite,
                     0.08f);
    const int lit = sh.hoverRow >= 0 ? sh.hoverRow : sh.hoverPlus >= 0 ? sh.hoverPlus : sh.selected;
    for (int i = 0; i < kStats; ++i) {
        const panel::Box& r = L.rows[size_t(i)];
        if (i == lit) panel::roundRect(out, r, 6 * s, kWhite, 0.06f);
        if (i < kStats - 1)
            panel::roundRect(out, panel::Box{r.x, r.y + r.h - 1 * s, r.w, 1 * s}, 0, kWhite, 0.05f);
        if (d.base[i] >= 99) continue;
        const bool can = canRaise(d, i);
        const panel::Box& p = L.pluses[size_t(i)];
        panel::roundRect(out, p, 6 * s, can ? kWhite : kWhite, can ? (i == sh.hoverPlus ? 0.22f : 0.12f) : 0.03f,
                         can ? kGold2 : kWhite, can ? 0.7f : 0.06f, 1.0f * s);
    }
    const int lit2 = sh.hoverBattle >= 0 ? sh.hoverBattle : sh.pinnedBattle;  // 3.0.11-beta.266
    if (lit2 >= 0 && lit2 < int(L.battleRows.size()))
        panel::roundRect(out, L.battleRows[size_t(lit2)], 6 * s, kWhite, sh.pinnedBattle == lit2 ? 0.09f : 0.06f);
    // the panel «Як рахується»: a tinted ground under the formulas
    const panel::Box ground{kSide * s / 2, L.line.y + 4 * s, L.glass.w - kSide * s, L.line.h - 10 * s};
    panel::roundRect(out, ground, 8 * s, kWhite, 0.04f, kWhite, 0.08f, 1.0f * s);
}

namespace {

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
    for (int i = 0; i < 2; ++i)  // 3.0.11-beta.267: the tabs
        out.push_back(Words{L.tabs[i], t.tabs[i], i == L.tab ? kGold : kMuted, 12 * s, i == L.tab, Align::Center});
    if (L.tab == 1) {
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
