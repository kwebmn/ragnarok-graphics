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

float cover(float d) { return std::clamp(0.5f - d, 0.0f, 1.0f); }

// A premultiplied picture inside a circle (its middle at the box's), over what is there
void putRound(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const float cx = box.x + box.w / 2, cy = box.y + box.h / 2, r = box.w / 2;
    const int ox = int(std::lround(cx - float(pic.width) / 2)), oy = int(std::lround(cy - float(pic.height) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const float k = cover(panel::length(float(tx) + 0.5f - cx, float(ty) + 0.5f - cy) - r);
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (k <= 0 || !(c >> 24)) continue;
            if (k < 1) {
                const auto ch = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * k + 0.5f); };
                c = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
            }
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

void corners(panel::Picture& p, const panel::Box& b, float s) {
    const float len = kCorner * s, w = 2 * s, e = 1 * s;
    const panel::Box bars[] = {{b.x, b.y, len, w},
                               {b.x, b.y, w, len},
                               {b.x + b.w - len, b.y, len, w},
                               {b.x + b.w - w, b.y, w, len},
                               {b.x, b.y + b.h - w, len, w},
                               {b.x, b.y + b.h - len, w, len},
                               {b.x + b.w - len, b.y + b.h - w, len, w},
                               {b.x + b.w - w, b.y + b.h - len, w, len}};
    for (const panel::Box& x : bars)
        panel::roundRect(p, panel::Box{x.x - e, x.y - e, x.w + 2 * e, x.h + 2 * e}, 0, kBlack, 0.55f);
    for (const panel::Box& x : bars) panel::roundRect(p, x, 0, kGold, 1.0f);
}

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

void paint(panel::Picture& out, const Layout& L, const Data& d, bool hoverBook) {
    const float s = L.s;
    panel::roundRect(out, L.card, 10 * s, kCard, uistyle::panel(), kWhite, 0.09f, 1.0f * s);
    corners(out, L.card, s);
    const panel::Box& r = L.round;
    panel::circle(out, r.x + r.w / 2, r.y + r.h / 2, r.w / 2, kTile, 1.0f);
    if (d.picture && d.picture->width > 0) putRound(out, *d.picture, r);
    if (L.element.w > 0)
        panel::roundRect(out, L.element, 6 * s, d.elementRgb, 0.22f, d.elementRgb, 0.6f, 1.0f * s);
    if (L.hp.w > 0) {  // 3.0.11-beta.95: green, red below a quarter, over a dark rest
        panel::roundRect(out, L.hp, L.hp.h / 2, kBlack, 0.55f);
        const float done = L.hp.w * float(std::clamp(d.hp, 0, 100)) / 100.0f;
        if (done > 0.5f)
            panel::roundRect(out, panel::Box{L.hp.x, L.hp.y, done, L.hp.h}, L.hp.h / 2, d.hp < 25 ? kRed : kGreen,
                             1.0f);
    }
    panel::roundRect(out, L.book, 7 * s, kWhite, hoverBook ? 0.14f : 0.06f, kGold, hoverBook ? 0.8f : 0.0f, 1.0f * s);
    const float icon = 18 * s;
    uiicons::draw(out, uiicons::Icon::Quest, L.book.x + (L.book.w - icon) / 2, L.book.y + (L.book.h - icon) / 2, icon,
                  hoverBook ? kText : kMuted, 1.0f, 2.0f);
    if (L.hints.w > 0) {  // 3.0.11-beta.86: a line before them, A and the right stick
        const panel::Box& h = L.hints;
        panel::roundRect(out, panel::Box{h.x - 7 * s, L.card.y + 9 * s, 1 * s, L.card.h - 18 * s}, 0, kWhite, 0.10f);
        const padglyph::Family f = d.padSony ? padglyph::Family::Sony : padglyph::Family::Xbox;
        padglyph::paint(out, padglyph::Button::A, f, h.x, h.y, 17 * s);
        padglyph::paint(out, padglyph::Button::R3, f, h.x, h.y + 19 * s, 17 * s);
    }
    if (L.details.w > 0) panel::roundRect(out, L.details, 8 * s, kCard, 0.94f, kWhite, 0.12f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const Data& d, const Texts& t, bool hoverBook) {
    (void)hoverBook;
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.name, d.name, kText, 16 * s, true, Align::Left});
    out.push_back(Words{L.level, levelText(d), kMuted, 13 * s, false, Align::Left});
    if (L.element.w > 0) out.push_back(Words{L.element, d.element, d.elementRgb, 12.5f * s, true, Align::Center});
    if (L.hpText.w > 0)  // 3.0.11-beta.95
        out.push_back(Words{L.hpText, std::to_string(d.hp) + "%", kText, 12 * s, true, Align::Right});
    if (L.hints.w > 0) {  // 3.0.11-beta.86: its buttons' letters and what they do
        const panel::Box& h = L.hints;
        const padglyph::Family f = d.padSony ? padglyph::Family::Sony : padglyph::Family::Xbox;
        for (const padglyph::Letter& l : {padglyph::letter(padglyph::Button::A, f, h.x, h.y, 17 * s),
                                          padglyph::letter(padglyph::Button::R3, f, h.x, h.y + 19 * s, 17 * s)})
            if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        out.push_back(Words{panel::Box{h.x + 24 * s, h.y, h.w - 24 * s, 17 * s}, t.attack, kText, 13 * s, false,
                            Align::Left});
        out.push_back(Words{panel::Box{h.x + 24 * s, h.y + 19 * s, h.w - 24 * s, 17 * s}, t.next, kText, 13 * s, false,
                            Align::Left});
    }
    if (L.details.w > 0) {
        const std::vector<std::pair<std::string, uint32_t>> l = lines(d, t);
        for (size_t i = 0; i < l.size(); ++i)
            out.push_back(Words{panel::Box{L.details.x + 12 * s, L.details.y + (5 + kLine * float(i)) * s,
                                           L.details.w - 24 * s, kLine * s},
                                l[i].first, l[i].second, 13 * s, i == 0, Align::Left});
    }
    return out;
}

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
