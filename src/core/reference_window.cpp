#include "core/reference_window.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::refwin {
namespace {

// At scale 1 (1080p), the target's pixels
constexpr float kW = 1270, kH = 850, kHeader = 44, kSearch = 50, kFooter = 28, kPad = 18, kEdgeRoom = 20;
constexpr float kTitleRow = 46, kSub = 26, kHeading = 38, kRow = 26, kNote = 24, kTile = 78, kGap = 10;
constexpr uint32_t kGround = 0x12161E, kBand = 0x0D1016, kField = 0x1B212C, kFieldEdge = 0x4C8DFF;
constexpr uint32_t kRule = 0x2A3342, kHover = 0x1F2733, kTileFill = 0x1A2029, kTileHot = 0x232B37;
constexpr uint32_t kTrack = 0x161B23, kThumb = 0x3A4556;

int px(float v, float s) { return int(std::lround(v * s)); }

int heightOf(refpages::Style st, float s) {
    switch (st) {
        case refpages::Style::Title: return px(kTitleRow, s);
        case refpages::Style::Sub: return px(kSub, s);
        case refpages::Style::Heading: return px(kHeading, s);
        case refpages::Style::Note: return px(kNote, s);
        case refpages::Style::Tile: return px(kTile, s);
        case refpages::Style::Row: break;
    }
    return px(kRow, s);
}

panel::Box backBox(const Layout& L) { return panel::Box{float(L.pad) * 0.5f, 0, float(L.w) * 0.42f, float(L.header)}; }
panel::Box closeBox(const Layout& L) {
    return panel::Box{float(L.w - L.header), 0, float(L.header), float(L.header)};
}
panel::Box fieldBox(const Layout& L) {
    return panel::Box{float(L.pad), float(L.header) + 7 * L.s, float(L.w - 2 * L.pad), float(L.search) - 14 * L.s};
}

void fill(panel::Picture& p, const panel::Box& b, uint32_t rgb) { panel::roundRect(p, b, 0, rgb, 1.0f); }

}  // namespace

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    L.s = std::max(0.5f, scale);
    const int room = px(kEdgeRoom, L.s);
    L.w = std::max(200, std::min(px(kW, L.s), targetW - 2 * room));
    L.h = std::max(200, std::min(px(kH, L.s), targetH - 2 * room));
    L.x = std::max(0, (targetW - L.w) / 2);
    L.y = std::max(0, (targetH - L.h) / 2);
    L.header = px(kHeader, L.s);
    L.search = px(kSearch, L.s);
    L.footer = px(kFooter, L.s);
    L.pad = px(kPad, L.s);
    L.bodyY = L.header + L.search;
    L.bodyH = std::max(0, L.h - L.bodyY - L.footer);
    return L;
}

std::vector<Placed> place(const Layout& L, const refpages::Page& p, int& total) {
    std::vector<Placed> out;
    out.reserve(p.rows.size());
    const int innerW = L.w - 2 * L.pad - px(10, L.s);  // room for the scroll bar
    const int gap = px(kGap, L.s);
    const int tileW = (innerW - 2 * gap) / 3;
    int y = px(6, L.s), tile = 0;
    for (const refpages::Row& r : p.rows) {
        const int h = heightOf(r.style, L.s);
        if (r.style == refpages::Style::Tile) {
            const int col = tile % 3;
            if (col == 0 && tile > 0) y += h + gap;
            out.push_back(Placed{L.pad + col * (tileW + gap), y, tileW, h});
            ++tile;
            continue;
        }
        if (tile > 0) {  // after the tiles' last line
            y += heightOf(refpages::Style::Tile, L.s) + gap;
            tile = 0;
        }
        out.push_back(Placed{L.pad, y, innerW, h});
        y += h;
    }
    if (tile > 0) y += heightOf(refpages::Style::Tile, L.s) + gap;
    total = y + px(8, L.s);
    return out;
}

int clampScroll(int scroll, int total, int bodyH) { return std::clamp(scroll, 0, std::max(0, total - bodyH)); }

bool shown(const Layout& L, const Placed& r, int scroll) { return r.y - scroll >= 0 && r.y - scroll + r.h <= L.bodyH; }

Under under(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll, bool backShown,
            float x, float y) {
    if (x < 0 || y < 0 || x >= float(L.w) || y >= float(L.h)) return Under{};
    if (closeBox(L).contains(x, y)) return Under{Part::Close, -1};
    if (backShown && backBox(L).contains(x, y)) return Under{Part::Back, -1};
    if (y >= float(L.header) && y < float(L.bodyY)) return Under{Part::Search, -1};
    if (y < float(L.bodyY) || y >= float(L.bodyY + L.bodyH)) return Under{};
    const float by = y - float(L.bodyY) + float(scroll);
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        if (p.rows[i].link.go != refpages::Go::None && x >= float(r.x) && x < float(r.x + r.w) && by >= float(r.y) &&
            by < float(r.y + r.h) && shown(L, r, scroll))
            return Under{Part::Row, int(i)};
    }
    return Under{Part::Body, -1};
}

void paint(panel::Picture& out, const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
           int total, const Look& look) {
    out.width = L.w;
    out.height = L.h;
    out.px.assign(size_t(L.w) * size_t(L.h), 0xFF000000u | kGround);
    const float s = L.s;
    fill(out, panel::Box{0, 0, float(L.w), float(L.header)}, kBand);
    if (look.backShown && look.hover.part == Part::Back) fill(out, backBox(L), kHover);
    if (look.hover.part == Part::Close) fill(out, closeBox(L), 0x5A2330);
    panel::roundRect(out, fieldBox(L), 6 * s, kField, 1.0f, kFieldEdge, 0.9f, std::max(1.0f, 1.5f * s));
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        if (!shown(L, r, scroll)) continue;
        const panel::Box b{float(r.x), float(L.bodyY + r.y - scroll), float(r.w), float(r.h)};
        const refpages::Row& row = p.rows[i];
        const bool hot = look.hover.part == Part::Row && look.hover.row == int(i);
        if (row.style == refpages::Style::Tile) {
            panel::roundRect(out, b, 8 * s, hot ? kTileHot : kTileFill, 1.0f, hot ? kFieldEdge : kRule, 1.0f,
                             std::max(1.0f, s));
            continue;
        }
        if (hot) panel::roundRect(out, panel::Box{b.x - 6 * s, b.y, b.w + 12 * s, b.h}, 4 * s, kHover, 1.0f);
        if (row.style == refpages::Style::Heading)
            fill(out, panel::Box{b.x, b.y + b.h - std::max(1.0f, s), b.w, std::max(1.0f, s)}, kRule);
        if (row.dot) panel::circle(out, b.x + 6 * s, b.y + b.h / 2, 4 * s, row.dot, 1.0f);
    }
    fill(out, panel::Box{0, float(L.h - L.footer), float(L.w), float(L.footer)}, kBand);
    if (total > L.bodyH && L.bodyH > 0) {  // the scroll bar
        const float trackX = float(L.w) - 10 * s, trackY = float(L.bodyY) + 4 * s, trackH = float(L.bodyH) - 8 * s;
        panel::roundRect(out, panel::Box{trackX, trackY, 5 * s, trackH}, 2.5f * s, kTrack, 1.0f);
        const float thumbH = std::max(24 * s, trackH * float(L.bodyH) / float(total));
        const float at = trackY + (trackH - thumbH) * float(scroll) / float(std::max(1, total - L.bodyH));
        panel::roundRect(out, panel::Box{trackX, at, 5 * s, thumbH}, 2.5f * s, kThumb, 1.0f);
    }
    panel::roundRect(out, panel::Box{0, 0, float(L.w), float(L.h)}, 0, 0, 0.0f, kRule, 1.0f, std::max(1.0f, s));
}

std::vector<Words> words(const Layout& L, const refpages::Page& p, const std::vector<Placed>& rows, int scroll,
                         const Texts& t, const Look& look) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{panel::Box{float(L.w) * 0.3f, 0, float(L.w) * 0.4f, float(L.header)}, t.title, panel::kText,
                        17 * s, true, Align::Center});
    if (look.backShown && !t.back.empty())
        out.push_back(Words{backBox(L), "‹ " + t.back, look.hover.part == Part::Back ? kAccent : panel::kMuted, 14 * s,
                            false, Align::Left});
    out.push_back(Words{closeBox(L), "×", look.hover.part == Part::Close ? panel::kText : panel::kMuted, 22 * s, false,
                        Align::Center});
    const panel::Box field = fieldBox(L);
    const panel::Box typed{field.x + 12 * s, field.y, field.w - 24 * s, field.h};
    if (t.query.empty())
        out.push_back(Words{typed, "| " + t.hint, panel::kMuted, 15.5f * s, false, Align::Left});
    else
        out.push_back(Words{typed, t.query + "|", panel::kText, 15.5f * s, false, Align::Left});
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        const Placed& r = rows[i];
        if (!shown(L, r, scroll)) continue;
        const refpages::Row& row = p.rows[i];
        const float x = float(r.x), y = float(L.bodyY + r.y - scroll), w = float(r.w), h = float(r.h);
        const bool hot = look.hover.part == Part::Row && look.hover.row == int(i);
        const bool link = row.link.go != refpages::Go::None;
        switch (row.style) {
            case refpages::Style::Tile:
                out.push_back(Words{panel::Box{x + 14 * s, y + 8 * s, w - 28 * s, h * 0.5f - 6 * s}, row.left,
                                    hot ? kAccent : panel::kText, 16 * s, true, Align::Left});
                out.push_back(Words{panel::Box{x + 14 * s, y + h * 0.5f, w - 28 * s, h * 0.5f - 8 * s}, row.right,
                                    panel::kMuted, 13 * s, false, Align::Left});
                break;
            case refpages::Style::Title:
                out.push_back(Words{panel::Box{x, y, w * 0.72f, h}, row.left, row.gold ? kGold : panel::kText, 24 * s,
                                    true, Align::Left});
                out.push_back(Words{panel::Box{x + w * 0.72f, y, w * 0.28f, h}, row.right, panel::kMuted, 15 * s, false,
                                    Align::Right});
                break;
            case refpages::Style::Sub:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, panel::kMuted, 13.5f * s, false, Align::Left});
                break;
            case refpages::Style::Heading:
                out.push_back(Words{panel::Box{x, y, w * 0.8f, h}, row.left, kAccent, 15.5f * s, true, Align::Left});
                out.push_back(Words{panel::Box{x + w * 0.8f, y, w * 0.2f, h}, row.right, panel::kMuted, 13 * s, false,
                                    Align::Right});
                break;
            case refpages::Style::Note:
                out.push_back(Words{panel::Box{x, y, w, h}, row.left, panel::kMuted, 13.5f * s, false, Align::Left});
                break;
            case refpages::Style::Row: {
                const float indent = row.dot ? 16 * s : 0;
                const float split = w * 0.6f;
                const uint32_t rgb = row.gold ? kGold : hot ? kAccent : link ? kLink : panel::kText;
                out.push_back(Words{panel::Box{x + indent, y, split - indent - 8 * s, h}, row.left, rgb, 14 * s,
                                    row.gold, Align::Left});
                out.push_back(Words{panel::Box{x + split, y, w - split, h}, row.right, panel::kMuted, 13 * s, false,
                                    Align::Right});
                break;
            }
        }
    }
    out.push_back(Words{panel::Box{float(L.pad), float(L.h - L.footer), float(L.w - 2 * L.pad), float(L.footer)},
                        t.footer, panel::kMuted, 12.5f * s, false, Align::Center});
    return out;
}

}  // namespace uagfx::refwin
