#include "core/target_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/reference_pages.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::targetcard {
namespace {

// The concept's sizes at scale 1 (target16, v19_cave): the card 48 high, its round 32, its book 30, its chip 22
constexpr float kH = 48, kPad = 8, kRound = 32, kBook = 30, kChipH = 22, kGap = 10, kLine = 18, kCorner = 11;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kTile = 0x1B1D24;
constexpr uint32_t kGold = 0xC9A55B, kText = 0xECEFF5, kMuted = 0x9AA3B5;
constexpr uint32_t kGreen = 0x5BD36C, kRed = 0xE5534B;  // 3.0.11-beta.95: the HP bar's, as over the monsters' heads

std::string replaced(std::string text, const std::string& value) {
    const size_t at = text.find("{1}");
    if (at != std::string::npos) text.replace(at, 3, value);
    return text;
}

std::string levelText(const Data& d) { return "Lv " + std::to_string(d.level); }

// The details' lines: its kind, its numbers, what hurts it, what heals it, the click's
std::vector<std::pair<std::string, uint32_t>> lines(const Data& d, const Texts& t) {
    std::vector<std::pair<std::string, uint32_t>> out;
    if (!d.kind.empty()) out.emplace_back(d.kind, kText);
    if (!d.numbers.empty()) out.emplace_back(d.numbers, kMuted);
    if (!d.weak.empty()) out.emplace_back(replaced(t.weak, d.weak), 0xE6CB8A);
    if (!d.heals.empty()) out.emplace_back(replaced(t.heals, d.heals), 0xFF9A8F);
    if (!d.bar.empty() && !t.bar.empty()) out.emplace_back(replaced(t.bar, d.bar), kText);  // 3.0.11-beta.68
    if (!t.book.empty()) out.emplace_back(t.book, kMuted);
    return out;
}

}  // namespace

Layout layout(const Data& d, const Texts& t, int centreX, int top, float scale, bool details) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float nameW = panel::wordsWidth(d.name, 16 * s, true) + 4 * s;
    const float levelW = panel::wordsWidth(levelText(d), 13 * s, false) + 4 * s;
    const float chipW = d.element.empty() ? 0 : panel::wordsWidth(d.element, 12.5f * s, true) + 18 * s;
    float x = kPad * s;
    const bool hp = d.hp >= 0;  // 3.0.11-beta.95: the name's line up, the HP bar under it
    L.round = panel::Box{x, kPad * s, kRound * s, kRound * s};
    x += kRound * s + kGap * s;
    L.name = panel::Box{x, (hp ? 5 : 14) * s, nameW, 20 * s};
    x += nameW + 6 * s;
    L.level = panel::Box{x, (hp ? 6 : 15) * s, levelW, 20 * s};
    x += levelW + 8 * s;
    L.element = panel::Box{x, (hp ? 4 : (kH - kChipH) / 2) * s, chipW, (hp ? kChipH - 2 : kChipH) * s};
    x += chipW;
    if (hp) {
        const float pct = panel::wordsWidth("100%", 12 * s, true) + 2 * s;
        const float end = std::max(x, L.name.x + 80 * s + 6 * s + pct);
        L.hp = panel::Box{L.name.x, 32 * s, end - pct - 6 * s - L.name.x, 5 * s};
        L.hpText = panel::Box{end - pct, 26 * s, pct, 16 * s};
        x = end;
    }
    x += 12 * s;
    L.book = panel::Box{x, (kH - kBook) / 2 * s, kBook * s, kBook * s};
    x += kBook * s;
    if (d.pad && !t.attack.empty()) {  // 3.0.11-beta.86: the controller's buttons after a line
        const float most =
            std::max(panel::wordsWidth(t.attack, 13 * s, false), panel::wordsWidth(t.next, 13 * s, false));
        x += 13 * s;
        L.hints = panel::Box{x, 6 * s, 24 * s + most, 36 * s};
        x += L.hints.w;
    }
    x += kPad * s;
    L.card = panel::Box{0, 0, x, kH * s};
    float width = L.card.w;
    if (details) {
        const std::vector<std::pair<std::string, uint32_t>> l = lines(d, t);
        float most = 0;
        for (const auto& [text, rgb] : l) most = std::max(most, panel::wordsWidth(text, 13 * s, false));
        const float h = (10 + kLine * float(l.size())) * s;
        L.details = panel::Box{0, L.card.h + 6 * s, std::max(L.card.w, most + 24 * s), h};
        width = std::max(width, L.details.w);
    }
    // the card and its details centred in the picture, the picture's middle at the screen's
    L.w = int(std::ceil(width));
    const float shiftCard = (float(L.w) - L.card.w) / 2;
    for (panel::Box* b : {&L.card, &L.round, &L.name, &L.level, &L.element, &L.book, &L.hints, &L.hp, &L.hpText})
        b->x += shiftCard;
    if (details) L.details.x = (float(L.w) - L.details.w) / 2;
    L.h = int(std::ceil(details ? L.details.y + L.details.h : L.card.h));
    L.x = centreX - L.w / 2;
    L.y = top;
    return L;
}

Part hitTest(const Layout& L, float x, float y) {
    if (L.book.contains(x, y)) return Part::Book;
    return L.card.contains(x, y) ? Part::Card : Part::None;
}

std::vector<std::pair<std::string, uint32_t>> detailLines(const Data& d, const Texts& t) { return lines(d, t); }

Weakness weakness(int element, int level, size_t most) {
    Weakness w;
    for (int a = 0; a < 10; ++a) {
        const int damage = refpages::elementDamage(a, element, level);
        if (damage >= 150) w.weak.emplace_back(a, damage);
        if (damage < 0) w.heals.push_back(a);
    }
    std::stable_sort(w.weak.begin(), w.weak.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (w.weak.size() > most) w.weak.resize(most);
    return w;
}

}  // namespace uagfx::targetcard
