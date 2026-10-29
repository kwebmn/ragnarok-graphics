#include "core/minimap_proto.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/ui_icons.hpp"

namespace uagfx::minimap {
namespace {

// At scale 1, the target's pixels (3.0.11-beta.31: the concept's v19_town - 200 a side, the bars under it)
constexpr float kSide = 220, kMargin = 10, kGap = 6, kBarH = 30, kRouteH = 46, kLineH = 19, kListPad = 7;
constexpr float kButton = 22, kButtonGap = 4, kRadius = 10;
// The concept's style A: its glass, white over it, its gold, its words' colours
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kDark = 0x0B0E14;
constexpr uint32_t kAccent = 0xC9A55B, kAccent2 = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5;
constexpr uint32_t kGround = 0xFF141922;   // the map's square where it has no picture
constexpr uint32_t kKafra = 0xE86AA8;      // the concept's «K»
constexpr uint32_t kPortal = 0x23285A, kPortalRing = 0x8E9BFF;
constexpr uint32_t kArrow = 0x4FA3FF, kCone = 0x5AB4FF;

// a smooth line `width` wide and a ring - each pixel's coverage by its distance (inside `clip`)
void segment(panel::Picture& p, const panel::Box& clip, float x0, float y0, float x1, float y1, float width,
             uint32_t rgb, float a) {
    const float half = width / 2, dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    const int left = std::max(int(clip.x), int(std::floor(std::min(x0, x1) - half - 1)));
    const int right = std::min(int(clip.x + clip.w) - 1, int(std::ceil(std::max(x0, x1) + half + 1)));
    const int top = std::max(int(clip.y), int(std::floor(std::min(y0, y1) - half - 1)));
    const int bottom = std::min(int(clip.y + clip.h) - 1, int(std::ceil(std::max(y0, y1) + half + 1)));
    for (int y = std::max(0, top); y <= std::min(p.height - 1, bottom); ++y)
        for (int x = std::max(0, left); x <= std::min(p.width - 1, right); ++x) {
            const float px = float(x) + 0.5f - x0, py = float(y) + 0.5f - y0;
            const float t = len2 > 0 ? std::clamp((px * dx + py * dy) / len2, 0.0f, 1.0f) : 0.0f;
            const float ex = px - t * dx, ey = py - t * dy;
            const float cover = std::clamp(half + 0.5f - std::sqrt(ex * ex + ey * ey), 0.0f, 1.0f);
            if (cover > 0) panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], rgb, cover * a);
        }
}

void ring(panel::Picture& p, float cx, float cy, float r, float width, uint32_t rgb, float a) {
    const float reach = r + width;
    for (int y = std::max(0, int(cy - reach - 1)); y <= std::min(p.height - 1, int(cy + reach + 1)); ++y)
        for (int x = std::max(0, int(cx - reach - 1)); x <= std::min(p.width - 1, int(cx + reach + 1)); ++x) {
            const float d = std::hypot(float(x) + 0.5f - cx, float(y) + 0.5f - cy);
            const float cover = std::clamp(width / 2 + 0.5f - std::fabs(d - r), 0.0f, 1.0f);
            if (cover > 0) panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], rgb, cover * a);
        }
}

// A triangle's coverage of a pixel's middle: the least of its distances inside its three sides, half a pixel round
// (either way round)
float triangleCover(const float t[6], float px, float py) {
    const float turn = (t[2] - t[0]) * (t[5] - t[1]) - (t[3] - t[1]) * (t[4] - t[0]);
    if (turn == 0) return 0;
    const float sign = turn > 0 ? 1.0f : -1.0f;
    float least = 1e9f;
    for (int i = 0; i < 3; ++i) {
        const float ax = t[2 * i], ay = t[2 * i + 1], bx = t[(2 * i + 2) % 6], by = t[(2 * i + 3) % 6];
        const float ex = bx - ax, ey = by - ay, len = std::sqrt(ex * ex + ey * ey);
        if (len <= 0) return 0;
        least = std::min(least, sign * (ex * (py - ay) - ey * (px - ax)) / len);
    }
    return std::clamp(least + 0.5f, 0.0f, 1.0f);
}

// Triangles filled as one shape: each pixel the most any of them covers it
void triangles(panel::Picture& p, const std::vector<std::array<float, 6>>& tris, uint32_t rgb, float a) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const auto& t : tris)
        for (int i = 0; i < 3; ++i) {
            x0 = std::min(x0, t[size_t(2 * i)]);
            x1 = std::max(x1, t[size_t(2 * i)]);
            y0 = std::min(y0, t[size_t(2 * i + 1)]);
            y1 = std::max(y1, t[size_t(2 * i + 1)]);
        }
    for (int y = std::max(0, int(y0) - 1); y <= std::min(p.height - 1, int(y1) + 1); ++y)
        for (int x = std::max(0, int(x0) - 1); x <= std::min(p.width - 1, int(x1) + 1); ++x) {
            float cover = 0;
            for (const auto& t : tris)
                cover = std::max(cover, triangleCover(t.data(), float(x) + 0.5f, float(y) + 0.5f));
            if (cover > 0) panel::over(p.px[size_t(y) * size_t(p.width) + size_t(x)], rgb, cover * a);
        }
}

// The player: where the camera looks, a soft fan of its view and the arrow; not known - a white dot
void player(panel::Picture& p, const panel::Box& clip, float x, float y, float lookX, float lookY, float size) {
    const float len = std::hypot(lookX, lookY);
    if (len < 1e-4f) {
        panel::circle(p, x, y, size * 0.32f + 1.5f, kBlack, 0.85f);
        panel::circle(p, x, y, size * 0.32f, kWhite, 1.0f);
        return;
    }
    const float dx = lookX / len, dy = -lookY / len;  // north up
    const float reach = size * 3.6f, half = 0.66f;      // the view: 38 degrees each way
    for (int py = std::max(int(clip.y), int(y - reach)); py <= std::min(int(clip.y + clip.h) - 1, int(y + reach));
         ++py)
        for (int px = std::max(int(clip.x), int(x - reach)); px <= std::min(int(clip.x + clip.w) - 1, int(x + reach));
             ++px) {
            if (px < 0 || py < 0 || px >= p.width || py >= p.height) continue;
            const float vx = float(px) + 0.5f - x, vy = float(py) + 0.5f - y, d = std::hypot(vx, vy);
            if (d > reach || d < 1) continue;
            const float angle = std::acos(std::clamp((vx * dx + vy * dy) / d, -1.0f, 1.0f));
            const float edge = std::clamp((half - angle) * reach * 0.5f, 0.0f, 1.0f);  // soft at its sides
            const float fall = 1.0f - d / reach;
            if (edge > 0) panel::over(p.px[size_t(py) * size_t(p.width) + size_t(px)], kCone, 0.42f * edge * fall);
        }
    // the arrow: a tip ahead, two wings behind it, a notch between them
    const auto at = [&](float ahead, float side, float out[2]) {
        out[0] = x + dx * ahead - dy * side;
        out[1] = y + dy * ahead + dx * side;
    };
    float tip[2], left[2], right[2], notch[2];
    const float r = size;
    at(r * 0.95f, 0, tip);
    at(-r * 0.6f, -r * 0.62f, left);
    at(-r * 0.6f, r * 0.62f, right);
    at(-r * 0.22f, 0, notch);
    const auto shape = [&](float grow) {  // tip, right wing, notch; tip, notch, left wing
        const auto out = [&](const float q[2], float o[2]) {
            const float ox = q[0] - x, oy = q[1] - y, d = std::max(1e-3f, std::hypot(ox, oy));
            o[0] = q[0] + ox / d * grow;
            o[1] = q[1] + oy / d * grow;
        };
        float a[2], b[2], c[2], d[2];
        out(tip, a);
        out(right, b);
        out(notch, c);
        out(left, d);
        return std::vector<std::array<float, 6>>{{a[0], a[1], b[0], b[1], c[0], c[1]},
                                                 {a[0], a[1], c[0], c[1], d[0], d[1]}};
    };
    triangles(p, shape(1.6f), kWhite, 1.0f);
    triangles(p, shape(0), kArrow, 1.0f);
}

// The way's next move: a gold pin standing on its cell
void pin(panel::Picture& p, float x, float y, float size) {
    segment(p, panel::Box{0, 0, float(p.width), float(p.height)}, x, y - size * 1.1f, x, y - 1, size * 0.45f,
            kRouteRgb, 1.0f);
    panel::circle(p, x, y - size * 1.75f, size * 0.95f + 1.2f, kBlack, 0.6f);
    panel::circle(p, x, y - size * 1.75f, size * 0.95f, kRouteRgb, 1.0f);
    panel::circle(p, x, y - size * 1.75f, size * 0.34f, kDark, 1.0f);
}

// A picture's rounded corners see-through (premultiplied: each pixel by how much of it the rounded box covers)
void roundCorners(panel::Picture& p, const panel::Box& b, float r) {
    const int x0 = int(b.x), y0 = int(b.y), x1 = int(b.x + b.w), y1 = int(b.y + b.h), rr = int(std::ceil(r));
    const auto cut = [&](int x, int y, float cx, float cy) {
        if (x < 0 || y < 0 || x >= p.width || y >= p.height) return;
        const float d = std::hypot(float(x) + 0.5f - cx, float(y) + 0.5f - cy);
        const float keep = std::clamp(r + 0.5f - d, 0.0f, 1.0f);
        if (keep >= 1) return;
        uint32_t& c = p.px[size_t(y) * size_t(p.width) + size_t(x)];
        const auto scale = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * keep + 0.5f) << sh; };
        c = scale(24) | scale(16) | scale(8) | scale(0);
    };
    for (int y = 0; y < rr; ++y)
        for (int x = 0; x < rr; ++x) {
            cut(x0 + x, y0 + y, float(x0) + r, float(y0) + r);
            cut(x1 - 1 - x, y0 + y, float(x1) - r, float(y0) + r);
            cut(x0 + x, y1 - 1 - y, float(x0) + r, float(y1) - r);
            cut(x1 - 1 - x, y1 - 1 - y, float(x1) - r, float(y1) - r);
        }
}

// The map's picture through a view into a box of the picture: each pixel its cell's (the game's magenta: none)
void mapPixels(panel::Picture& out, const panel::Box& box, const Scene& sc, const View& v) {
    const bool picture = sc.argb && sc.imgW > 0 && sc.imgH > 0 && sc.map.w > 0 && sc.map.h > 0 && v.cellPx > 0 &&
                         sc.argb->size() >= size_t(sc.imgW) * size_t(sc.imgH);
    for (int y = 0; y < int(box.h); ++y) {
        const int ty = int(box.y) + y;
        if (ty < 0 || ty >= out.height) continue;
        const float cy = v.top - (float(y) + 0.5f) / v.cellPx;
        for (int x = 0; x < int(box.w); ++x) {
            const int tx = int(box.x) + x;
            if (tx < 0 || tx >= out.width) continue;
            uint32_t c = kGround;
            const float cx = v.left + (float(x) + 0.5f) / v.cellPx;
            if (picture && cx >= 0 && cy >= 0 && cx < float(sc.map.w) && cy < float(sc.map.h)) {
                // 3.0.11-beta.31: between the picture's four nearest pixels (no blocks when zoomed in); the game's
                // magenta among them - the nearest alone
                const float fx = std::clamp(cx / float(sc.map.w) * float(sc.imgW) - 0.5f, 0.0f, float(sc.imgW - 1));
                const float fy = std::clamp((float(sc.map.h) - cy) / float(sc.map.h) * float(sc.imgH) - 0.5f, 0.0f,
                                            float(sc.imgH - 1));
                const int x0 = int(fx), y0 = int(fy), x1 = std::min(x0 + 1, sc.imgW - 1),
                          y1 = std::min(y0 + 1, sc.imgH - 1);
                const auto px = [&](int ix, int iy) { return (*sc.argb)[size_t(iy) * size_t(sc.imgW) + size_t(ix)]; };
                const auto keyed = [](uint32_t q) { return (q & 0xFFFFFF) == 0xFF00FF || (q >> 24) == 0; };
                const uint32_t q[4] = {px(x0, y0), px(x1, y0), px(x0, y1), px(x1, y1)};
                const float wx = fx - float(x0), wy = fy - float(y0);
                if (keyed(q[0]) || keyed(q[1]) || keyed(q[2]) || keyed(q[3])) {
                    const uint32_t n = q[(wy >= 0.5f ? 2 : 0) + (wx >= 0.5f ? 1 : 0)];
                    if (!keyed(n)) c = 0xFF000000 | (n & 0xFFFFFF);
                } else {
                    uint32_t mixed = 0xFF000000;
                    for (int sh = 0; sh <= 16; sh += 8) {
                        const float top = float((q[0] >> sh) & 0xFF) * (1 - wx) + float((q[1] >> sh) & 0xFF) * wx;
                        const float low = float((q[2] >> sh) & 0xFF) * (1 - wx) + float((q[3] >> sh) & 0xFF) * wx;
                        mixed |= uint32_t(top * (1 - wy) + low * wy + 0.5f) << sh;
                    }
                    c = mixed;
                }
            }
            out.px[size_t(ty) * size_t(out.width) + size_t(tx)] = c;
        }
    }
}

float markRadius(Glyph g, float s) { return g == Glyph::Warp ? 5.5f * s : g == Glyph::Kafra ? 6 * s : 2.8f * s; }

// The marks, the way, its pin and the player over the map's picture in `box` (a view's pixels from its corner)
void overMap(panel::Picture& out, const panel::Box& box, const Scene& sc, const View& v, float s, float big) {
    const auto place = [&](const Cell& c, float& x, float& y) {
        cellToPanel(v, sc.map, c, x, y);
        x += box.x;
        y += box.y;
    };
    const auto inBox = [&](float x, float y, float r) {
        return x >= box.x - r && y >= box.y - r && x <= box.x + box.w + r && y <= box.y + box.h + r;
    };
    float x = 0, y = 0;
    for (size_t i = 0; i < sc.marks.size(); ++i) {
        const Mark& m = sc.marks[i];
        place(m.cell, x, y);
        const float r = markRadius(m.glyph, s) * big;
        if (!inBox(x, y, r)) continue;
        if (int(i) == sc.hoverMark) ring(out, x, y, r + 3 * s, 1.6f * s, kWhite, 0.95f);
        switch (m.glyph) {
            case Glyph::Warp:  // a portal: a dark disc, its blue ring, a white heart
                panel::circle(out, x, y, r + 1, kBlack, 0.6f);
                panel::circle(out, x, y, r, kPortal, 0.95f);
                ring(out, x, y, r - 0.8f * s, 1.5f * s, kPortalRing, 1.0f);
                panel::circle(out, x, y, r * 0.3f, kWhite, 1.0f);
                break;
            case Glyph::Kafra:  // the concept's pink «K» (its letter over it by GDI)
                panel::roundRect(out, panel::Box{x - r, y - r, 2 * r, 2 * r}, 3 * s * big, kKafra, 1.0f, kWhite,
                                 0.9f, std::max(1.0f, s));
                break;
            case Glyph::Dot:
                panel::circle(out, x, y, r + 1.2f, kBlack, 0.8f);
                panel::circle(out, x, y, r, m.rgb, 1.0f);
                break;
        }
    }
    if (sc.path.size() > 1) {  // the way: gold over a dark edge
        float px0 = 0, py0 = 0, px1 = 0, py1 = 0;
        place(sc.path[0], px0, py0);
        for (size_t i = 1; i < sc.path.size(); ++i) {
            place(sc.path[i], px1, py1);
            segment(out, box, px0, py0, px1, py1, (5.5f * big) * s, kBlack, 0.5f);
            segment(out, box, px0, py0, px1, py1, (3.2f * big) * s, kRouteRgb, 1.0f);
            px0 = px1;
            py0 = py1;
        }
    }
    if (sc.pin) {
        place(sc.pinCell, x, y);
        if (inBox(x, y, 0)) pin(out, x, y, 5 * s * big);
    }
    place(sc.player, x, y);
    if (inBox(x, y, 0)) player(out, box, x, y, sc.lookX, sc.lookY, 9.5f * s * big);
}

uint32_t orText(uint32_t rgb) { return rgb ? rgb : kText; }

}  // namespace

bool cellOf(float worldX, float worldZ, int width, int height, int cellSize, Cell& out) {
    if (width <= 0 || height <= 0 || cellSize <= 0 || !std::isfinite(worldX) || !std::isfinite(worldZ)) return false;
    const float x = worldX / float(cellSize) + float(width / 2);
    const float y = worldZ / float(cellSize) + float(height / 2);
    if (!(x >= 0) || !(y >= 0) || x >= float(width) || y >= float(height)) return false;
    out.x = int(x);  // toward zero, as __ftol (positive here)
    out.y = int(y);
    return true;
}

View view(const MapSize& map, const Cell& player, int cells, int size) {
    View v;
    if (cells <= 0 || size <= 0) return v;
    v.cellPx = float(size) / float(cells);
    const float half = float(cells) / 2;
    v.left = std::clamp(float(player.x) + 0.5f - half, 0.0f, std::max(0.0f, float(map.w - cells)));
    v.top = std::clamp(float(player.y) + 0.5f + half, std::min(float(map.h), float(cells)), float(map.h));
    if (cells >= std::max(map.w, map.h)) {  // 3.0.11-beta.31: the whole map - in the square's middle
        v.left = (float(map.w) - float(cells)) / 2;
        v.top = float(map.h) + (float(cells) - float(map.h)) / 2;
    }
    return v;
}

void cellToPanel(const View& v, const MapSize&, const Cell& c, float& px, float& py) {
    px = (float(c.x) + 0.5f - v.left) * v.cellPx;
    py = (v.top - (float(c.y) + 0.5f)) * v.cellPx;
}

int cellsAt(int step, const MapSize& map) {
    step = std::clamp(step, 0, kZoomSteps - 1);
    if (step == kZoomSteps - 1) return std::max(1, std::max(map.w, map.h));
    return kZoomCells[step];
}

Layout layout(int targetW, int targetH, float scale, bool route, int listLines) {
    Layout L;
    const float s = std::max(0.5f, scale);
    L.s = s;
    const float side = std::round(kSide * s), gap = std::round(kGap * s), b = std::round(kButton * s);
    L.w = int(side);
    L.map = panel::Box{0, 0, side, side};
    const float inset = std::round(6 * s), bx = inset;  // the buttons: a column in the square's bottom left corner
    L.expand = panel::Box{bx, side - inset - b, b, b};
    L.zoomOut = panel::Box{bx, L.expand.y - std::round(kButtonGap * s) - b, b, b};
    L.zoomIn = panel::Box{bx, L.zoomOut.y - std::round(kButtonGap * s) - b, b, b};
    float y = side + gap;
    const float bar = std::round(kBarH * s);
    L.caption = panel::Box{0, y, side, bar};
    L.fold = panel::Box{side - std::round(26 * s), y + std::round(4 * s), std::round(22 * s), bar - std::round(8 * s)};
    y += bar;
    if (route) {
        y += gap;
        L.route = panel::Box{0, y, side, std::round(kRouteH * s)};
        L.stop = panel::Box{side - std::round(24 * s), y + std::round(4 * s), std::round(20 * s), std::round(18 * s)};
        y += L.route.h;
    }
    if (listLines > 0) {
        y += gap;
        L.lineH = std::round(kLineH * s);
        L.list = panel::Box{0, y, side, L.lineH * float(listLines) + 2 * std::round(kListPad * s)};
        y += L.list.h;
    }
    L.h = int(std::ceil(y));
    const int m = int(std::lround(kMargin * s));
    L.x = std::max(0, targetW - m - L.w);
    L.y = std::min(m, std::max(0, targetH - L.h));
    return L;
}

Part partAt(const Layout& L, float x, float y) {
    for (const auto& [box, part] : {std::pair{L.zoomIn, Part::ZoomIn}, std::pair{L.zoomOut, Part::ZoomOut},
                                    std::pair{L.expand, Part::Expand}, std::pair{L.fold, Part::Fold},
                                    std::pair{L.stop, Part::Stop}, std::pair{L.map, Part::Map},
                                    std::pair{L.caption, Part::Caption}, std::pair{L.route, Part::Route},
                                    std::pair{L.list, Part::List}})
        if (box.w > 0 && box.contains(x, y)) return part;
    return Part::None;
}

int lineAt(const Layout& L, float y) {
    if (L.list.w <= 0 || L.lineH <= 0) return -1;
    const float top = L.list.y + std::round(kListPad * L.s);
    if (y < top) return -1;
    const int i = int((y - top) / L.lineH);
    return float(i) * L.lineH + top < L.list.y + L.list.h ? i : -1;
}

void paint(panel::Picture& out, const Layout& L, const Scene& sc, const Texts& t) {
    out.width = L.w;
    out.height = L.h;
    out.px.assign(size_t(out.width) * size_t(out.height), 0u);  // see-through between the parts
    const float s = L.s, edge = std::max(1.0f, s);
    const View v = view(sc.map, sc.player, sc.cells, int(L.map.w));
    mapPixels(out, L.map, sc, v);
    overMap(out, L.map, sc, v, s, 1.0f);
    roundCorners(out, L.map, kRadius * s);
    panel::roundRect(out, L.map, kRadius * s, kWhite, 0.0f, kWhite, 0.16f, edge);
    const float arm = 15 * s, thick = std::max(1.5f, 2 * s);  // the concept's gold corners
    for (const auto& [cx, cy, sx, sy] : {std::array<float, 4>{0, 0, 1, 1}, std::array<float, 4>{L.map.w, 0, -1, 1},
                                         std::array<float, 4>{0, L.map.h, 1, -1},
                                         std::array<float, 4>{L.map.w, L.map.h, -1, -1}}) {
        panel::roundRect(out, panel::Box{sx > 0 ? cx : cx - arm, sy > 0 ? cy : cy - thick, arm, thick}, 0, kAccent2,
                         0.95f);
        panel::roundRect(out, panel::Box{sx > 0 ? cx : cx - thick, sy > 0 ? cy : cy - arm, thick, arm}, 0, kAccent2,
                         0.95f);
    }
    const std::pair<const panel::Box*, uiicons::Icon> buttons[] = {{&L.zoomIn, uiicons::Icon::Plus},
                                                                  {&L.zoomOut, uiicons::Icon::Minus},
                                                                  {&L.expand, uiicons::Icon::Expand}};
    const Part parts[] = {Part::ZoomIn, Part::ZoomOut, Part::Expand};
    for (int i = 0; i < 3; ++i) {
        const panel::Box& b = *buttons[i].first;
        const bool over = sc.hover == parts[i];
        panel::roundRect(out, b, 6 * s, over ? kAccent : kDark, over ? 0.55f : 0.72f, kWhite, 0.18f, edge);
        uiicons::draw(out, buttons[i].second, b.x + (b.w - 14 * s) / 2, b.y + (b.h - 14 * s) / 2, 14 * s, kWhite,
                      1.0f, 2.6f);
    }
    panel::roundRect(out, L.caption, 8 * s, kCard, 0.92f, kWhite, 0.12f, edge);  // the bar: name, cell, ▾
    if (sc.hover == Part::Fold) panel::roundRect(out, L.fold, 6 * s, kWhite, 0.10f);
    if (L.route.w > 0) {  // the way's bar: gold-tinted, its ×
        panel::roundRect(out, L.route, 8 * s, kCard, 0.92f);
        panel::roundRect(out, L.route, 8 * s, kAccent, 0.16f, kAccent, 0.55f, edge);
        if (sc.hover == Part::Stop) panel::roundRect(out, L.stop, 5 * s, kWhite, 0.12f);
    }
    if (L.list.w > 0) panel::roundRect(out, L.list, 8 * s, kCard, 0.88f, kWhite, 0.10f, edge);
    (void)t;
    if (sc.opacity < 1) panel::fade(out, sc.opacity);  // Ctrl+Tab's see-through
}

std::vector<Words> words(const Layout& L, const Scene& sc, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    const View v = view(sc.map, sc.player, sc.cells, int(L.map.w));
    for (const Mark& m : sc.marks) {  // the Kafras' «K»
        if (m.glyph != Glyph::Kafra) continue;
        float x = 0, y = 0;
        cellToPanel(v, sc.map, m.cell, x, y);
        const float r = markRadius(m.glyph, s);
        if (x - r < 0 || y - r < 0 || x + r > L.map.w || y + r > L.map.h) continue;
        out.push_back(Words{panel::Box{x - r, y - r, 2 * r, 2 * r}, "K", kWhite, 9 * s, true, Align::Center});
    }
    const panel::Box& c = L.caption;
    const bool hovered = sc.hoverMark >= 0 && size_t(sc.hoverMark) < sc.marks.size();
    if (hovered) {  // the mark under the pointer names itself on the bar
        out.push_back(Words{panel::Box{c.x + 10 * s, c.y, L.fold.x - 4 * s - c.x - 10 * s, c.h},
                            sc.marks[size_t(sc.hoverMark)].label, kAccent2, 12.5f * s, true, Align::Left});
    } else {
        const float cellW = std::round(62 * s);
        out.push_back(Words{panel::Box{c.x + 10 * s, c.y, L.fold.x - 6 * s - cellW - c.x - 10 * s, c.h}, t.name, kText,
                            13 * s, true, Align::Left});
        out.push_back(Words{panel::Box{L.fold.x - 4 * s - cellW, c.y, cellW, c.h}, t.cell, kMuted, 12 * s, false,
                            Align::Right});
    }
    out.push_back(Words{L.fold, t.folded ? "▸" : "▾", sc.hover == Part::Fold ? kText : kMuted, 12 * s, false,
                        Align::Center});
    if (L.route.w > 0 && !t.route.empty()) {
        const panel::Box& r = L.route;
        const float rightW = std::round(48 * s);
        const Line& a = t.route[0];
        out.push_back(Words{panel::Box{r.x + 10 * s, r.y + 5 * s, L.stop.x - rightW - r.x - 14 * s, 18 * s}, a.left,
                            a.leftRgb ? a.leftRgb : kAccent2, 13 * s, true, Align::Left});
        if (!a.right.empty())
            out.push_back(Words{panel::Box{L.stop.x - rightW - 2 * s, r.y + 5 * s, rightW, 18 * s}, a.right, kMuted,
                                11.5f * s, false, Align::Right});
        if (t.route.size() > 1)
            out.push_back(Words{panel::Box{r.x + 10 * s, r.y + 24 * s, r.w - 20 * s, 16 * s}, t.route[1].left, kMuted,
                                12 * s, false, Align::Left});
        out.push_back(Words{L.stop, "✕", sc.hover == Part::Stop ? kText : kMuted, 11 * s, false, Align::Center});
    }
    if (L.list.w > 0) {
        const float top = L.list.y + std::round(kListPad * s), split = (L.list.w - 20 * s) * 0.66f;
        for (size_t i = 0; i < t.list.size(); ++i) {
            const Line& l = t.list[i];
            const float y = top + float(i) * L.lineH;
            out.push_back(Words{panel::Box{L.list.x + 10 * s, y, l.right.empty() ? L.list.w - 20 * s : split, L.lineH},
                                l.left, orText(l.leftRgb), 12 * s, l.bold, Align::Left});
            if (!l.right.empty())
                out.push_back(Words{panel::Box{L.list.x + 10 * s + split, y, L.list.w - 20 * s - split, L.lineH},
                                    l.right, l.rightRgb ? l.rightRgb : kMuted, 12 * s, false, Align::Right});
        }
    }
    return out;
}

int markAt(const Layout& L, const Scene& sc, float x, float y) {
    if (!L.map.contains(x, y)) return -1;
    const View v = view(sc.map, sc.player, sc.cells, int(L.map.w));
    int best = -1;
    float bestD = 0;
    for (size_t i = 0; i < sc.marks.size(); ++i) {
        float px = 0, py = 0;
        cellToPanel(v, sc.map, sc.marks[i].cell, px, py);
        const float reach = markRadius(sc.marks[i].glyph, L.s) + 4 * L.s;  // a hand's width more
        const float d = (px - x) * (px - x) + (py - y) * (py - y);
        if (d <= reach * reach && (best < 0 || d < bestD)) {
            best = int(i);
            bestD = d;
        }
    }
    return best;
}

namespace {

constexpr float kBigMargin = 48, kBigHeader = 46, kBigPad = 14;

// The whole map in the big map's box: its cells fitted, the box exactly its shape
View bigView(const Big& b, const Scene& sc) {
    View v;
    if (sc.map.w <= 0 || sc.map.h <= 0) return v;
    v.cellPx = b.map.w / float(sc.map.w);
    v.left = 0;
    v.top = float(sc.map.h);
    return v;
}

// The words beside the marks that always have them on the big map (the warps, the Kafras) and the one under the
// pointer: one of two that would lie over each other left out
struct Label {
    panel::Box box;
    std::string text;
    uint32_t rgb;
};
std::vector<Label> bigLabels(const Big& b, const Scene& sc) {
    std::vector<Label> out;
    const float s = b.s;
    const View v = bigView(b, sc);
    const auto guess = [&](const std::string& t) {  // the words' width, as the window's font is near (13 px bold)
        float w = 0;
        for (size_t i = 0; i < t.size(); ++i)
            if ((static_cast<unsigned char>(t[i]) & 0xC0) != 0x80) w += 7.6f * s;
        return w;
    };
    const auto add = [&](size_t i, bool force) {
        const Mark& m = sc.marks[i];
        if (m.label.empty()) return;
        float x = 0, y = 0;
        cellToPanel(v, sc.map, m.cell, x, y);
        x += b.map.x;
        y += b.map.y;
        const float r = markRadius(m.glyph, s) * 1.5f, w = guess(m.label) + 14 * s, h = 22 * s;
        panel::Box box{x + r + 4 * s, y - h / 2, w, h};
        if (box.x + box.w > b.map.x + b.map.w) box.x = x - r - 4 * s - w;  // past the right edge: on the left
        box.x = std::clamp(box.x, b.map.x, std::max(b.map.x, b.map.x + b.map.w - box.w));
        box.y = std::clamp(box.y, b.map.y, std::max(b.map.y, b.map.y + b.map.h - box.h));
        for (const Label& o : out)
            if (!force && box.x < o.box.x + o.box.w && o.box.x < box.x + box.w && box.y < o.box.y + o.box.h &&
                o.box.y < box.y + box.h)
                return;
        out.push_back(Label{box, m.label, m.glyph == Glyph::Warp ? 0xC7CEFF : m.glyph == Glyph::Kafra ? 0xFFC2E0
                                                                                                       : kText});
    };
    for (size_t i = 0; i < sc.marks.size(); ++i)
        if (sc.marks[i].glyph != Glyph::Dot) add(i, false);
    if (sc.hoverMark >= 0 && size_t(sc.hoverMark) < sc.marks.size() && sc.marks[size_t(sc.hoverMark)].glyph ==
                                                                           Glyph::Dot)
        add(size_t(sc.hoverMark), true);
    return out;
}

}  // namespace

Big bigLayout(int targetW, int targetH, float scale, const MapSize& map) {
    Big b;
    const float s = std::max(0.5f, scale);
    b.s = s;
    const float margin = kBigMargin * s, header = std::round(kBigHeader * s), pad = std::round(kBigPad * s);
    const float roomW = float(targetW) - 2 * margin - 2 * pad, roomH = float(targetH) - 2 * margin - header - pad;
    const float cell = std::max(0.1f, std::min(roomW / float(std::max(1, map.w)), roomH / float(std::max(1, map.h))));
    const float mapW = std::round(float(std::max(1, map.w)) * cell);
    const float mapH = std::round(float(std::max(1, map.h)) * cell);
    b.w = int(mapW + 2 * pad);
    b.h = int(header + mapH + pad);
    b.x = std::max(0, (targetW - b.w) / 2);
    b.y = std::max(0, (targetH - b.h) / 2);
    b.header = panel::Box{0, 0, float(b.w), header};
    b.close = panel::Box{float(b.w) - std::round(10 * s) - std::round(32 * s), (header - std::round(32 * s)) / 2,
                         std::round(32 * s), std::round(32 * s)};
    b.map = panel::Box{pad, header, mapW, mapH};
    return b;
}

BigPart bigPartAt(const Big& b, float x, float y) {
    if (b.close.contains(x, y)) return BigPart::Close;
    if (b.map.contains(x, y)) return BigPart::Map;
    if (x >= 0 && y >= 0 && x < float(b.w) && y < float(b.h)) return BigPart::Header;
    return BigPart::None;
}

void paintBigMap(panel::Picture& out, const Big& b, const Scene& sc) {
    out.width = int(b.map.w);
    out.height = int(b.map.h);
    out.px.assign(size_t(out.width) * size_t(out.height), kGround);
    mapPixels(out, panel::Box{0, 0, b.map.w, b.map.h}, sc, bigView(b, sc));
}

void paintBig(panel::Picture& out, const panel::Picture& map, const Big& b, const Scene& sc) {
    out.width = b.w;
    out.height = b.h;
    out.px.assign(size_t(out.width) * size_t(out.height), 0u);
    const float s = b.s, edge = std::max(1.0f, s);
    panel::roundRect(out, panel::Box{0, 0, float(b.w), float(b.h)}, 12 * s, kCard, 0.96f, kWhite, 0.14f, edge);
    panel::roundRect(out, panel::Box{0, b.header.h - edge, float(b.w), edge}, 0, kWhite, 0.10f);
    if (sc.bigHover == BigPart::Close) panel::roundRect(out, b.close, 9 * s, kWhite, 0.10f);  // the cross hovered
    if (map.width == int(b.map.w) && map.height == int(b.map.h))  // the map's picture, made once
        for (int y = 0; y < map.height; ++y)
            std::copy_n(map.px.begin() + std::ptrdiff_t(size_t(y) * size_t(map.width)), map.width,
                        out.px.begin() + std::ptrdiff_t(size_t(int(b.map.y) + y) * size_t(out.width) +
                                                        size_t(b.map.x)));
    const View v = bigView(b, sc);
    overMap(out, b.map, sc, v, s, 1.5f);
    for (const Label& l : bigLabels(b, sc))  // the words' grounds
        panel::roundRect(out, l.box, 6 * s, kDark, 0.84f);
    if (sc.opacity < 1) panel::fade(out, sc.opacity);
}

std::vector<Words> bigWords(const Big& b, const Scene& sc, const std::string& title, const std::string& cell) {
    std::vector<Words> out;
    const float s = b.s;
    const float cellW = std::round(110 * s);
    out.push_back(Words{panel::Box{16 * s, 0, b.close.x - 16 * s - cellW - 24 * s, b.header.h}, title, kText, 16 * s,
                        true, Align::Left});
    out.push_back(Words{panel::Box{b.close.x - 12 * s - cellW, 0, cellW, b.header.h}, cell, kMuted, 13 * s, false,
                        Align::Right});
    out.push_back(Words{b.close, "✕", sc.bigHover == BigPart::Close ? kText : kMuted, 16 * s, false, Align::Center});
    const View v = bigView(b, sc);
    for (const Mark& m : sc.marks) {  // the Kafras' «K»
        if (m.glyph != Glyph::Kafra) continue;
        float x = 0, y = 0;
        cellToPanel(v, sc.map, m.cell, x, y);
        const float r = markRadius(m.glyph, s) * 1.5f;
        out.push_back(Words{panel::Box{b.map.x + x - r, b.map.y + y - r, 2 * r, 2 * r}, "K", kWhite, 12 * s, true,
                            Align::Center});
    }
    for (const Label& l : bigLabels(b, sc))
        out.push_back(Words{l.box, l.text, l.rgb, 12.5f * s, true, Align::Center});
    return out;
}

int bigMarkAt(const Big& b, const Scene& sc, float x, float y) {
    if (!b.map.contains(x, y)) return -1;
    const View v = bigView(b, sc);
    int best = -1;
    float bestD = 0;
    for (size_t i = 0; i < sc.marks.size(); ++i) {
        float px = 0, py = 0;
        cellToPanel(v, sc.map, sc.marks[i].cell, px, py);
        px += b.map.x;
        py += b.map.y;
        const float reach = markRadius(sc.marks[i].glyph, b.s) * 1.5f + 5 * b.s;
        const float d = (px - x) * (px - x) + (py - y) * (py - y);
        if (d <= reach * reach && (best < 0 || d < bestD)) {
            best = int(i);
            bestD = d;
        }
    }
    return best;
}

Look next(Look l) {
    switch (l) {
        case Look::Opaque: return Look::Faded;
        case Look::Faded: return Look::Hidden;
        case Look::Hidden: break;
    }
    return Look::Opaque;
}

float opacity(Look l) {
    switch (l) {
        case Look::Opaque: return 1.0f;
        case Look::Faded: return float(0xAA) / float(0xFF);  // the game's see-through minimap: 0xAAFFFFFF
        case Look::Hidden: break;
    }
    return 0.0f;
}

}  // namespace uagfx::minimap
