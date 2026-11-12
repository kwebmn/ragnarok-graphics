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

int keyRow(int row, bool down) {  // 3.0.18-beta.228
    if (row < -1 || row >= kRows) row = -1;
    if (down) return row >= kRows - 1 ? -1 : row + 1;
    return row <= -1 ? kRows - 1 : row - 1;
}

bool keySides(int row) { return row >= 0 && row < kRows; }

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

}  // namespace uagfx::charmake
