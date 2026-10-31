#include "core/trade_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

#include "core/ui_style.hpp"

namespace uagfx::tradewin {
namespace {

using itemwin::Align;
using itemwin::Words;

// Its sizes at scale 1: 520 wide; the title 46, a side's head 26, cells 40 five a row 8 apart (two rows), the zeny row
// 34, the warning 22, the buttons 34, the hint 24; the two sides 16 apart
constexpr float kW = 520, kTitleH = 46, kHeadH = 26, kCell = 40, kGap = 8, kZenyH = 34, kWarnH = 22, kButtonH = 34;
constexpr float kHintH = 24, kSide = 18, kSidesGap = 16;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kGold2 = 0xC9A55B;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A, kGreen = 0x5FD08A, kRed = 0xFF7D6E;

// A premultiplied picture centred in a box, its alpha times `k`
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

const Thing* thingAt(const Side& me, const Side& them, int cell) {
    if (cell < 0 || cell >= 2 * kSlots) return nullptr;
    const Thing& x = cell < kSlots ? me.slots[cell] : them.slots[cell - kSlots];
    return x.count > 0 ? &x : nullptr;
}

std::string tipFirst(const Thing& x) {
    std::string s = x.refine > 0 ? "+" + std::to_string(x.refine) + " " + x.name : x.name;
    if (x.count > 1) s += " \xC3\x97" + itemwin::grouped(x.count);
    return s;
}

// The pointed cell's tip: under its row (over it on the lower row), inside the glass
panel::Box tipBox(const Layout& L, const Thing& x, int cell, const Texts& t, const itemwin::Measure& m) {
    const float s = L.s;
    const float wide = std::max(m(tipFirst(x), 13 * s, true), m(t.info, 11.5f * s, false)) + 22 * s;
    const float h = 44 * s;
    const panel::Box& c = L.cells[cell];
    const float x0 = std::clamp(c.x + c.w / 2 - wide / 2, L.glass.x + 6 * s, L.glass.x + L.glass.w - wide - 6 * s);
    const bool upper = (cell % kSlots) < kColumns;
    const float y = upper ? c.y - h - 6 * s : c.y + c.h + 6 * s;
    return panel::Box{x0, std::max(L.glass.y + 2 * s, y), wide, h};
}

}  // namespace

bool Late::any() const { return zeny || std::any_of(std::begin(slots), std::end(slots), [](bool b) { return b; }); }

Late watchLate(const Side& before, const Side& now, bool meReady, Late was) {
    if (!meReady) return was;
    for (int i = 0; i < kSlots; ++i)
        if (now.slots[i].id != before.slots[i].id || now.slots[i].count != before.slots[i].count) was.slots[i] = true;
    if (now.zeny != before.zeny) was.zeny = true;
    return was;
}

bool canReady(const Side& me) { return !me.ready; }

bool canTrade(const Side& me, const Side& them, bool traded) { return me.ready && them.ready && !traded; }

Layout layout(int x, int y, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s, colW = (w - 2 * side - kSidesGap * s) / 2;
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 26 * s, (kTitleH * s - 26 * s) / 2, 26 * s, 26 * s};
    const float headY = kTitleH * s;
    L.mineHead = panel::Box{side, headY, colW, kHeadH * s};
    L.theirHead = panel::Box{side + colW + kSidesGap * s, headY, colW, kHeadH * s};
    const float cell = kCell * s, gap = kGap * s;
    const float gridW = float(kColumns) * cell + float(kColumns - 1) * gap;
    const float gridY = headY + kHeadH * s + 6 * s;
    for (int sideNo = 0; sideNo < 2; ++sideNo) {
        const panel::Box& head = sideNo == 0 ? L.mineHead : L.theirHead;
        const float gx = head.x + (head.w - gridW) / 2;
        for (int i = 0; i < kSlots; ++i)
            L.cells[sideNo * kSlots + i] = panel::Box{gx + float(i % kColumns) * (cell + gap),
                                                      gridY + float(i / kColumns) * (cell + gap), cell, cell};
    }
    const float zenyY = gridY + 2 * cell + gap + 10 * s;
    L.mineZeny = panel::Box{L.mineHead.x, zenyY, colW, kZenyH * s};
    L.theirZeny = panel::Box{L.theirHead.x, zenyY, colW, kZenyH * s};
    L.zenyField = panel::Box{L.mineZeny.x + 56 * s, zenyY + 3 * s, colW - 56 * s, kZenyH * s - 6 * s};
    L.warn = panel::Box{side, zenyY + kZenyH * s + 4 * s, w - 2 * side, kWarnH * s};
    const float buttonsY = L.warn.y + L.warn.h + 4 * s;
    const float bw = (w - 2 * side - 2 * 10 * s) / 3;
    for (int i = 0; i < 3; ++i)
        L.buttons[i] = panel::Box{side + float(i) * (bw + 10 * s), buttonsY, bw, kButtonH * s};
    L.hint = panel::Box{side, buttonsY + kButtonH * s + 4 * s, w - 2 * side, kHintH * s};
    L.glass = panel::Box{0, 0, w, L.hint.y + L.hint.h + 6 * s};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = x;
    L.y = y;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    for (int i = 0; i < 2 * kSlots; ++i)
        if (L.cells[i].contains(x, y)) return Hit{Part::Cell, i};
    if (L.zenyField.contains(x, y)) return Hit{Part::Zeny, -1};
    for (int i = 0; i < 3; ++i)
        if (L.buttons[i].contains(x, y)) return Hit{Part::Button, i};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const Side& me, const Side& them, const Show& sh, const Texts& t,
           const itemwin::Measure& m) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    const float divider = (L.mineHead.x + L.mineHead.w + L.theirHead.x) / 2;  // the sides apart
    panel::roundRect(out, panel::Box{divider, L.mineHead.y + 4 * s, 1 * s, L.mineZeny.y + L.mineZeny.h - L.mineHead.y},
                     0, kWhite, 0.08f);
    for (int sideNo = 0; sideNo < 2; ++sideNo) {  // a ready side's head: a green pill
        const Side& who = sideNo == 0 ? me : them;
        if (!who.ready) continue;
        const panel::Box& head = sideNo == 0 ? L.mineHead : L.theirHead;
        const float pw = m("\xE2\x9C\x93 " + t.ready, 11 * s, true) + 14 * s;
        panel::roundRect(out, panel::Box{head.x + head.w - pw, head.y + 4 * s, pw, head.h - 8 * s}, 8 * s, kGreen,
                         0.22f, kGreen, 0.7f, 1.0f * s);
    }
    if (sh.dropping && !me.ready) {  // a bag thing dragged: your side takes it
        const panel::Box& a = L.cells[0];
        const panel::Box& b = L.cells[kSlots - 1];
        panel::roundRect(out, panel::Box{a.x - 4 * s, a.y - 4 * s, b.x + b.w - a.x + 8 * s, b.y + b.h - a.y + 8 * s},
                         8 * s, kGold2, 0.06f, kGold, 0.9f, 2.0f * s);
    }
    for (int i = 0; i < 2 * kSlots; ++i) {
        const panel::Box& b = L.cells[i];
        const Thing* x = thingAt(me, them, i);
        const bool late = i >= kSlots && sh.late.slots[i - kSlots];
        const bool hot = i == sh.hover;
        panel::roundRect(out, b, 7 * s, kBlack, 0.35f, late ? kRed : hot ? kGold2 : kWhite,
                         late || hot ? 0.95f : 0.08f, (late || hot ? 1.5f : 1) * s);
        if (!x) continue;
        if (x->icon && x->icon->width > 0) put(out, *x->icon, b, x->identified ? 1.0f : 0.45f);
        if (x->count > 1) {
            const float w = m(itemwin::grouped(x->count), 10 * s, true) + 6 * s;
            panel::roundRect(out, panel::Box{b.x + b.w - w - 2 * s, b.y + b.h - 13 * s, w, 11 * s}, 3 * s, kBlack,
                             0.7f);
        }
    }
    panel::roundRect(out, L.zenyField, 7 * s, kBlack, 0.35f, sh.typing ? kGold2 : kWhite, sh.typing ? 0.6f : 0.10f,
                     1.0f * s);
    if (sh.typing && sh.caretOn) {
        const float x = L.zenyField.x + L.zenyField.w - 10 * s;
        panel::roundRect(out, panel::Box{x, L.zenyField.y + 7 * s, 1.5f * s, L.zenyField.h - 14 * s}, 0, kText, 0.95f);
    }
    if (sh.late.any())
        panel::roundRect(out, L.warn, 6 * s, kRed, 0.14f, kRed, 0.5f, 1.0f * s);
    const bool on[3] = {canReady(me), canTrade(me, them, sh.traded), true};
    for (int i = 0; i < 3; ++i) {
        const bool hot = i == sh.hoverButton && on[i];
        const bool main = (i == 0 && on[0]) || (i == 1 && on[1]);  // the next step in gold
        const float fill = main ? (hot ? 0.95f : 0.85f) : hot ? 0.16f : 0.08f;
        panel::roundRect(out, L.buttons[i], 8 * s, main ? kGold : kWhite, fill, kWhite, on[i] ? 0.14f : 0.05f,
                         1.0f * s);
    }
    if (sh.tip && sh.hover >= 0) {
        if (const Thing* x = thingAt(me, them, sh.hover)) {
            const panel::Box tip = tipBox(L, *x, sh.hover, t, m);
            panel::roundRect(out, tip, 8 * s, kCard, 0.96f, kWhite, 0.14f, 1.0f * s);
        }
    }
}

std::vector<Words> words(const Layout& L, const Side& me, const Side& them, const Show& sh, const Texts& t,
                         const itemwin::Measure& m) {
    std::vector<Words> out;
    const float s = L.s, side = kSide * s;
    const float titleW = m(t.title, 15 * s, true) + 2;
    out.push_back(Words{panel::Box{side, 0, titleW, L.title.h}, t.title, kText, 15 * s, true, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});  // ×
    const std::string ready = "\xE2\x9C\x93 " + t.ready;  // ✓ Готово
    for (int sideNo = 0; sideNo < 2; ++sideNo) {
        const Side& who = sideNo == 0 ? me : them;
        const panel::Box& head = sideNo == 0 ? L.mineHead : L.theirHead;
        const std::string& name = sideNo == 0 ? t.you : t.partner;
        out.push_back(Words{head, name, sideNo == 0 ? kMuted : kText, 13 * s, true, Align::Left});
        if (who.ready) {
            const float pw = m(ready, 11 * s, true) + 14 * s;
            out.push_back(Words{panel::Box{head.x + head.w - pw, head.y, pw, head.h}, ready, kGreen, 11 * s, true,
                                Align::Center});
        }
        const panel::Box& zeny = sideNo == 0 ? L.mineZeny : L.theirZeny;
        out.push_back(Words{panel::Box{zeny.x, zeny.y, 56 * s, zeny.h}, t.zeny, kMuted, 12 * s, false, Align::Left});
    }
    const panel::Box field{L.zenyField.x + 8 * s, L.zenyField.y, L.zenyField.w - 20 * s, L.zenyField.h};
    const std::string mine = !sh.typing        ? itemwin::grouped(me.zeny)
                             : sh.typed.empty() ? std::string()
                                                : itemwin::grouped(std::atoll(sh.typed.c_str()));
    if (!mine.empty()) out.push_back(Words{field, mine, kText, 12.5f * s, true, Align::Right});
    const panel::Box theirs{L.theirZeny.x + 56 * s, L.theirZeny.y, L.theirZeny.w - 64 * s, L.theirZeny.h};
    out.push_back(Words{theirs, itemwin::grouped(them.zeny), sh.late.zeny ? kRed : kGold, 12.5f * s, true,
                        Align::Right});
    panel::Box tip;  // the pointed cell's tip: the cells' words under it not drawn over it
    const Thing* pointed = sh.tip ? thingAt(me, them, sh.hover) : nullptr;
    if (pointed) tip = tipBox(L, *pointed, sh.hover, t, m);
    const auto under = [&](const panel::Box& b) {
        return tip.w > 0 && b.x < tip.x + tip.w && b.x + b.w > tip.x && b.y < tip.y + tip.h && b.y + b.h > tip.y;
    };
    for (int i = 0; i < 2 * kSlots; ++i) {
        const Thing* x = thingAt(me, them, i);
        const panel::Box& b = L.cells[i];
        if (!x || under(b)) continue;
        if (x->count > 1) {
            const std::string n = itemwin::grouped(x->count);
            const float w = m(n, 10 * s, true) + 6 * s;
            out.push_back(Words{panel::Box{b.x + b.w - w - 2 * s, b.y + b.h - 13 * s, w, 11 * s}, n, kText, 10 * s,
                                true, Align::Center});
        }
        if (x->refine > 0) {
            const std::string n = "+" + std::to_string(x->refine);
            out.push_back(Words{panel::Box{b.x + 2 * s, b.y + 2 * s, m(n, 10 * s, true) + 6 * s, 11 * s}, n, kGold,
                                10 * s, true, Align::Center});
        }
    }
    if (sh.late.any())
        out.push_back(Words{L.warn, "\xE2\x9A\xA0 " + t.late, kRed, 12 * s, true, Align::Center});  // ⚠
    const bool on[3] = {canReady(me), canTrade(me, them, sh.traded), true};
    const std::string labels[3] = {t.ready, t.trade, t.cancel};
    for (int i = 0; i < 3; ++i) {
        const bool main = (i == 0 && on[0]) || (i == 1 && on[1]);
        out.push_back(Words{L.buttons[i], labels[i], main ? kCard : on[i] ? kText : kFaint, 13 * s, true,
                            Align::Center});
    }
    if (!me.ready) out.push_back(Words{L.hint, t.hint, kFaint, 11.5f * s, false, Align::Center});
    if (pointed && tip.w > 0) {
        out.push_back(Words{panel::Box{tip.x + 11 * s, tip.y + 4 * s, tip.w - 22 * s, 18 * s}, tipFirst(*pointed),
                            kText, 13 * s, true, Align::Left});
        out.push_back(Words{panel::Box{tip.x + 11 * s, tip.y + 23 * s, tip.w - 22 * s, 16 * s}, t.info, kMuted,
                            11.5f * s, false, Align::Left});
    }
    return out;
}

}  // namespace uagfx::tradewin
