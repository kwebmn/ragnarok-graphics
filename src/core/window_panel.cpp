#include "core/window_panel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <sstream>

#include "core/d3d7.hpp"
#include "core/states.hpp"

namespace uagfx::panel {
namespace {

// The panel at scale 1, in pixels
constexpr float kPad = 6, kTop = 24, kRadius = 9, kCardRadius = 7, kBar = 5, kMargin = 8;
constexpr float kRowCard = 150, kNarrowCard = 104, kRowCardH = 52, kRowGap = 6;
constexpr float kColumnCard = 210, kColumnCardH = 48, kColumnGap = 5;
constexpr float kLevels = 46;
constexpr float kIcon = 30, kIconGap = 4, kIconEdge = 4, kIconGrip = 12;  // 3.0.3-beta.11: the icons
constexpr uint32_t kGround = 0x111319, kGreen = 0x4ADE80, kBlue = 0x60A5FA, kRed = 0xF87171, kWhite = 0xFFFFFF;
constexpr uint32_t kCurrent = 0x3B82F6, kCurrentEdge = 0x60A5FA;

// Signed distance to a rounded box (negative inside)
float roundBox(float px, float py, const Box& b, float r) {
    r = std::min(r, std::min(b.w, b.h) / 2);
    const float qx = std::fabs(px - (b.x + b.w / 2)) - (b.w / 2 - r);
    const float qy = std::fabs(py - (b.y + b.h / 2)) - (b.h / 2 - r);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

float cover(float d) { return std::clamp(0.5f - d, 0.0f, 1.0f); }

// A shape's distance at every pixel of its box (and one around): filled, and a ring `edge` wide inside its border
template <typename Distance>
void shape(Picture& p, const Box& around, const Distance& distance, uint32_t fill, float fillA, uint32_t edgeRgb,
           float edgeA, float edge) {
    const int x0 = std::max(0, int(std::floor(around.x)) - 1), y0 = std::max(0, int(std::floor(around.y)) - 1);
    const int x1 = std::min(p.width, int(std::ceil(around.x + around.w)) + 1);
    const int y1 = std::min(p.height, int(std::ceil(around.y + around.h)) + 1);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const float d = distance(float(x) + 0.5f, float(y) + 0.5f);
            const float inside = cover(d);
            if (inside <= 0.0f) continue;
            uint32_t& px = p.px[size_t(y) * size_t(p.width) + size_t(x)];
            over(px, fill, fillA * inside);
            if (edgeA > 0.0f) over(px, edgeRgb, edgeA * (inside - cover(d + edge)));
        }
}

void roundRect(Picture& p, const Box& b, float r, uint32_t fill, float fillA, uint32_t edgeRgb = 0, float edgeA = 0,
               float edge = 0) {
    shape(p, b, [&](float x, float y) { return roundBox(x, y, b, r); }, fill, fillA, edgeRgb, edgeA, edge);
}

void circle(Picture& p, float cx, float cy, float r, uint32_t rgb, float a) {
    shape(p, Box{cx - r, cy - r, 2 * r, 2 * r},
          [&](float x, float y) { return std::hypot(x - cx, y - cy) - r; }, rgb, a, 0, 0, 0);
}

void line(Picture& p, float ax, float ay, float bx, float by, float r, uint32_t rgb, float a) {
    const float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    const Box around{std::min(ax, bx) - r, std::min(ay, by) - r, std::fabs(dx) + 2 * r, std::fabs(dy) + 2 * r};
    shape(p, around,
          [&](float x, float y) {
              const float t = len2 > 0 ? std::clamp(((x - ax) * dx + (y - ay) * dy) / len2, 0.0f, 1.0f) : 0.0f;
              return std::hypot(x - (ax + t * dx), y - (ay + t * dy)) - r;
          },
          rgb, a, 0, 0, 0);
}

void bar(Picture& p, const Box& b, int percent, bool hp) {
    if (percent < 0) return;
    roundRect(p, b, b.h / 2, kWhite, 30.0f / 255);
    if (percent == 0) return;
    Box fill = b;
    fill.w = std::max(b.h, b.w * float(std::min(percent, 100)) / 100.0f);
    roundRect(p, fill, b.h / 2, barColour(percent, hp), 1.0f);
}

}  // namespace

Geometry geometry(int count, Layout layout, float s, int roomWidth) {
    Geometry g;
    g.scale = s;
    g.radius = kRadius * s;
    count = std::max(count, 0);
    if (layout == Layout::Icons) {  // 3.0.3-beta.11: the grip, then a tile a window; no top row
        const float w = kIconEdge + kIconGrip + float(count) * kIcon + float(std::max(count - 1, 0)) * kIconGap +
                        kIconEdge;
        const float h = 2 * kIconEdge + kIcon;
        g.width = int(std::lround(w * s));
        g.height = int(std::lround(h * s));
        g.radius = 8 * s;
        g.top = Box{0, 0, (kIconEdge + kIconGrip) * s, h * s};
        g.grip = g.top;
        for (int i = 0; i < count; ++i) {
            const float x = kIconEdge + kIconGrip + float(i) * (kIcon + kIconGap), y = kIconEdge;
            CardBoxes c;
            c.plate = Box{x * s, y * s, kIcon * s, kIcon * s};
            c.number = Box{x * s, (y + 1) * s, kIcon * s, 18 * s};
            c.badgeX = (x + kIcon / 2) * s;
            c.badgeY = (y + 10) * s;
            c.hp = Box{(x + 5) * s, (y + 20) * s, (kIcon - 10) * s, 3 * s};
            c.sp = Box{(x + 5) * s, (y + 25) * s, (kIcon - 10) * s, 3 * s};
            g.cards.push_back(c);
        }
        return g;
    }
    const bool row = layout == Layout::Row;
    float cw = row ? kRowCard : kColumnCard;
    const float ch = row ? kRowCardH : kColumnCardH, gap = row ? kRowGap : kColumnGap;
    const auto widthOf = [&](float w) {
        return row ? 2 * kPad + float(count) * w + float(count - 1) * gap : 2 * kPad + w;
    };
    if (row && count > 0 && widthOf(cw) * s > float(roomWidth) / 2) {
        cw = kNarrowCard;
        g.narrow = true;
    }
    const float w = widthOf(cw);
    const float h = row ? kTop + ch + kPad : kTop + float(count) * ch + float(std::max(count - 1, 0)) * gap + kPad;
    g.width = int(std::lround(w * s));
    g.height = int(std::lround(h * s));
    g.top = Box{0, 0, w * s, kTop * s};
    g.grip = Box{6 * s, 4 * s, 14 * s, 16 * s};
    g.title = Box{22 * s, 2 * s, (w - 72) * s, 20 * s};
    g.layout = Box{(w - 48) * s, 2 * s, 20 * s, 20 * s};
    g.close = Box{(w - 26) * s, 2 * s, 20 * s, 20 * s};
    for (int i = 0; i < count; ++i) {
        const float x = row ? kPad + float(i) * (cw + gap) : kPad;
        const float y = row ? kTop : kTop + float(i) * (ch + gap);
        const float middle = row ? 16 : 15, bars = row ? 32 : 29;
        const float levels = g.narrow ? 0 : kLevels;
        CardBoxes c;
        c.plate = Box{x * s, y * s, cw * s, ch * s};
        c.badgeX = (x + 17) * s;
        c.badgeY = (y + middle) * s;
        c.badgeR = 9 * s;
        c.number = Box{c.badgeX - c.badgeR, c.badgeY - c.badgeR, 2 * c.badgeR, 2 * c.badgeR};
        c.levels = Box{(x + cw - 10 - levels) * s, (y + middle - 10) * s, levels * s, 20 * s};
        c.name = Box{(x + 31) * s, (y + middle - 10) * s, (cw - 41 - (levels > 0 ? levels + 4 : 0)) * s, 20 * s};
        c.hp = Box{(x + 10) * s, (y + bars) * s, (cw - 20) * s, kBar * s};
        c.sp = Box{(x + 10) * s, (y + bars + 8) * s, (cw - 20) * s, kBar * s};
        g.cards.push_back(c);
    }
    return g;
}

Hit hitTest(const Geometry& g, float x, float y) {
    Hit h;
    if (x < 0 || y < 0 || x >= float(g.width) || y >= float(g.height)) return h;
    if (g.close.contains(x, y)) {
        h.part = Part::Close;
    } else if (g.layout.contains(x, y)) {
        h.part = Part::Layout;
    } else if (g.top.contains(x, y)) {
        h.part = Part::Top;
    } else {
        for (size_t i = 0; i < g.cards.size(); ++i)
            if (g.cards[i].plate.contains(x, y)) {
                h.part = Part::Card;
                h.card = int(i);
            }
    }
    return h;
}

Place parsePlace(const std::string& text) {
    std::istringstream in(text);
    Place p;
    float x = 0, y = 0;
    if (!(in >> x >> y) || !std::isfinite(x) || !std::isfinite(y)) return p;
    p.x = std::clamp(x, 0.0f, 1.0f);
    p.y = std::clamp(y, 0.0f, 1.0f);
    return p;
}

std::string formatPlace(const Place& p) {
    char text[32];
    std::snprintf(text, sizeof text, "%.3f %.3f", double(p.x), double(p.y));
    return text;
}

Point position(const Place& p, int panelW, int panelH, int roomW, int roomH, float scale) {
    const float m = kMargin * scale;
    const float freeW = std::max(0.0f, float(roomW - panelW) - 2 * m);
    const float freeH = std::max(0.0f, float(roomH - panelH) - 2 * m);
    return Point{int(std::lround(m + p.x * freeW)), int(std::lround(m + p.y * freeH))};
}

Place placeOf(Point at, int panelW, int panelH, int roomW, int roomH, float scale) {
    const float m = kMargin * scale;
    const float freeW = float(roomW - panelW) - 2 * m, freeH = float(roomH - panelH) - 2 * m;
    Place p;
    p.x = freeW > 0 ? std::clamp((float(at.x) - m) / freeW, 0.0f, 1.0f) : 0.0f;
    p.y = freeH > 0 ? std::clamp((float(at.y) - m) / freeH, 0.0f, 1.0f) : 0.0f;
    return p;
}

uint32_t barColour(int percent, bool hp) { return percent < 25 ? kRed : hp ? kGreen : kBlue; }

int cursorDraw(const FrameRecord& r, float x, float y) {
    constexpr int kLookBack = 8;  // without its colour: among the frame's last draws
    constexpr float kMost = 64;   // a cursor's picture is no bigger
    // A small interface quad over the point (`near`: up to that far off it): whether every corner has the cursor's
    // colour
    const auto over = [&](const DrawCmd& d, bool& coloured, float near) {
        const uint32_t stride = FrameRecord::stride(d.fvf);
        if (!stride || d.vertexCount < 3 || d.vertexCount > 6 || d.state >= r.states().size() ||
            !r.states()[d.state].ui)
            return false;
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        coloured = true;
        for (uint32_t v = 0; v < d.vertexCount; ++v) {
            const uint8_t* at = r.vertexBytes().data() + d.vertexOffset + size_t(v) * stride;
            float p[2];
            uint32_t colour = 0;
            std::memcpy(p, at, sizeof p);
            std::memcpy(&colour, at + 16, 4);  // FVF_TL1 and TL2: x y z rhw, then the diffuse colour
            x0 = std::min(x0, p[0]);
            y0 = std::min(y0, p[1]);
            x1 = std::max(x1, p[0]);
            y1 = std::max(y1, p[1]);
            coloured = coloured && colour == kCursorColour;
        }
        return x1 - x0 <= kMost && y1 - y0 <= kMost && x >= x0 - near && x <= x1 + near && y >= y0 - near &&
               y <= y1 + near;
    };
    bool coloured = false;
    // 3.0.3-beta.11: by its colour, wherever it is; 3.0.3-beta.12: a few pixels off it too - FUN_004f5a90 puts its
    // quad half a pixel right and down of the point, the arrow's tip at the picture's corner
    for (size_t c = r.commands().size(); c-- > 0;)
        if (r.commands()[c].type == Cmd::Draw && over(r.draws()[r.commands()[c].index], coloured, 3.0f) && coloured)
            return int(c);
    int seen = 0;
    for (size_t c = r.commands().size(); c-- > 0 && seen < kLookBack;) {
        const Command& cmd = r.commands()[c];
        if (cmd.type != Cmd::Draw) continue;
        ++seen;
        if (over(r.draws()[cmd.index], coloured, 0.0f)) return int(c);
    }
    return -1;
}

Point tipPlace(const Box& tile, int w, int h, int roomW, int roomH, float scale) {
    const float gap = 6 * scale;
    float y = tile.y - gap - float(h);
    if (y < 0) y = tile.y + tile.h + gap;  // the panel at the top: under the tile
    const float x = tile.x + tile.w / 2 - float(w) / 2;
    return Point{std::clamp(int(std::lround(x)), 0, std::max(0, roomW - w)),
                 std::clamp(int(std::lround(y)), 0, std::max(0, roomH - h))};
}

void paintTip(Picture& out, int w, int h, float scale) {
    out.width = std::max(w, 0);
    out.height = std::max(h, 0);
    out.px.assign(size_t(out.width) * size_t(out.height), 0u);
    roundRect(out, Box{0, 0, float(out.width), float(out.height)}, 6 * scale, kGround, 236.0f / 255, kWhite,
              40.0f / 255, std::max(1.0f, scale));
}

MenuGeometry menuGeometry(const std::vector<MenuRow>& rows, float s) {
    constexpr float kWidth = 360, kRow = 28, kLabelW = 112, kSegmentsX = 128, kGap = 4, kLine = 7;
    MenuGeometry g;
    g.scale = s;
    float y = kPad;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0 && rows[i].kind != rows[i - 1].kind) {  // a thin line between kinds
            g.lines.push_back((y + kLine / 2) * s);
            y += kLine;
        }
        const MenuRow& r = rows[i];
        g.rows.push_back(Box{kPad * s, y * s, (kWidth - 2 * kPad) * s, kRow * s});
        const bool box = r.kind == MenuRow::Kind::Check;
        const float lx = box ? 34 : 12;
        const float lw = r.kind == MenuRow::Kind::Choice ? kLabelW : kWidth - lx - 12;
        g.label.push_back(Box{lx * s, y * s, lw * s, kRow * s});
        g.check.push_back(box ? Box{12 * s, (y + 7) * s, 14 * s, 14 * s} : Box{});
        std::vector<Box> segs;
        const int n = r.kind == MenuRow::Kind::Choice ? std::max(r.segments, 0) : 0;
        if (n > 0) {
            const float w = (kWidth - 10 - kSegmentsX - float(n - 1) * kGap) / float(n);
            for (int k = 0; k < n; ++k)
                segs.push_back(Box{(kSegmentsX + float(k) * (w + kGap)) * s, (y + 4) * s, w * s, (kRow - 8) * s});
        }
        g.segments.push_back(segs);
        y += kRow;
    }
    g.width = int(std::lround(kWidth * s));
    g.height = int(std::lround((y + kPad) * s));
    return g;
}

MenuHit menuHit(const MenuGeometry& g, float x, float y) {
    MenuHit h;
    for (size_t i = 0; i < g.rows.size(); ++i) {
        if (!g.rows[i].contains(x, y)) continue;
        h.row = int(i);
        for (size_t k = 0; k < g.segments[i].size(); ++k)
            if (g.segments[i][k].contains(x, y)) h.segment = int(k);
    }
    return h;
}

void paintMenu(Picture& out, const MenuGeometry& g, const std::vector<MenuRow>& rows, MenuHit hover) {
    out.width = g.width;
    out.height = g.height;
    out.px.assign(size_t(g.width) * size_t(g.height), 0u);
    const float s = g.scale, edge = std::max(1.0f, s);
    roundRect(out, Box{0, 0, float(g.width), float(g.height)}, kRadius * s, kGround, 236.0f / 255, kWhite,
              40.0f / 255, edge);
    for (float y : g.lines) roundRect(out, Box{12 * s, y - edge / 2, float(g.width) - 24 * s, edge}, 0, kWhite, 0.12f);
    for (size_t i = 0; i < rows.size() && i < g.rows.size(); ++i) {
        const MenuRow& r = rows[i];
        if (hover.row == int(i) && r.kind != MenuRow::Kind::Choice)
            roundRect(out, g.rows[i], 5 * s, kWhite, 22.0f / 255);
        for (size_t k = 0; k < g.segments[i].size(); ++k) {
            const bool chosen = int(k) == r.chosen, hovered = hover.row == int(i) && hover.segment == int(k);
            if (chosen)
                roundRect(out, g.segments[i][k], 5 * s, kCurrent, 170.0f / 255, kCurrentEdge, 1.0f, edge);
            else
                roundRect(out, g.segments[i][k], 5 * s, kWhite, (hovered ? 34.0f : 14.0f) / 255, kWhite,
                          (hovered ? 70.0f : 30.0f) / 255, edge);
        }
        if (r.kind == MenuRow::Kind::Check) {
            const Box& b = g.check[i];
            if (r.on) {
                roundRect(out, b, 3 * s, kCurrentEdge, 1.0f);
                line(out, b.x + 3 * s, b.y + 7 * s, b.x + 6 * s, b.y + 10 * s, 0.9f * s, kWhite, 1.0f);  // a tick
                line(out, b.x + 6 * s, b.y + 10 * s, b.x + 11 * s, b.y + 4 * s, 0.9f * s, kWhite, 1.0f);
            } else {
                roundRect(out, b, 3 * s, kWhite, 10.0f / 255, kWhite, 110.0f / 255, edge);
            }
        }
    }
}

namespace {

enum MenuItem : int { kLayoutItem, kSizeItem, kOpacityItem, kMusicItem, kSoundsItem, kLightItem, kHideItem };

template <size_t N>
int indexOf(const int (&values)[N], int v) {
    for (size_t i = 0; i < N; ++i)
        if (values[i] == v) return int(i);
    return -1;
}

}  // namespace

std::vector<MenuRow> menuRows(const Config& c) {
    using K = MenuRow::Kind;
    return {{K::Choice, 3, int(c.panelLayout), false},  // a row, a column, the icons (3.0.3-beta.11)
            {K::Choice, int(std::size(kMenuSizes)), indexOf(kMenuSizes, c.panelSize), false},
            {K::Choice, int(std::size(kMenuOpacities)), indexOf(kMenuOpacities, c.panelOpacity), false},
            {K::Check, 0, -1, c.windowOthersMusic},
            {K::Check, 0, -1, c.windowOthersSounds},
            {K::Check, 0, -1, c.windowOthersLight},
            {K::Action, 0, -1, false}};
}

std::vector<MenuText> menuTexts(settings::Lang lang) {
    using settings::Text;
    const auto t = [lang](Text x) { return std::string(settings::tr(lang, x)); };
    const char* percent = lang == settings::Lang::En ? "%" : " %";
    std::vector<std::string> sizes, opacities;
    for (int v : kMenuSizes) sizes.push_back(std::to_string(v) + percent);
    for (int v : kMenuOpacities) opacities.push_back(std::to_string(v) + percent);
    return {{t(Text::PanelMenuLayout),
             {t(Text::WindowsPanelRow), t(Text::WindowsPanelColumn), t(Text::WindowsPanelIcons)}},
            {t(Text::PanelMenuSize), sizes},
            {t(Text::PanelMenuOpacity), opacities},
            {t(Text::WindowsOthersMusic), {}},
            {t(Text::WindowsOthersSounds), {}},
            {t(Text::WindowsOthersLight), {}},
            {t(Text::PanelMenuHide), {}}};
}

bool menuPick(Config& c, MenuHit hit, MenuChange& change) {
    change = MenuChange{};
    const auto segment = [&](size_t count) { return hit.segment >= 0 && size_t(hit.segment) < count; };
    const auto flip = [&](bool& on, const char* key) {
        on = !on;
        change = MenuChange{"windows", key, on ? "1" : "0", false};
        return true;
    };
    switch (hit.row) {
    case kLayoutItem:
        if (!segment(3)) return false;
        c.panelLayout = PanelLayout(hit.segment);
        change = MenuChange{"panel", "layout", panelLayoutToken(c.panelLayout), false};
        return true;
    case kSizeItem:
        if (!segment(std::size(kMenuSizes))) return false;
        c.panelSize = kMenuSizes[hit.segment];
        change = MenuChange{"panel", "size", std::to_string(c.panelSize), false};
        return true;
    case kOpacityItem:
        if (!segment(std::size(kMenuOpacities))) return false;
        c.panelOpacity = kMenuOpacities[hit.segment];
        change = MenuChange{"panel", "opacity", std::to_string(c.panelOpacity), false};
        return true;
    case kMusicItem: return flip(c.windowOthersMusic, "others_music");
    case kSoundsItem: return flip(c.windowOthersSounds, "others_sounds");
    case kLightItem: return flip(c.windowOthersLight, "others_light");
    case kHideItem:
        change.hide = true;
        return true;
    default: return false;
    }
}

void fade(Picture& p, float opacity) {
    if (opacity >= 1.0f) return;
    const float a = std::max(opacity, 0.0f);
    for (uint32_t& px : p.px) {
        const auto scaled = [&](int shift) { return uint32_t(std::lround(float((px >> shift) & 0xFF) * a)) << shift; };
        px = scaled(24) | scaled(16) | scaled(8) | scaled(0);
    }
}

void over(uint32_t& dst, uint32_t rgb, float a) {
    if (a <= 0.0f) return;
    a = std::min(a, 1.0f);
    const float keep = 1.0f - a;
    const auto channel = [&](int shift) {
        return std::lround(float((rgb >> shift) & 0xFF) * a + float((dst >> shift) & 0xFF) * keep);
    };
    const long alpha = std::lround(255.0f * a + float(dst >> 24) * keep);
    dst = uint32_t(alpha) << 24 | uint32_t(channel(16)) << 16 | uint32_t(channel(8)) << 8 | uint32_t(channel(0));
}

void paint(Picture& out, const Geometry& g, const std::vector<Card>& cards, Part hovered, Layout layout) {
    out.width = g.width;
    out.height = g.height;
    out.px.assign(size_t(g.width) * size_t(g.height), 0u);
    const float s = g.scale, edge = std::max(1.0f, s);
    roundRect(out, Box{0, 0, float(g.width), float(g.height)}, g.radius, kGround, 224.0f / 255, kWhite, 34.0f / 255,
              edge);
    if (layout == Layout::Icons) {  // 3.0.3-beta.11: the grip at its start, a tile a window - the current one blue
        const float middle = float(g.height) / 2;
        for (float gx : {7.0f, 11.0f})
            for (float gy : {-5.0f, 0.0f, 5.0f}) circle(out, gx * s, middle + gy * s, 1.3f * s, kWhite, 96.0f / 255);
        for (size_t i = 0; i < g.cards.size() && i < cards.size(); ++i) {
            const CardBoxes& b = g.cards[i];
            const Card& c = cards[i];
            if (c.current)
                roundRect(out, b.plate, 7 * s, kCurrent, 200.0f / 255, kCurrentEdge, 1.0f, edge);
            else
                roundRect(out, b.plate, 7 * s, kWhite, (c.hover ? 30.0f : 14.0f) / 255, kWhite,
                          (c.hover ? 70.0f : 30.0f) / 255, edge);
            bar(out, b.hp, c.hp, true);
            bar(out, b.sp, c.sp, false);
        }
        return;
    }
    for (float gx : {10.0f, 15.0f})  // the grip: dragged by the top row
        for (float gy : {8.0f, 12.0f, 16.0f}) circle(out, gx * s, gy * s, 1.3f * s, kWhite, 96.0f / 255);
    const float cx = g.close.x + g.close.w / 2, cy = g.close.y + g.close.h / 2, arm = 4 * s;
    const uint32_t closeRgb = hovered == Part::Close ? kWhite : 0x969EAE;
    line(out, cx - arm, cy - arm, cx + arm, cy + arm, 0.7f * s, closeRgb, 1.0f);
    line(out, cx - arm, cy + arm, cx + arm, cy - arm, 0.7f * s, closeRgb, 1.0f);
    // the layout button: three bars one under another (to a column) or side by side (to a row)
    const float lx = g.layout.x + g.layout.w / 2, ly = g.layout.y + g.layout.h / 2;
    const uint32_t layoutRgb = hovered == Part::Layout ? kWhite : 0x969EAE;
    for (float o : {-4.0f, 0.0f, 4.0f}) {
        const Box bar = layout == Layout::Row ? Box{lx - 5 * s, ly + (o - 1) * s, 10 * s, 2 * s}
                                              : Box{lx + (o - 1) * s, ly - 5 * s, 2 * s, 10 * s};
        roundRect(out, bar, s, layoutRgb, 1.0f);
    }
    for (size_t i = 0; i < g.cards.size() && i < cards.size(); ++i) {
        const CardBoxes& b = g.cards[i];
        const Card& c = cards[i];
        if (c.current)
            roundRect(out, b.plate, kCardRadius * s, kCurrent, 70.0f / 255, kCurrentEdge, 230.0f / 255, edge);
        else
            roundRect(out, b.plate, kCardRadius * s, kWhite, (c.hover ? 26.0f : 12.0f) / 255, kWhite,
                      (c.hover ? 60.0f : 26.0f) / 255, edge);
        circle(out, b.badgeX, b.badgeY, b.badgeR, c.current ? kCurrentEdge : kWhite, c.current ? 1.0f : 46.0f / 255);
        bar(out, b.hp, c.hp, true);
        bar(out, b.sp, c.sp, false);
    }
}

}  // namespace uagfx::panel
