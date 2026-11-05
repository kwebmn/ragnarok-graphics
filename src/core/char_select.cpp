#include "core/char_select.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::charselect {
namespace {

// At scale 1: a place 230 x 330, 14 apart; the card 22 in; its title 40; the face 96; the buttons 42 high
constexpr float kSlotW = 230, kSlotH = 330, kGap = 14, kPad = 22, kTitleH = 40, kFace = 96, kButtonH = 42;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kHp = 0x4CC777, kSp = 0x3D7EEB, kBlack = 0x000000;

void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

// A place's inner parts (its own pixels): the face's box, its name's row, ...
struct Inner {
    panel::Box face, name, job, level, hp, hpBar, sp, spBar;
    panel::Box stats[6];
};
Inner inner(const panel::Box& b, float s) {
    Inner i;
    const float pad = 14 * s, w = b.w - 2 * pad;
    float y = b.y + pad;
    i.face = panel::Box{b.x + (b.w - kFace * s) / 2, y, kFace * s, kFace * s};
    y += kFace * s + 8 * s;
    i.name = panel::Box{b.x + pad, y, w, 20 * s};
    y += 20 * s;
    i.job = panel::Box{b.x + pad, y, w, 18 * s};
    y += 18 * s;
    i.level = panel::Box{b.x + pad, y, w, 18 * s};
    y += 24 * s;
    i.hp = panel::Box{b.x + pad, y, w, 16 * s};
    y += 16 * s;
    i.hpBar = panel::Box{b.x + pad, y, w, 5 * s};
    y += 9 * s;
    i.sp = panel::Box{b.x + pad, y, w, 16 * s};
    y += 16 * s;
    i.spBar = panel::Box{b.x + pad, y, w, 5 * s};
    y += 12 * s;
    const float cw = w / 3;
    for (int k = 0; k < 6; ++k)
        i.stats[k] = panel::Box{b.x + pad + float(k % 3) * cw, y + float(k / 3) * 18 * s, cw, 18 * s};
    return i;
}

}  // namespace

int faceSide(float scale) { return int(std::lround(kFace * (scale > 0 ? scale : 1.0f))); }

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float pad = kPad * s, sw = kSlotW * s, gap = kGap * s;
    const float w = 2 * pad + 3 * sw + 2 * gap;
    L.title = panel::Box{pad, pad, w - 2 * pad - 140 * s, kTitleH * s};
    L.pageLeft = panel::Box{w - pad - 120 * s, pad + 6 * s, 28 * s, 28 * s};
    L.page = panel::Box{w - pad - 92 * s, pad + 6 * s, 64 * s, 28 * s};
    L.pageRight = panel::Box{w - pad - 28 * s, pad + 6 * s, 28 * s, 28 * s};
    const float top = pad + kTitleH * s + 8 * s;
    for (int i = 0; i < 3; ++i) L.slots.push_back(panel::Box{pad + float(i) * (sw + gap), top, sw, kSlotH * s});
    L.summary = panel::Box{pad, top + kSlotH * s + 6 * s, w - 2 * pad, 22 * s};  // 3.0.11-beta.274
    const float by = top + kSlotH * s + 34 * s, bw = (w - 2 * pad - 3 * gap) / 4;
    for (int i = 0; i < 4; ++i) L.buttons.push_back(panel::Box{pad + float(i) * (bw + gap), by, bw, kButtonH * s});
    const float h = by + kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.slots.size(); ++i)
        if (L.slots[i].contains(x, y)) return Hit{Part::Slot, int(i)};
    for (size_t i = 0; i < L.buttons.size(); ++i)
        if (L.buttons[i].contains(x, y)) return Hit{Part::Button, int(i)};
    if (L.pageLeft.contains(x, y)) return Hit{Part::PageLeft, -1};
    if (L.pageRight.contains(x, y)) return Hit{Part::PageRight, -1};
    return Hit{};
}

int gameButton(Button b) {
    switch (b) {
        case Button::Enter: return 0x76;
        case Button::Make: return 0x89;
        case Button::Remove: return 0x91;
        case Button::Back: break;
    }
    return 0x77;
}

int pageButton(bool right) { return right ? 0x96 : 0x97; }

bool allowed(const Slot& s, Button b) {
    switch (b) {
        case Button::Enter:
        case Button::Remove: return s.used;
        case Button::Make: return !s.used;
        case Button::Back: break;
    }
    return true;
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Slot>& slots, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 14 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    for (const panel::Box* b : {&L.pageLeft, &L.pageRight}) panel::roundRect(out, *b, 7 * s, kWhite, 0.06f);
    for (size_t i = 0; i < L.slots.size(); ++i) {
        const panel::Box& b = L.slots[i];
        const bool chosen = int(i) == sh.selected, hot = int(i) == sh.hover;
        const Slot* sl = i < slots.size() ? &slots[i] : nullptr;
        panel::roundRect(out, b, 12 * s, kWhite, hot ? 0.07f : 0.04f, chosen ? kGold : kWhite,
                         chosen ? 1.0f : hot ? 0.24f : 0.12f, (chosen ? 2.0f : 1.0f) * s);
        if (!sl || !sl->used) continue;
        const Inner in = inner(b, s);
        panel::roundRect(out, in.face, in.face.w / 2, kBlack, 0.30f);
        if (sl->face && sl->face->width > 0) put(out, *sl->face, in.face);
        const auto bar = [&](const panel::Box& bb, int now, int most, uint32_t rgb) {
            panel::roundRect(out, bb, bb.h / 2, kWhite, 0.08f);
            if (most > 0 && now > 0)
                panel::roundRect(out, panel::Box{bb.x, bb.y, bb.w * std::min(1.0f, float(now) / float(most)), bb.h},
                                 bb.h / 2, rgb, 0.9f);
        };
        bar(in.hpBar, sl->hp, sl->maxHp, kHp);
        bar(in.spBar, sl->sp, sl->maxSp, kSp);
    }
    const Slot* chosen = sh.selected >= 0 && sh.selected < int(slots.size()) ? &slots[size_t(sh.selected)] : nullptr;
    for (size_t i = 0; i < L.buttons.size(); ++i) {
        const Button b = Button(i);
        const bool can = chosen ? allowed(*chosen, b) : b == Button::Back;
        const bool primary = b == Button::Enter || (b == Button::Make && chosen && !chosen->used);
        const bool hot = int(i) == sh.hoverButton;
        panel::roundRect(out, L.buttons[i], 9 * s, primary ? kGold : kWhite,
                         !can ? 0.02f : primary ? (hot ? 0.36f : 0.24f) : (hot ? 0.12f : 0.06f),
                         primary && can ? kGold : kWhite, primary && can ? 0.85f : 0.16f, 1.0f * s);
    }
}

std::vector<Words> words(const Layout& L, const std::vector<Slot>& slots, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 18 * s, true, Align::Left});
    out.push_back(Words{L.page, std::to_string(sh.page + 1) + " / " + std::to_string(std::max(1, sh.pages)), kMuted,
                        12.5f * s, false, Align::Center});
    out.push_back(Words{L.pageLeft, "\xE2\x80\xB9", kText, 16 * s, true, Align::Center});   // ‹
    out.push_back(Words{L.pageRight, "\xE2\x80\xBA", kText, 16 * s, true, Align::Center});  // ›
    for (size_t i = 0; i < L.slots.size(); ++i) {
        const panel::Box& b = L.slots[i];
        const Slot* sl = i < slots.size() ? &slots[i] : nullptr;
        if (!sl || !sl->used) {
            out.push_back(Words{panel::Box{b.x, b.y + b.h / 2 - 22 * s, b.w, 20 * s}, t.empty, kMuted, 14 * s, true,
                                Align::Center});
            out.push_back(Words{panel::Box{b.x + 12 * s, b.y + b.h / 2, b.w - 24 * s, 18 * s}, t.makeHint, kMuted,
                                11.5f * s, false, Align::Center});
            continue;
        }
        const Inner in = inner(b, s);
        out.push_back(Words{in.name, sl->name, kText, 15 * s, true, Align::Center});
        // 3.0.11-beta.273: the map it was last on after its job, when it was played after its levels
        out.push_back(Words{in.job, sl->map.empty() ? sl->job : sl->job + " \xC2\xB7 " + sl->map, kGold2, 12.5f * s,
                            false, Align::Center});
        out.push_back(Words{in.level,
                            "Lv " + std::to_string(sl->level) + " / " + std::to_string(sl->jobLevel) +
                                (sl->ago.empty() ? std::string() : " \xC2\xB7 " + sl->ago),
                            kMuted, 12 * s, false, Align::Center});
        out.push_back(Words{in.hp, "HP " + std::to_string(sl->hp) + " / " + std::to_string(sl->maxHp), kText,
                            11.5f * s, false, Align::Left});
        out.push_back(Words{in.sp, "SP " + std::to_string(sl->sp) + " / " + std::to_string(sl->maxSp), kText,
                            11.5f * s, false, Align::Left});
        for (int k = 0; k < 6; ++k)
            out.push_back(Words{in.stats[k], t.stats[k] + " " + std::to_string(sl->stats[k]), kMuted, 11.5f * s, false,
                                Align::Left});
    }
    const std::string* names[4] = {&t.enter, &t.make, &t.remove, &t.back};
    // 3.0.11-beta.274: the chosen character's last session over the buttons
    const int chosen = sh.selected;
    if (chosen >= 0 && chosen < int(slots.size()) && !slots[size_t(chosen)].summary.empty())
        out.push_back(Words{L.summary, slots[size_t(chosen)].summary, kMuted, 12 * s, false, Align::Center});
    for (size_t i = 0; i < L.buttons.size(); ++i)
        out.push_back(Words{L.buttons[i], *names[i], kText, 14 * s, i == 0, Align::Center});
    return out;
}

}  // namespace uagfx::charselect
