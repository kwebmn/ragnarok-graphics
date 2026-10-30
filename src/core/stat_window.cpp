#include "core/stat_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace uagfx::statwin {
namespace {

// The concept's sizes at scale 1 (v17_windows): 760 wide; the title 46; the table's header 30, its rows 38; the
// battle's rows 26; the line under them 42
constexpr float kW = 760, kTitleH = 46, kHeaderH = 30, kRowH = 38, kBattleRowH = 26, kLineH = 42, kSide = 18;
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

int total(const Data& d, int i) { return d.base[i] + d.bonus[i]; }

}  // namespace

const char* const kNames[kStats] = {"STR", "AGI", "VIT", "INT", "DEX", "LUK"};

bool canRaise(const Data& d, int stat) {
    return stat >= 0 && stat < kStats && d.need[stat] > 0 && d.need[stat] <= d.free && d.base[stat] < 99;
}

std::vector<Row> battleRows(const Data& d, const Texts& t) {
    const auto plus = [](int a, int b) { return std::to_string(a) + " + " + std::to_string(b); };
    const double cast = std::min(100.0, double(total(d, 4)) * 100.0 / 150.0);
    return {Row{"ATK", plus(d.atk, d.atk2)},
            Row{"MATK", std::to_string(d.matkMin) + " ~ " + std::to_string(d.matkMax)},
            Row{"HIT", std::to_string(d.hit)},
            Row{"FLEE", plus(d.flee, d.flee2)},
            Row{"CRIT", std::to_string(d.crit)},
            Row{"DEF", plus(d.def, d.def2)},
            Row{"MDEF", plus(d.mdef, d.mdef2)},
            Row{"ASPD", std::to_string(d.aspd)},
            Row{t.cast, cast > 0 ? "\xE2\x88\x92" + std::to_string(int(cast)) + " %" : "0 %"}};  // −56 %
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
        case 4: {  // HIT +1; the cast a 150th shorter a point
            const double cast = std::min(100.0, double(v + 1) * 100.0 / 150.0);
            effect = fill(t.effects[4], {std::to_string(d.hit + 1), decimal(cast, t.decimal)});
            break;
        }
        default: effect = t.effects[5]; break;
    }
    return effect.empty() ? line : line + " \xC2\xB7 " + effect;
}

Layout layout(int x, int y, float scale, const Data& d, const Texts& t, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    const std::string chip = fill(t.free, {std::to_string(d.free)});
    const float chipW = m(chip, 11.5f * s, true) + 18 * s;
    L.freeChip = panel::Box{L.close.x - 14 * s - chipW, (kTitleH * s - 22 * s) / 2, chipW, 22 * s};
    L.table = panel::Box{side, kTitleH * s + 4 * s, kTableW * s, kHeaderH * s + float(kStats) * kRowH * s};
    L.header = panel::Box{L.table.x, L.table.y, L.table.w, kHeaderH * s};
    for (int i = 0; i < kStats; ++i) {
        const panel::Box r{L.table.x, L.table.y + kHeaderH * s + float(i) * kRowH * s, L.table.w, kRowH * s};
        L.rows.push_back(r);
        L.pluses.push_back(panel::Box{r.x + kPlusX * s, r.y + (r.h - 24 * s) / 2, 24 * s, 24 * s});
    }
    const float bx = L.table.x + L.table.w + 26 * s;
    L.battle = panel::Box{bx, L.table.y, w - side - bx, kHeaderH * s + 9 * kBattleRowH * s};
    const float bottom = std::max(L.table.y + L.table.h, L.battle.y + L.battle.h) + 8 * s;
    L.line = panel::Box{0, bottom, w, kLineH * s};
    L.glass = panel::Box{0, 0, w, L.line.y + L.line.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (size_t i = 0; i < L.pluses.size(); ++i)
        if (L.pluses[i].contains(x, y)) return Hit{Part::Plus, int(i)};
    for (size_t i = 0; i < L.rows.size(); ++i)
        if (L.rows[i].contains(x, y)) return Hit{Part::Row, int(i)};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const Data& d, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, 0.9f, kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    if (d.free > 0) panel::roundRect(out, L.freeChip, 6 * s, kGold, 0.18f, kGold, 0.5f, 1.0f * s);
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
    panel::roundRect(out, panel::Box{0, L.line.y, L.glass.w, 1 * s}, 0, kWhite, 0.08f);
}

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
    const int lit = sh.hoverRow >= 0 ? sh.hoverRow : sh.hoverPlus >= 0 ? sh.hoverPlus : sh.selected;
    out.push_back(Words{panel::Box{side, L.line.y, L.glass.w - 2 * side, L.line.h}, explain(d, lit, t),
                        lit >= 0 ? 0xC9D1DE : kFaint, 12 * s, false, Align::Left});
    return out;
}

}  // namespace uagfx::statwin
