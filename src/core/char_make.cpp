#include "core/char_make.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::charmake {
namespace {

// At scale 1: the card 620 wide, 22 in; the face 128; a row 34; the buttons 42 high
constexpr float kW = 620, kPad = 22, kTitleH = 40, kFace = 128, kRowH = 34, kButtonH = 42, kLeftW = 200;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kBlue = 0x3D7EEB, kBlack = 0x000000;

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

}  // namespace

int faceSide(float scale) { return int(std::lround(kFace * (scale > 0 ? scale : 1.0f))); }

int pointButton(int pair, bool toLeft) {
    switch (pair) {
        case 0: return toLeft ? 0x8b : 0x8e;
        case 1: return toLeft ? 0x8c : 0x90;
        case 2: return toLeft ? 0x8d : 0x8f;
        default: return 0;
    }
}

bool canMove(const Fields& f, int pair, bool toLeft) {
    if (pair < 0 || pair > 2) return false;
    const int giver = toLeft ? kPairs[pair].right : kPairs[pair].left;
    return f.stats[giver] >= 2;
}

int hairButton(bool after) { return after ? 0xa0 : 0xa1; }

int colourPresses(bool after) { return after ? 1 : 8; }

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float pad = kPad * s, w = kW * s, row = kRowH * s, arrow = 28 * s;
    L.title = panel::Box{pad, pad, w - 2 * pad, kTitleH * s};
    const float top = pad + kTitleH * s + 10 * s, lw = kLeftW * s;
    // the left column: the face, the hair's style and colour under it
    L.face = panel::Box{pad + (lw - kFace * s) / 2, top, kFace * s, kFace * s};
    float y = top + kFace * s + 14 * s;
    const auto chooser = [&](panel::Box& label, panel::Box& before, panel::Box& value, panel::Box& after) {
        label = panel::Box{pad, y, lw, 18 * s};
        y += 20 * s;
        before = panel::Box{pad, y + (row - arrow) / 2, arrow, arrow};
        value = panel::Box{pad + arrow, y, lw - 2 * arrow, row};
        after = panel::Box{pad + lw - arrow, y + (row - arrow) / 2, arrow, arrow};
        y += row + 8 * s;
    };
    chooser(L.hairLabel, L.hairBefore, L.hairValue, L.hairAfter);
    chooser(L.colourLabel, L.colourBefore, L.colourValue, L.colourAfter);
    const float leftBottom = y;
    // the right column: the name, the three pairs
    const float rx = pad + lw + 24 * s, rw = w - pad - rx;
    L.nameLabel = panel::Box{rx, top, rw, 18 * s};
    L.name = panel::Box{rx, top + 22 * s, rw, 36 * s};
    float py = top + 22 * s + 36 * s + 22 * s;
    for (int i = 0; i < 3; ++i) {
        // + STR 5 [gold | blue] 5 INT + : each stat's own plus beside it (the point comes from its pair)
        L.pairs[i] = panel::Box{rx, py, rw, row};
        L.pairLeft[i] = panel::Box{rx, py + (row - arrow) / 2, arrow, arrow};
        L.leftWords[i] = panel::Box{rx + arrow + 8 * s, py, 60 * s, row};
        L.pairRight[i] = panel::Box{rx + rw - arrow, py + (row - arrow) / 2, arrow, arrow};
        L.rightWords[i] = panel::Box{L.pairRight[i].x - 8 * s - 60 * s, py, 60 * s, row};
        const float bx = L.leftWords[i].x + 60 * s + 6 * s, bw = L.rightWords[i].x - 6 * s - bx;
        L.pairBar[i] = panel::Box{bx, py + row / 2 - 4 * s, bw, 8 * s};
        py += row + 10 * s;
    }
    L.pairsHint = panel::Box{rx, py, rw, 18 * s};
    py += 18 * s;
    const float by = std::max(leftBottom, py) + 14 * s, bw = (w - 2 * pad - 14 * s) / 2;
    L.make = panel::Box{pad, by, bw, kButtonH * s};
    L.cancel = panel::Box{pad + bw + 14 * s, by, bw, kButtonH * s};
    const float h = by + kButtonH * s + pad;
    L.glass = panel::Box{0, 0, w, h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.name.contains(x, y)) return Hit{Part::Name, -1};
    if (L.hairBefore.contains(x, y)) return Hit{Part::HairBefore, -1};
    if (L.hairAfter.contains(x, y)) return Hit{Part::HairAfter, -1};
    if (L.colourBefore.contains(x, y)) return Hit{Part::ColourBefore, -1};
    if (L.colourAfter.contains(x, y)) return Hit{Part::ColourAfter, -1};
    for (int i = 0; i < 3; ++i) {
        if (L.pairLeft[i].contains(x, y)) return Hit{Part::PairLeft, i};
        if (L.pairRight[i].contains(x, y)) return Hit{Part::PairRight, i};
    }
    if (L.make.contains(x, y)) return Hit{Part::Make, -1};
    if (L.cancel.contains(x, y)) return Hit{Part::Cancel, -1};
    return Hit{};
}

void paint(panel::Picture& out, const Layout& L, const Fields& f, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 14 * s, kCard, std::max(0.94f, uistyle::window()), kWhite, 0.12f, 1.0f * s);
    panel::roundRect(out, L.face, L.face.w / 2, kBlack, 0.30f, kWhite, 0.12f, 1.0f * s);
    if (f.face && f.face->width > 0) put(out, *f.face, L.face);
    const auto arrow = [&](const panel::Box& b, Part p, int index, bool can) {
        const bool hot = sh.hover == p && sh.hoverIndex == index;
        panel::roundRect(out, b, 7 * s, kWhite, !can ? 0.02f : hot ? 0.14f : 0.06f);
    };
    const auto focused = [&](const panel::Box& b, int row) {
        if (sh.focusRow == row)
            panel::roundRect(out, panel::Box{b.x - 4 * s, b.y - 2 * s, b.w + 8 * s, b.h + 4 * s}, 9 * s, kGold, 0.0f,
                             kGold, 0.8f, 1.5f * s);
    };
    arrow(L.hairBefore, Part::HairBefore, -1, true);
    arrow(L.hairAfter, Part::HairAfter, -1, true);
    arrow(L.colourBefore, Part::ColourBefore, -1, true);
    arrow(L.colourAfter, Part::ColourAfter, -1, true);
    focused(panel::Box{L.hairBefore.x, L.hairValue.y, L.hairAfter.x + L.hairAfter.w - L.hairBefore.x, L.hairValue.h},
            0);
    focused(panel::Box{L.colourBefore.x, L.colourValue.y,
                       L.colourAfter.x + L.colourAfter.w - L.colourBefore.x, L.colourValue.h},
            1);
    panel::roundRect(out, L.name, 9 * s, kBlack, 0.35f, f.focus ? kGold : kWhite, f.focus ? 0.85f : 0.16f, 1.0f * s);
    for (int i = 0; i < 3; ++i) {
        arrow(L.pairLeft[i], Part::PairLeft, i, canMove(f, i, true));
        arrow(L.pairRight[i], Part::PairRight, i, canMove(f, i, false));
        const panel::Box& bar = L.pairBar[i];
        const int left = f.stats[kPairs[i].left], right = f.stats[kPairs[i].right];
        const float share = left + right > 0 ? float(left) / float(left + right) : 0.5f;
        panel::roundRect(out, bar, bar.h / 2, kBlue, 0.75f);
        panel::roundRect(out, panel::Box{bar.x, bar.y, bar.w * share, bar.h}, bar.h / 2, kGold, 0.9f);
        focused(L.pairs[i], 2 + i);
    }
    const bool hotMake = sh.hover == Part::Make, hotCancel = sh.hover == Part::Cancel;
    panel::roundRect(out, L.make, 9 * s, kGold, hotMake ? 0.36f : 0.24f, kGold, 0.85f, 1.0f * s);
    panel::roundRect(out, L.cancel, 9 * s, kWhite, hotCancel ? 0.12f : 0.06f, kWhite, 0.16f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const Fields& f, const Show& sh, const Texts& t) {
    (void)sh;
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 18 * s, true, Align::Left});
    out.push_back(Words{L.hairLabel, t.hair, kMuted, 12 * s, false, Align::Center});
    out.push_back(Words{L.hairValue, std::to_string(f.hair), kText, 15 * s, true, Align::Center});
    out.push_back(Words{L.colourLabel, t.colour, kMuted, 12 * s, false, Align::Center});
    out.push_back(Words{L.colourValue, std::to_string(f.colour), kText, 15 * s, true, Align::Center});
    for (const panel::Box* b : {&L.hairBefore, &L.colourBefore})
        out.push_back(Words{*b, "\xE2\x80\xB9", kText, 16 * s, true, Align::Center});  // ‹
    for (const panel::Box* b : {&L.hairAfter, &L.colourAfter})
        out.push_back(Words{*b, "\xE2\x80\xBA", kText, 16 * s, true, Align::Center});  // ›
    out.push_back(Words{L.nameLabel, t.name, kMuted, 12 * s, false, Align::Left});
    const panel::Box field{L.name.x + 12 * s, L.name.y, L.name.w - 24 * s, L.name.h};
    if (f.name.empty() && !f.focus)
        out.push_back(Words{field, t.nameHint, kMuted, 14 * s, false, Align::Left});
    else
        out.push_back(Words{field, f.name + (f.focus && f.caret ? "|" : ""), kText, 15 * s, true, Align::Left});
    for (int i = 0; i < 3; ++i) {
        const int a = kPairs[i].left, b = kPairs[i].right;
        out.push_back(Words{L.leftWords[i], t.stats[a] + " " + std::to_string(f.stats[a]), kGold2, 13 * s, true,
                            Align::Left});
        out.push_back(Words{L.rightWords[i], std::to_string(f.stats[b]) + " " + t.stats[b], kText, 13 * s, true,
                            Align::Right});
        out.push_back(Words{L.pairLeft[i], "+", canMove(f, i, true) ? kText : kMuted, 16 * s, true, Align::Center});
        out.push_back(Words{L.pairRight[i], "+", canMove(f, i, false) ? kText : kMuted, 16 * s, true,
                            Align::Center});
    }
    out.push_back(Words{L.pairsHint, t.pairsHint, kMuted, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.make, t.make, kText, 14 * s, true, Align::Center});
    out.push_back(Words{L.cancel, t.cancel, kText, 14 * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::charmake
