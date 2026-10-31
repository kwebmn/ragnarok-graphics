#include "core/equip_window.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/ui_style.hpp"

namespace uagfx::equipwin {
namespace {

// The concept's sizes at scale 1: 880 wide, the title 46, a slot 56 high every 64, the middle 220, the footer 40
constexpr float kW = 880, kTitleH = 46, kSide = 16, kSlotH = 56, kSlotStep = 64, kColumnW = 314, kIcon = 44;
constexpr float kCentreW = 220, kPortrait = 96, kFooterH = 40, kPickH = 34, kDot = 7, kDotStep = 10, kGlyph = 17;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xF3C653, kGold2 = 0xC9A55B, kTile = 0x1B1D24;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kBlue = 0x8FB8F0;

std::string fill(std::string form, const std::vector<std::string>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        for (size_t at = form.find(key); at != std::string::npos; at = form.find(key, at + values[i].size()))
            form.replace(at, key.size(), values[i]);
    }
    return form;
}

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box, float k) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (k < 1) {
                const auto ch = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * k + 0.5f); };
                c = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
            }
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

bool left(int slot) { return slot >= HeadMid && slot <= AccL; }

std::string named(const Worn& w) { return w.refine > 0 ? "+" + std::to_string(w.refine) + " " + w.name : w.name; }

struct PadItem {
    padglyph::Button button;
    const std::string* words;
};

std::vector<PadItem> padItems(const Data& d, const Show& sh, const Texts& t) {
    if (sh.picking) return {{padglyph::Button::A, &t.put}, {padglyph::Button::B, &t.back}};
    const bool worn = sh.focus >= 0 && sh.focus < kSlots && d.slots[sh.focus].index > 0;
    if (!worn) return {{padglyph::Button::A, &t.put}, {padglyph::Button::B, &t.close}};
    return {{padglyph::Button::A, &t.off}, {padglyph::Button::X, &t.change}, {padglyph::Button::Y, &t.card},
            {padglyph::Button::B, &t.close}};
}

}  // namespace

const uint32_t kBits[kSlots] = {0x200, 0x001, 0x010, 0x004, 0x008, 0x100, 0x002, 0x020, 0x040, 0x080, 0x8000};

int bitIndex(uint32_t bit) {
    if (bit == 0x8000) return 15;
    for (int i = 0; i < 10; ++i)
        if (bit & (1u << i)) return i;
    return -1;
}

std::vector<int> slotsOf(uint32_t location) {
    static const int kOrder[kSlots] = {HeadTop, HeadMid, HeadLow, Weapon, Shield, Armor, Garment, Shoes, AccL, AccR,
                                       Ammo};
    std::vector<int> out;
    for (int s : kOrder)
        if (location & kBits[s]) out.push_back(s);
    return out;
}

bool fits(uint32_t location, int slot) { return slot >= 0 && slot < kSlots && (location & kBits[slot]) != 0; }

int iconSide(float scale) { return int(std::lround(32 * scale)); }

Layout layout(int x, int y, float scale, const Data& d, const Show& sh, const Texts& t, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s, top = (kTitleH + 10) * s;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    const float columnH = 4 * kSlotStep * s + kSlotH * s;
    const float rightX = w - side - kColumnW * s;
    for (int i = 0; i < 10; ++i) {
        const bool l = left(i);
        const float sy = top + float(l ? i : i - 5) * kSlotStep * s;
        L.slots[i] = panel::Box{l ? side : rightX, sy, kColumnW * s, kSlotH * s};
        const float ix = l ? side + kColumnW * s - 8 * s - kIcon * s : rightX + 8 * s;
        L.icons[i] = panel::Box{ix, sy + (kSlotH - kIcon) / 2 * s, kIcon * s, kIcon * s};
        const Worn& wi = d.slots[i];
        if (wi.index > 0 && wi.slots > 0 && !wi.spill) {
            const float dw = float(std::min(wi.slots, 4)) * kDotStep * s - (kDotStep - kDot) * s;
            const float dx = l ? L.icons[i].x - 10 * s - dw : L.icons[i].x + L.icons[i].w + 10 * s;
            L.dots[i] = panel::Box{dx, sy + 40 * s, dw, kDot * s};
        }
    }
    L.centre = panel::Box{(w - kCentreW * s) / 2, top, kCentreW * s, columnH};
    L.portrait = panel::Box{L.centre.x + (L.centre.w - kPortrait * s) / 2, top + 4 * s, kPortrait * s, kPortrait * s};
    L.slots[Ammo] = panel::Box{L.centre.x + 6 * s, top + columnH - 50 * s, L.centre.w - 12 * s, 48 * s};
    L.icons[Ammo] = panel::Box{L.slots[Ammo].x + 4 * s, L.slots[Ammo].y + 2 * s, kIcon * s, kIcon * s};
    if (sh.picking) {
        L.pickList = L.centre;
        const int most = int((L.pickList.h - 32 * s) / (kPickH * s));
        for (size_t i = 0; i < d.picks.size() && int(i) < most; ++i)
            L.picks.push_back(panel::Box{L.pickList.x + 6 * s, L.pickList.y + 30 * s + float(i) * kPickH * s,
                                         L.pickList.w - 12 * s, (kPickH - 2) * s});
    }
    L.footer = panel::Box{0, top + columnH + 12 * s, w, kFooterH * s};
    if (sh.pad) {  // the controller's buttons in the footer, from its right
        const std::vector<PadItem> items = padItems(d, sh, t);
        float right = w - side;
        L.padGlyphs.resize(items.size());
        L.padWords.resize(items.size());
        for (size_t i = items.size(); i-- > 0;) {
            const float ww = m(*items[i].words, 12 * s, false) + 2;
            const float gw = padglyph::widthOf(items[i].button, kGlyph * s);
            L.padWords[i] = panel::Box{right - ww, L.footer.y, ww, L.footer.h};
            L.padGlyphs[i] = panel::Box{L.padWords[i].x - 6 * s - gw, L.footer.y + (L.footer.h - kGlyph * s) / 2, gw,
                                        kGlyph * s};
            right = L.padGlyphs[i].x - 16 * s;
        }
    }
    L.glass = panel::Box{0, 0, w, L.footer.y + L.footer.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (size_t i = 0; i < L.picks.size(); ++i)
        if (L.picks[i].contains(x, y)) return Hit{Part::Pick, int(i)};
    for (int i = 0; i < kSlots; ++i)
        if (L.slots[i].w > 0 && L.slots[i].contains(x, y)) return Hit{Part::Slot, i};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const Data& d, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    for (int i = 0; i < kSlots; ++i) {
        const panel::Box& b = L.slots[i];
        if (b.w <= 0) continue;
        if (i == sh.hoverSlot && !sh.picking) panel::roundRect(out, b, 8 * s, kWhite, 0.06f);
        if (sh.pad && i == sh.focus) panel::roundRect(out, b, 8 * s, kGold2, 0.08f, kGold, 0.9f, 1.5f * s);
        if ((sh.dropFits >> i) & 1u) panel::roundRect(out, b, 8 * s, kGold2, 0.12f, kGold, 0.95f, 2.0f * s);  // .110
        panel::roundRect(out, L.icons[i], 8 * s, kTile, 0.9f, kWhite, 0.08f, 1.0f * s);
        const Worn& wi = d.slots[i];
        if (wi.index > 0 && wi.icon && wi.icon->width > 0) put(out, *wi.icon, L.icons[i], wi.spill ? 0.4f : 1.0f);
        const panel::Box& dots = L.dots[i];
        for (int n = 0; dots.w > 0 && n < std::min(wi.slots, 4); ++n) {
            const float cx = dots.x + (float(n) * kDotStep + kDot / 2) * s, cy = dots.y + dots.h / 2;
            if (wi.cards[n] > 0)
                panel::circle(out, cx, cy, kDot / 2 * s, kGold, 1.0f);
            else
                panel::circle(out, cx, cy, kDot / 2 * s, kWhite, 0.16f);
        }
    }
    if (sh.picking) {  // the list of what fits, over the middle
        panel::roundRect(out, L.pickList, 10 * s, kTile, 0.95f, kGold2, 0.5f, 1.0f * s);
        for (size_t i = 0; i < L.picks.size(); ++i) {
            if (int(i) == sh.hoverPick) panel::roundRect(out, L.picks[i], 6 * s, kWhite, 0.07f);
            if (int(i) == sh.pickFocus) panel::roundRect(out, L.picks[i], 6 * s, kGold2, 0.1f, kGold, 0.9f, 1.5f * s);
            const Pick& p = d.picks[i];
            if (p.icon && p.icon->width > 0)
                put(out, *p.icon, panel::Box{L.picks[i].x + 2 * s, L.picks[i].y, 30 * s, L.picks[i].h}, 1.0f);
        }
    } else {
        const panel::Box& r = L.portrait;
        panel::circle(out, r.x + r.w / 2, r.y + r.h / 2, r.w / 2, kTile, 1.0f);
        if (d.portrait && d.portrait->width > 0) put(out, *d.portrait, r, 1.0f);
    }
    panel::roundRect(out, panel::Box{0, L.footer.y, L.glass.w, 1 * s}, 0, kWhite, 0.08f);
    if (sh.pad) {
        const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
        Texts none;
        const std::vector<PadItem> items = padItems(d, sh, none);
        for (size_t i = 0; i < items.size() && i < L.padGlyphs.size(); ++i)
            padglyph::paint(out, items[i].button, f, L.padGlyphs[i].x, L.padGlyphs[i].y, kGlyph * s);
    }
}

std::vector<Words> words(const Layout& L, const Data& d, const Show& sh, const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s, side = kSide * s;
    const float titleW = m(t.title, 15 * s, true) + 2;
    out.push_back(Words{panel::Box{side, 0, titleW, L.title.h}, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{panel::Box{side + titleW + 8 * s, 0, m(t.hotkey, 11 * s, false) + 2, L.title.h}, t.hotkey,
                        kFaint, 11 * s, false, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});
    for (int i = 0; i < 10; ++i) {
        const panel::Box& b = L.slots[i];
        const bool l = left(i);
        const float x0 = l ? b.x + 6 * s : L.icons[i].x + L.icons[i].w + 10 * s;
        const float x1 = l ? L.icons[i].x - 10 * s : b.x + b.w - 6 * s;
        const Align a = l ? Align::Right : Align::Left;
        out.push_back(Words{panel::Box{x0, b.y + 5 * s, x1 - x0, 16 * s}, t.labels[i], kFaint, 11.5f * s, false, a});
        const Worn& wi = d.slots[i];
        const std::string text = wi.index > 0 ? named(wi) : t.empty;
        out.push_back(Words{panel::Box{x0, b.y + 20 * s, x1 - x0, 20 * s}, text,
                            wi.index > 0 ? (wi.spill ? kFaint : kText) : kFaint, 13 * s, wi.index > 0, a});
    }
    {  // the ammo under the character
        const panel::Box& b = L.slots[Ammo];
        const Worn& wi = d.slots[Ammo];
        const float x0 = L.icons[Ammo].x + L.icons[Ammo].w + 8 * s, x1 = b.x + b.w - 4 * s;
        out.push_back(Words{panel::Box{x0, b.y + 4 * s, x1 - x0, 16 * s}, t.labels[Ammo], kFaint, 11.5f * s, false,
                            Align::Left});
        const std::string text = wi.index > 0 ? named(wi) + (wi.count > 0 ? " \xC3\x97" + std::to_string(wi.count) : "")
                                              : t.empty;
        out.push_back(Words{panel::Box{x0, b.y + 20 * s, x1 - x0, 20 * s}, text, wi.index > 0 ? kText : kFaint,
                            12.5f * s, wi.index > 0, Align::Left});
    }
    if (sh.picking) {
        const std::string title = fill(t.pickTitle, {sh.pickSlot >= 0 && sh.pickSlot < kSlots ? t.labels[sh.pickSlot]
                                                                                              : std::string()});
        out.push_back(Words{panel::Box{L.pickList.x + 10 * s, L.pickList.y + 4 * s, L.pickList.w - 20 * s, 24 * s},
                            title, kText, 12.5f * s, true, Align::Left});
        if (d.picks.empty())
            out.push_back(Words{panel::Box{L.pickList.x + 10 * s, L.pickList.y + 34 * s, L.pickList.w - 20 * s, 20 * s},
                                t.pickNone, kFaint, 12 * s, false, Align::Left});
        for (size_t i = 0; i < L.picks.size(); ++i) {
            const Pick& p = d.picks[i];
            const std::string text = p.refine > 0 ? "+" + std::to_string(p.refine) + " " + p.name : p.name;
            out.push_back(Words{panel::Box{L.picks[i].x + 36 * s, L.picks[i].y, L.picks[i].w - 40 * s, L.picks[i].h},
                                text, kText, 12.5f * s, false, Align::Left});
        }
    } else {
        const float cx = L.centre.x, cw = L.centre.w;
        float y = L.portrait.y + L.portrait.h + 6 * s;
        out.push_back(Words{panel::Box{cx, y, cw, 20 * s}, d.who, kText, 14 * s, true, Align::Center});
        y += 20 * s;
        if (!d.job.empty())
            out.push_back(Words{panel::Box{cx, y, cw, 16 * s}, d.job, kBlue, 12 * s, false, Align::Center});
        y += 16 * s;
        if (d.baseLevel > 0)
            out.push_back(Words{panel::Box{cx, y, cw, 16 * s},
                                fill(t.level, {std::to_string(d.baseLevel), std::to_string(d.jobLevel)}), kMuted,
                                11.5f * s, false, Align::Center});
        y += 22 * s;
        for (size_t i = 0; i < d.stats.size() && y + 16 * s <= L.slots[Ammo].y; ++i) {  // two to a line
            const float half = (cw - 16 * s) / 2, sx = cx + 8 * s + float(i % 2) * half;
            out.push_back(Words{panel::Box{sx, y, half * 0.42f, 16 * s}, d.stats[i].first, kFaint, 11 * s, false,
                                Align::Left});
            out.push_back(Words{panel::Box{sx + half * 0.42f, y, half * 0.58f - 4 * s, 16 * s}, d.stats[i].second,
                                kText, 11.5f * s, true, Align::Right});
            if (i % 2) y += 16 * s;
        }
    }
    out.push_back(Words{panel::Box{side, L.footer.y, m(t.cards, 11.5f * s, false) + 4, L.footer.h}, t.cards, kFaint,
                        11.5f * s, false, Align::Left});
    if (!sh.pad) {
        const float dw = m(t.drag, 11.5f * s, false) + 2;
        out.push_back(Words{panel::Box{L.footer.w - side - dw, L.footer.y, dw, L.footer.h}, t.drag, kFaint, 11.5f * s,
                            false, Align::Right});
        return out;
    }
    const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    const std::vector<PadItem> items = padItems(d, sh, t);
    for (size_t i = 0; i < items.size() && i < L.padGlyphs.size(); ++i) {
        const padglyph::Letter l = padglyph::letter(items[i].button, f, L.padGlyphs[i].x, L.padGlyphs[i].y,
                                                    kGlyph * s);
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        out.push_back(Words{L.padWords[i], *items[i].words, kText, 12 * s, false, Align::Left});
    }
    return out;
}

}  // namespace uagfx::equipwin
