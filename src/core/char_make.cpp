#include "core/char_make.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::charmake {

Move cornerMove(int stat) {
    for (int i = 0; i < 3; ++i) {
        if (kPairs[i].left == stat) return {i, true};
        if (kPairs[i].right == stat) return {i, false};
    }
    return {};
}

int hexRadius(int side) { return int(float(side) * 0.31f); }

namespace {
struct P2 {
    float x, y;
};
float segmentDistance(P2 p, P2 a, P2 b) {
    const float dx = b.x - a.x, dy = b.y - a.y, l = dx * dx + dy * dy;
    float t = l > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l : 0;
    t = std::clamp(t, 0.0f, 1.0f);
    const float ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
    return std::sqrt(ex * ex + ey * ey);
}
bool inside(const std::vector<P2>& poly, float x, float y) {  // even-odd
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        if ((poly[i].y > y) != (poly[j].y > y) &&
            x < (poly[j].x - poly[i].x) * (y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            in = !in;
    return in;
}
// `top` (straight ARGB, its alpha scaled by `cover`) over the premultiplied pixel
void over(uint32_t& px, uint32_t argb, float cover) {
    const float a = float(argb >> 24) / 255.0f * std::clamp(cover, 0.0f, 1.0f);
    if (a <= 0) return;
    float c[4] = {float(px >> 24), float((px >> 16) & 255), float((px >> 8) & 255), float(px & 255)};
    const float s[4] = {255.0f, float((argb >> 16) & 255), float((argb >> 8) & 255), float(argb & 255)};
    for (int i = 0; i < 4; ++i) c[i] = s[i] * a + c[i] * (1 - a);
    px = uint32_t(std::lround(c[0])) << 24 | uint32_t(std::lround(c[1])) << 16 | uint32_t(std::lround(c[2])) << 8 |
         uint32_t(std::lround(c[3]));
}
}  // namespace

panel::Picture hexPicture(const int stats[6], int side) {
    panel::Picture p;
    if (side <= 0) return p;
    p.width = p.height = side;
    p.px.assign(size_t(side) * size_t(side), 0);
    const float c = float(side) / 2, r = float(hexRadius(side)), line = std::max(1.0f, float(side) / 300.0f);
    const auto at = [&](int k, float rr) {
        const float a = -3.14159265f / 2 + float(k) * 3.14159265f / 3;
        return P2{c + rr * std::cos(a), c + rr * std::sin(a)};
    };
    std::vector<P2> outer, shape;
    for (int k = 0; k < 6; ++k) {
        outer.push_back(at(k, r));
        shape.push_back(at(k, r * float(std::clamp(stats[kHexOrder[k]], 0, 9)) / 9.0f));
    }
    std::vector<std::pair<P2, P2>> grid;
    for (const float f : {1.0f, 2.0f / 3, 1.0f / 3})
        for (int k = 0; k < 6; ++k) grid.push_back({at(k, r * f), at(k + 1, r * f)});
    for (int k = 0; k < 3; ++k) grid.push_back({at(k, r), at(k + 3, r)});
    constexpr int kSub = 4;
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            uint32_t& px = p.px[size_t(y) * size_t(side) + size_t(x)];
            int inOuter = 0, inShape = 0;
            for (int sy = 0; sy < kSub; ++sy)
                for (int sx = 0; sx < kSub; ++sx) {
                    const float fx = float(x) + (float(sx) + 0.5f) / kSub, fy = float(y) + (float(sy) + 0.5f) / kSub;
                    inOuter += inside(outer, fx, fy);
                    inShape += inside(shape, fx, fy);
                }
            const P2 m{float(x) + 0.5f, float(y) + 0.5f};
            over(px, 0xE00C1019, float(inOuter) / (kSub * kSub));
            float g = 1e9f;
            for (const auto& [a, b] : grid) g = std::min(g, segmentDistance(m, a, b));
            over(px, 0xFF2E3650, line - g + 0.5f);
            over(px, 0x8CD8B46A, float(inShape) / (kSub * kSub));
            float e = 1e9f;
            for (size_t i = 0; i < shape.size(); ++i) e = std::min(e, segmentDistance(m, shape[i], shape[(i + 1) % 6]));
            over(px, 0xFFD8B46A, line * 1.5f - e + 0.5f);
            float d = 1e9f;
            for (const P2& q : shape) d = std::min(d, std::hypot(m.x - q.x, m.y - q.y));
            over(px, 0xFFF1D9A0, line * 3.0f - d + 0.5f);
        }
    return p;
}
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
