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
    L.wide = L.w;
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

Layout carouselLayout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.carousel = true;
    L.scales = {0.68f, 0.84f, 1.08f, 0.84f, 0.68f};
    const float pad = kPad * s, sw = kSlotW * s, step = sw * 0.78f;
    const float w = 2 * pad + 4 * step + sw * 1.08f;
    L.title = panel::Box{pad, pad, w - 2 * pad - 140 * s, kTitleH * s};
    L.page = panel::Box{w - pad - 92 * s, pad + 6 * s, 64 * s, 28 * s};
    const float top = pad + kTitleH * s + 8 * s, tallest = kSlotH * s * 1.08f, mid = w / 2;
    for (int i = 0; i < kPositions; ++i) {
        const float f = L.scales[size_t(i)], cw = sw * f, ch = kSlotH * s * f;
        const float cx = mid + float(i - kMiddle) * step;
        L.slots.push_back(panel::Box{cx - cw / 2, top + (tallest - ch) / 2, cw, ch});
    }
    L.pageLeft = panel::Box{pad / 2, top + tallest / 2 - 20 * s, 28 * s, 40 * s};
    L.pageRight = panel::Box{w - pad / 2 - 28 * s, top + tallest / 2 - 20 * s, 28 * s, 40 * s};
    L.summary = panel::Box{pad, top + tallest + 6 * s, w - 2 * pad, 22 * s};
    const float by = top + tallest + 34 * s, gap = kGap * s, bw = (w - 2 * pad - 3 * gap) / 4;
    for (int i = 0; i < 4; ++i) L.buttons.push_back(panel::Box{pad + float(i) * (bw + gap), by, bw, kButtonH * s});
    const float h = by + kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, h};
    L.w = int(std::ceil(w));
    L.wide = L.w;
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

int placeAt(int chosen, int position, int count) {
    if (count <= 0) return -1;
    const int offset = position - kMiddle;
    if (2 * std::abs(offset) >= count + (offset > 0 ? 0 : 1)) return -1;  // each place shown once
    return ((chosen + offset) % count + count) % count;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.carousel) {  // 3.0.11-beta.275: the nearer positions over the farther
        for (const int i : {kMiddle, 1, 3, 0, 4})
            if (i < int(L.slots.size()) && L.slots[size_t(i)].contains(x, y)) return Hit{Part::Slot, i};
    }
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

std::vector<Words> words(const Layout& L, const std::vector<Slot>& slots, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 18 * s, true, Align::Left});
    out.push_back(Words{L.page, std::to_string(sh.page + 1) + " / " + std::to_string(std::max(1, sh.pages)), kMuted,
                        12.5f * s, false, Align::Center});
    out.push_back(Words{L.pageLeft, "\xE2\x80\xB9", kText, 16 * s, true, Align::Center});   // ‹
    out.push_back(Words{L.pageRight, "\xE2\x80\xBA", kText, 16 * s, true, Align::Center});  // ›
    const std::vector<size_t> order =
        L.carousel ? std::vector<size_t>{0, 4, 1, 3, 2} : std::vector<size_t>{0, 1, 2};  // .275
    for (const size_t i : order) {
        if (i >= L.slots.size()) continue;
        const panel::Box& b = L.slots[i];
        const Slot* sl = i < slots.size() ? &slots[i] : nullptr;
        if (sl && sl->none) continue;
        const float s0 = s;
        const float s = s0 * (L.carousel ? L.scales[i] : 1.0f);  // .275: a carousel's card at its own size
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

CardParts cardParts(const panel::Box& card, float s) {
    const Inner in = inner(card, s);
    return CardParts{in.face, in.hpBar, in.spBar};
}

}  // namespace uagfx::charselect
