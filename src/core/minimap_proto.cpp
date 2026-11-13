#include "core/minimap_proto.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::minimap {
namespace {

// At scale 1, the target's pixels (3.0.11-beta.31: the concept's v19_town - 200 a side, the bars under it)
constexpr float kSide = 220, kMargin = 10, kGap = 6, kBarH = 30, kRouteH = 46, kLineH = 19, kListPad = 7;
// 3.0.11-beta.127: the round trip's plate - 20 high, its words 58 wide, a graph of 60 bars a pixel each, 12 high
constexpr float kNetH = 20, kNetText = 58, kNetGraphW = 60, kNetGraphH = 12, kNetPad = 8;
constexpr uint32_t kNetGood = 0x5FD08A, kNetFair = 0xF0A050, kNetBad = 0xFF7D6E, kNetStall = 0xFF5A4E;
constexpr float kButton = 22, kButtonGap = 4, kRadius = 10;
constexpr float kBigFooter = 44;  // 3.0.11-beta.119: the big map's row of the layers' chips
// The concept's style A: its glass, white over it, its gold, its words' colours
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kDark = 0x0B0E14;
constexpr uint32_t kAccent = 0xC9A55B, kAccent2 = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5;
constexpr uint32_t kGround = 0xFF141922;   // the map's square where it has no picture
constexpr uint32_t kKafra = 0xE86AA8;      // the concept's «K»
// 3.0.11-beta.201: a guild mate's; 3.0.11-beta.222 (the tester: «з відсилкою на оригінал і старим кольором») the
// game's own pink for them (its mode's message 0xB8 files the mark 0xFFA6D2), not the NPCs' green
constexpr uint32_t kGuildRing = 0xFFA6D2;
constexpr uint32_t kPortal = 0x23285A, kPortalRing = 0x8E9BFF;
// 3.0.11-beta.187 (the tester took the fan for the character's sight): the camera's fan a neutral light, the arrow
// alone blue
constexpr uint32_t kArrow = 0x4FA3FF, kCone = 0xDDE6F2;
constexpr uint32_t kMemberRing = 0x7FD3FF, kDeadRing = 0xFF5A4E;  // 3.0.11-beta.83: the party's (as the chat's)

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
            const float d = panel::length(float(x) + 0.5f - cx, float(y) + 0.5f - cy);  // 3.0.11-beta.73: inline
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

// The player: where the camera looks, a soft fan of its view; 3.0.11-beta.71 the arrow the way the character faces
// (the camera's when not known); neither known - a white dot
void player(panel::Picture& p, const panel::Box& clip, float x, float y, float lookX, float lookY, float headX,
            float headY, float size) {
    const float len = std::hypot(lookX, lookY), head = std::hypot(headX, headY);
    if (len < 1e-4f && head < 1e-4f) {
        panel::circle(p, x, y, size * 0.32f + 1.5f, kBlack, 0.85f);
        panel::circle(p, x, y, size * 0.32f, kWhite, 1.0f);
        return;
    }
    const float fx = len < 1e-4f ? 0.0f : lookX / len, fy = len < 1e-4f ? 0.0f : -lookY / len;  // north up
    const float reach = len < 1e-4f ? 0.0f : size * 3.6f, half = 0.66f;  // the view: 38 degrees each way
    // 3.0.11-beta.57: well outside the fan by its cosine - no arc cosine for four pixels of five
    const float outside = std::cos(half) - 1e-3f;
    for (int py = std::max(int(clip.y), int(y - reach)); py <= std::min(int(clip.y + clip.h) - 1, int(y + reach));
         ++py)
        for (int px = std::max(int(clip.x), int(x - reach)); px <= std::min(int(clip.x + clip.w) - 1, int(x + reach));
             ++px) {
            if (px < 0 || py < 0 || px >= p.width || py >= p.height) continue;
            const float vx = float(px) + 0.5f - x, vy = float(py) + 0.5f - y, d = panel::length(vx, vy);
            if (d > reach || d < 1) continue;
            const float along = (vx * fx + vy * fy) / d;
            if (along < outside) continue;
            const float angle = std::acos(std::clamp(along, -1.0f, 1.0f));
            const float edge = std::clamp((half - angle) * reach * 0.5f, 0.0f, 1.0f);  // soft at its sides
            const float fall = 1.0f - d / reach;
            if (edge > 0) panel::over(p.px[size_t(py) * size_t(p.width) + size_t(px)], kCone, 0.42f * edge * fall);
        }
    // the arrow: a tip ahead, two wings behind it, a notch between them - the way the character faces
    const float dx = head < 1e-4f ? fx : headX / head, dy = head < 1e-4f ? fy : -headY / head;
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
        const float d = panel::length(float(x) + 0.5f - cx, float(y) + 0.5f - cy);
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

// 3.0.11-beta.65: a cell not reached on foot - its picture at 40 %, still seen (3.0.11-beta.126: a walked-on one of
// another part only - a wall or the map's ground dimmed made c_tower2's light ground its dark corridors' grey)
uint32_t dimmed(uint32_t argb) {
    uint32_t out = argb & 0xFF000000;
    for (int sh = 0; sh <= 16; sh += 8) out |= (((argb >> sh) & 0xFF) * 2 / 5) << sh;
    return out;
}

// 3.0.11-beta.72: two pixels mixed, `t` of 256 the second's share - the four channels at once, two in each half
uint32_t mix(uint32_t a, uint32_t b, uint32_t t) {
    const uint32_t rb = ((a & 0x00FF00FFu) * (256 - t) + (b & 0x00FF00FFu) * t) >> 8;
    const uint32_t ag = ((a >> 8) & 0x00FF00FFu) * (256 - t) + ((b >> 8) & 0x00FF00FFu) * t;
    return (rb & 0x00FF00FFu) | (ag & 0xFF00FF00u);
}

// A premultiplied pixel over the ground (its channels never past its alpha: no carry between them)
uint32_t overGround(uint32_t p) {
    const uint32_t a = p >> 24, keep = 256 - a - (a >> 7);
    const uint32_t rb = (((kGround & 0x00FF00FFu) * keep) >> 8) & 0x00FF00FFu;
    const uint32_t g = (((kGround >> 8) & 0xFFu) * keep) & 0xFF00u;
    return 0xFF000000u | ((p & 0x00FFFFFFu) + rb + g);
}

}  // namespace

int dropWarpDots(std::vector<uint32_t>& argb, int width, int height) {
    if (width <= 0 || height <= 0 || argb.size() < size_t(width) * size_t(height)) return 0;
    const auto rgbOf = [](uint32_t c, int& r, int& g, int& b) {
        r = int((c >> 16) & 0xFF), g = int((c >> 8) & 0xFF), b = int(c & 0xFF);
    };
    const auto pure = [&](uint32_t c) {  // opaque: the pictures' dots are (an AI picture's soft edge is not one)
        int r, g, b;
        rgbOf(c, r, g, b);
        return (c >> 24) >= 0xF0 && r >= 200 && g <= 50 && b <= 50;
    };
    const auto reddish = [&](uint32_t c) {
        int r, g, b;
        rgbOf(c, r, g, b);
        return r >= 90 && r - std::max(g, b) >= 50 && std::abs(g - b) <= 24;
    };
    const size_t n = size_t(width) * size_t(height);
    std::vector<uint8_t> fill(n, 0);  // 1 to fill
    std::vector<int> blob(n, -1);
    int dots = 0;
    std::vector<size_t> stack;
    for (size_t i = 0; i < n; ++i) {
        if (blob[i] >= 0 || !pure(argb[i])) continue;
        // a dot: its pure middle by flood, then its soft red edge within 3 pixels
        stack.assign(1, i);
        blob[i] = dots;
        std::vector<size_t> core;
        while (!stack.empty()) {
            const size_t j = stack.back();
            stack.pop_back();
            core.push_back(j);
            const int x = int(j % size_t(width)), y = int(j / size_t(width));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int u = x + dx, v = y + dy;
                    if (u < 0 || v < 0 || u >= width || v >= height) continue;
                    const size_t k = size_t(v) * size_t(width) + size_t(u);
                    if (blob[k] < 0 && pure(argb[k])) {
                        blob[k] = dots;
                        stack.push_back(k);
                    }
                }
        }
        // its round whole (an enlarged picture's dot is hatched dark red inside): from its middle out past its farthest
        // pure pixel by 2.5, the reddish ones round it by 4 more
        double cx = 0, cy = 0;
        for (const size_t j : core) cx += double(j % size_t(width)), cy += double(j / size_t(width));
        cx = cx / double(core.size()) + 0.5, cy = cy / double(core.size()) + 0.5;
        double radius = 0;
        for (const size_t j : core)
            radius = std::max(radius, std::hypot(double(j % size_t(width)) + 0.5 - cx,
                                                 double(j / size_t(width)) + 0.5 - cy));
        // 3.0.16-beta.6 (R-W9VM29, ama_dun01's «X»: the picture's maze route is drawn in the same pure red - one blob
        // the whole map round, filled from its edges in, 47 s): a dot is round and small - its pure pixels at least
        // half its circle, its radius a 32nd of the picture at most (16 pixels at least); a line kept as drawn
        const double circle = 3.14159265358979 * std::max(1.0, radius * radius);
        if (double(core.size()) < 0.5 * circle || radius > std::max(16.0, double(std::max(width, height)) / 32.0))
            continue;
        const int reach = int(std::ceil(radius + 6.5));
        for (int v = std::max(0, int(cy) - reach); v <= std::min(height - 1, int(cy) + reach); ++v)
            for (int u = std::max(0, int(cx) - reach); u <= std::min(width - 1, int(cx) + reach); ++u) {
                const size_t k = size_t(v) * size_t(width) + size_t(u);
                const double d = std::hypot(double(u) + 0.5 - cx, double(v) + 0.5 - cy);
                if (keyedPink(argb[k])) continue;
                if (d <= radius + 2.5 || (d <= radius + 6.5 && reddish(argb[k]))) fill[k] = 1;
            }
        ++dots;
    }
    if (!dots) return 0;
    // filled ring by ring from its known pixels round it (the key's pink not taken where another colour is near)
    for (bool more = true; more;) {
        more = false;
        std::vector<std::pair<size_t, uint32_t>> ring;
        for (size_t i = 0; i < n; ++i) {
            if (!fill[i]) continue;
            const int x = int(i % size_t(width)), y = int(i / size_t(width));
            int sum[3] = {0, 0, 0}, count = 0, keyed = 0;
            uint32_t key = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int u = x + dx, v = y + dy;
                    if ((!dx && !dy) || u < 0 || v < 0 || u >= width || v >= height) continue;
                    const size_t k = size_t(v) * size_t(width) + size_t(u);
                    if (fill[k]) continue;
                    if (keyedPink(argb[k])) {
                        ++keyed;
                        key = argb[k];
                        continue;
                    }
                    int r, g, b;
                    rgbOf(argb[k], r, g, b);
                    sum[0] += r, sum[1] += g, sum[2] += b;
                    ++count;
                }
            if (count)
                ring.emplace_back(i, 0xFF000000u | uint32_t(sum[0] / count) << 16 | uint32_t(sum[1] / count) << 8 |
                                         uint32_t(sum[2] / count));
            else if (keyed)
                ring.emplace_back(i, key);
        }
        for (const auto& [i, c] : ring) {
            argb[i] = c;
            fill[i] = 0;
            more = true;
        }
    }
    return dots;
}

MapPicture prepare(const std::vector<uint32_t>& given, int width, int height) {
    MapPicture m;
    if (width <= 0 || height <= 0 || given.size() < size_t(width) * size_t(height)) return m;
    // 3.0.11-beta.201: the picture's warp dots out (the minimap's rings mark the warps) - on a copy, where it has any
    std::vector<uint32_t> undotted;
    const bool dotted = std::any_of(given.begin(), given.begin() + ptrdiff_t(size_t(width) * size_t(height)),
                                    [](uint32_t c) {
                                        return (c >> 24) >= 0xF0 && ((c >> 16) & 0xFF) >= 200 &&
                                               ((c >> 8) & 0xFF) <= 50 && (c & 0xFF) <= 50;
                                    });
    if (dotted) {
        undotted.assign(given.begin(), given.begin() + ptrdiff_t(size_t(width) * size_t(height)));
        dropWarpDots(undotted, width, height);
    }
    const std::vector<uint32_t>& argb = dotted ? undotted : given;
    MapPicture::Level first{width, height, std::vector<uint32_t>(size_t(width) * size_t(height))};
    std::vector<uint8_t> key(first.px.size());
    for (size_t i = 0; i < key.size(); ++i) key[i] = keyedPink(argb[i]);
    // a lossy upscale's pink fringe (texture_x4.zip: the first ring round the key 98.6 % pinkish, the second 35.8 %,
    // further in 0.1 % - the maps' own colours): pinkish within two pixels of the key
    const auto fringe = [&](int x, int y) {
        for (int v = std::max(0, y - 2); v <= std::min(height - 1, y + 2); ++v)
            for (int u = std::max(0, x - 2); u <= std::min(width - 1, x + 2); ++u)
                if (key[size_t(v) * size_t(width) + size_t(u)]) return true;
        return false;
    };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {  // the game's pictures opaque but for their key, the AI's soft
            const size_t i = size_t(y) * size_t(width) + size_t(x);
            const uint32_t c = argb[i], a = c >> 24;
            const int r = int((c >> 16) & 0xFF), g = int((c >> 8) & 0xFF), b = int(c & 0xFF);
            if (key[i] || (std::min(r, b) - g > 20 && fringe(x, y))) continue;
            uint32_t p = a << 24;
            for (int sh = 0; sh <= 16; sh += 8) p |= ((((c >> sh) & 0xFF) * a + 127) / 255) << sh;
            first.px[i] = p;
        }
    m.levels.push_back(std::move(first));
    while (m.levels.back().w > 1 || m.levels.back().h > 1) {  // halved down to a pixel: each the average of four
        const MapPicture::Level& up = m.levels.back();
        MapPicture::Level next{std::max(1, up.w / 2), std::max(1, up.h / 2), {}};
        next.px.resize(size_t(next.w) * size_t(next.h));
        for (int y = 0; y < next.h; ++y) {
            const uint32_t* a = up.px.data() + size_t(std::min(2 * y, up.h - 1)) * size_t(up.w);
            const uint32_t* b = up.px.data() + size_t(std::min(2 * y + 1, up.h - 1)) * size_t(up.w);
            for (int x = 0; x < next.w; ++x) {
                const int x0 = std::min(2 * x, up.w - 1), x1 = std::min(2 * x + 1, up.w - 1);
                uint32_t out = 0;
                for (int sh = 0; sh <= 24; sh += 8) {
                    const uint32_t sum = ((a[x0] >> sh) & 0xFF) + ((a[x1] >> sh) & 0xFF) + ((b[x0] >> sh) & 0xFF) +
                                         ((b[x1] >> sh) & 0xFF);
                    out |= ((sum + 2) / 4) << sh;
                }
                next.px[size_t(y) * size_t(next.w) + size_t(x)] = out;
            }
        }
        m.levels.push_back(std::move(next));
    }
    return m;
}

namespace {

// The map's picture through a view into a box of the picture: each pixel its cell's (the game's magenta: none).
// 3.0.11-beta.57 (R-T942DZ: the minimap painted again at each step, 15.6 ms at 4K): what a column or a row of the box
// takes from the picture worked out once for it, not for each of its pixels - the same numbers, the same picture
void mapPixelsMade(panel::Picture& out, const panel::Box& box, const Scene& sc, const View& v) {
    // 3.0.11-beta.72: the prepared picture (made here of argb for a scene without one - the tests); the smallest level
    // with as many pixels a cell as the view shows
    MapPicture made;
    const MapPicture* pic = sc.picture;
    if (!pic && sc.argb && sc.imgW > 0 && sc.imgH > 0 && sc.argb->size() >= size_t(sc.imgW) * size_t(sc.imgH)) {
        made = prepare(*sc.argb, sc.imgW, sc.imgH);
        pic = &made;
    }
    const MapPicture::Level* lv = nullptr;
    if (pic && !pic->levels.empty() && sc.map.w > 0 && sc.map.h > 0 && v.cellPx > 0) {
        const float need = v.cellPx * float(sc.map.w);
        for (const MapPicture::Level& l : pic->levels)
            if (!lv || float(l.w) >= need * 0.999f) lv = &l;
    }
    struct Column {
        int at;      // the out picture's x
        bool in;     // over the map
        int x0, x1;  // the level's two columns
        uint32_t wx;  // between them, of 256
        int cell;    // 3.0.11-beta.65: the map's cell column under it
    };
    // 3.0.11-beta.65: the cells not reached from the player dimmed (the reached ones as they are); 3.0.11-beta.126:
    // walked on, another part's (walkgrid::reachMask's kElsewhere) - the walls and the ground as drawn
    const std::vector<uint8_t>* reach =
        sc.reach && sc.reach->size() == size_t(std::max(0, sc.map.w)) * size_t(std::max(0, sc.map.h)) ? sc.reach
                                                                                                      : nullptr;
    std::vector<Column> columns;
    columns.reserve(size_t(std::max(0, int(box.w))));
    for (int x = 0; x < int(box.w); ++x) {
        const int tx = int(box.x) + x;
        if (tx < 0 || tx >= out.width) continue;
        Column c{tx, false, 0, 0, 0, 0};
        const float cx = v.left + (float(x) + 0.5f) / v.cellPx;
        c.cell = std::clamp(int(std::floor(cx)), 0, std::max(0, sc.map.w - 1));
        if (lv && cx >= 0 && cx < float(sc.map.w)) {
            const float fx = std::clamp(cx / float(sc.map.w) * float(lv->w) - 0.5f, 0.0f, float(lv->w - 1));
            c.in = true;
            c.x0 = int(fx);
            c.x1 = std::min(c.x0 + 1, lv->w - 1);
            c.wx = std::min(256u, uint32_t((fx - float(c.x0)) * 256.0f + 0.5f));
        }
        columns.push_back(c);
    }
    for (int y = 0; y < int(box.h); ++y) {
        const int ty = int(box.y) + y;
        if (ty < 0 || ty >= out.height) continue;
        uint32_t* row = out.px.data() + size_t(ty) * size_t(out.width);
        const float cy = v.top - (float(y) + 0.5f) / v.cellPx;
        if (!lv || !(cy >= 0 && cy < float(sc.map.h))) {
            for (const Column& c : columns) row[c.at] = kGround;
            continue;
        }
        const float fy = std::clamp((float(sc.map.h) - cy) / float(sc.map.h) * float(lv->h) - 0.5f, 0.0f,
                                    float(lv->h - 1));
        const int y0 = int(fy), y1 = std::min(y0 + 1, lv->h - 1);
        const uint32_t wy = std::min(256u, uint32_t((fy - float(y0)) * 256.0f + 0.5f));
        const uint32_t* top = lv->px.data() + size_t(y0) * size_t(lv->w);
        const uint32_t* low = lv->px.data() + size_t(y1) * size_t(lv->w);
        const uint8_t* reached =  // 3.0.11-beta.65: this row's cells
            reach ? reach->data() + size_t(std::clamp(int(std::floor(cy)), 0, sc.map.h - 1)) * size_t(sc.map.w)
                  : nullptr;
        for (const Column& c : columns) {
            uint32_t pixel = kGround;
            if (c.in) {
                // between the four nearest, premultiplied - a see-through one adds nothing: the ground shows there
                const uint32_t p = mix(mix(top[c.x0], top[c.x1], c.wx), mix(low[c.x0], low[c.x1], c.wx), wy);
                pixel = overGround(p);
                if (reached && reached[c.cell] == walkgrid::kElsewhere && (p >> 24)) pixel = dimmed(pixel);
            }
            row[c.at] = pixel;
        }
    }
}

// 3.0.13-beta.2: through the scene's cache when it has one - the view's square copied out of the pixels made round it
void mapPixels(panel::Picture& out, const panel::Box& box, const Scene& sc, const View& v) {
    MapCache* mc = sc.cache;
    const MapPicture* pic = sc.picture;
    const int bw = int(box.w), bh = int(box.h);
    if (!mc || !pic || pic->levels.empty() || v.cellPx <= 0 || bw <= 0 || bh <= 0 || sc.map.w <= 0 || sc.map.h <= 0) {
        mapPixelsMade(out, box, sc, v);
        return;
    }
    const std::vector<uint8_t>* reach =
        sc.reach && sc.reach->size() == size_t(sc.map.w) * size_t(sc.map.h) ? sc.reach : nullptr;
    const auto offsets = [&](int& ox, int& oy) {
        ox = int(std::lround((v.left - mc->left) * v.cellPx));
        oy = int(std::lround((mc->top - v.top) * v.cellPx));
        return ox >= 0 && oy >= 0 && ox + bw <= mc->w && oy + bh <= mc->h;
    };
    int ox = 0, oy = 0;
    const bool same = mc->level == pic->levels.front().px.data() && mc->cellPx == v.cellPx && mc->mapW == sc.map.w &&
                      mc->mapH == sc.map.h && (reach ? *reach == mc->reach : mc->reach.empty());
    if (!same || !offsets(ox, oy)) {
        const int margin = std::max(8, bw / 3);
        mc->w = bw + 2 * margin;
        mc->h = bh + 2 * margin;
        mc->cellPx = v.cellPx;
        mc->left = v.left - float(margin) / v.cellPx;
        mc->top = v.top + float(margin) / v.cellPx;
        mc->level = pic->levels.front().px.data();
        mc->mapW = sc.map.w;
        mc->mapH = sc.map.h;
        if (reach) mc->reach = *reach;
        else mc->reach.clear();
        panel::Picture made;
        made.width = mc->w;
        made.height = mc->h;
        made.px.assign(size_t(mc->w) * size_t(mc->h), 0u);
        mapPixelsMade(made, panel::Box{0, 0, float(mc->w), float(mc->h)}, sc, View{mc->left, mc->top, v.cellPx});
        mc->px = std::move(made.px);
        ++mc->made;
        offsets(ox, oy);
    }
    for (int y = 0; y < bh; ++y) {
        const int ty = int(box.y) + y;
        if (ty < 0 || ty >= out.height) continue;
        const int x0 = std::max(0, -int(box.x)), x1 = std::min(bw, out.width - int(box.x));
        if (x0 >= x1) continue;
        const uint32_t* from = mc->px.data() + size_t(oy + y) * size_t(mc->w) + size_t(ox + x0);
        std::copy(from, from + (x1 - x0), out.px.data() + size_t(ty) * size_t(out.width) + size_t(int(box.x) + x0));
    }
}

float markRadius(Glyph g, float s) {
    switch (g) {
        case Glyph::Warp: return 5.5f * s;
        case Glyph::Kafra: return 6 * s;
        case Glyph::Member: return 7 * s;
        case Glyph::Spot: return 5 * s;   // 3.0.11-beta.201
        case Glyph::Guild: return 4.2f * s;
        case Glyph::Dot: break;
    }
    return 2.8f * s;
}

// 3.0.11-beta.196: a mark's own - a Kafra with its face as big as a member's
float radiusOf(const Mark& m, float s) {
    return markRadius(m.glyph == Glyph::Kafra && m.picture ? Glyph::Member : m.glyph, s);
}

// 3.0.11-beta.196 (the tester: «відображення нпц перекриває значок кафри»): the marks in the order they are drawn - by
// their kind, the NPCs' dots first, then the warps, the Kafras, the party; the list's order within a kind
std::vector<size_t> drawOrder(const std::vector<Mark>& marks) {
    std::vector<size_t> order(marks.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    const auto rank = [](Glyph g) {  // 3.0.11-beta.201: the guild's under the party's, the server's marks on top
        return g == Glyph::Dot ? 0 : g == Glyph::Warp ? 1 : g == Glyph::Kafra ? 2 : g == Glyph::Guild ? 3
                                                                              : g == Glyph::Member ? 4 : 5;
    };
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return rank(marks[a].glyph) < rank(marks[b].glyph); });
    return order;
}

// 3.0.11-beta.83: a premultiplied picture inside a circle round (cx, cy), over what is there
void faceIn(panel::Picture& out, const panel::Picture& pic, float cx, float cy, float r) {
    const int ox = int(std::floor(cx - float(pic.width) / 2)), oy = int(std::floor(cy - float(pic.height) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            const float k = std::clamp(r + 0.5f - panel::length(float(tx) + 0.5f - cx, float(ty) + 0.5f - cy), 0.0f,
                                       1.0f);
            const uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (k <= 0 || !(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            uint32_t mixed = 0;
            const float keep = 1.0f - float(c >> 24) / 255.0f * k;
            for (int sh = 0; sh <= 24; sh += 8)
                mixed |= uint32_t(std::min(255.0f, float((c >> sh) & 0xFF) * k + float((d >> sh) & 0xFF) * keep)) << sh;
            d = mixed;
        }
}

// 3.0.11-beta.219: a guild mate's mark - a violet triangle pointing up round (x, y), `r` its middle to a side's end
void guildSign(panel::Picture& out, float x, float y, float r, float s) {
    const float k = r * 1.25f, e = 1.3f * s;
    const auto tri = [&](float grow) {
        return std::vector<std::array<float, 6>>{
            {x, y - k - grow * 1.6f, x + k * 0.95f + grow, y + k * 0.62f + grow, x - k * 0.95f - grow,
             y + k * 0.62f + grow}};
    };
    triangles(out, tri(e), kBlack, 0.75f);
    triangles(out, tri(0), kGuildRing, 1.0f);
}

// 3.0.11-beta.119: a layer's sign - as its marks look on the map - at (x, y), `r` across; a layer off dimmed and struck
void layerSign(panel::Picture& out, Layer l, float x, float y, float r, float s, bool on,
               const panel::Picture* kafraFace = nullptr) {
    const float k = on ? 1.0f : 0.35f;
    if (l == Layer::Kafra && kafraFace && kafraFace->width > 0) {  // 3.0.11-beta.201: as its marks - the face
        panel::circle(out, x, y, r + 1.0f, kDark, 1.0f);
        faceIn(out, *kafraFace, x, y, r - 0.8f * s);
        ring(out, x, y, r - 0.4f * s, 1.3f * s, kKafra, 1.0f);
        if (!on) {
            panel::circle(out, x, y, r + 1.0f, kDark, 0.6f);
            segment(out, panel::Box{0, 0, float(out.width), float(out.height)}, x - r, y + r, x + r, y - r, 1.8f * s,
                    kMuted, 0.9f);
        }
        return;
    }
    switch (l) {
        case Layer::Warps:
            panel::circle(out, x, y, r, kPortal, 0.95f * k);
            ring(out, x, y, r - 0.8f * s, 1.5f * s, kPortalRing, k);
            panel::circle(out, x, y, r * 0.3f, kWhite, k);
            break;
        case Layer::Npcs:
            panel::circle(out, x, y, r * 0.62f, 0x37B24D, k);
            break;
        case Layer::Kafra:
            panel::roundRect(out, panel::Box{x - r, y - r, 2 * r, 2 * r}, 3 * s, kKafra, k);
            break;
        case Layer::Party:  // 3.0.11-beta.219: the party's ring and a guild mate's triangle - both on this layer
            panel::circle(out, x - r * 0.28f, y, r * 0.78f, kDark, k);
            ring(out, x - r * 0.28f, y, r * 0.78f - 0.6f * s, 1.4f * s, kMemberRing, k);
            if (on) guildSign(out, x + r * 0.45f, y + r * 0.25f, r * 0.42f, s);
            break;
    }
    if (!on) segment(out, panel::Box{0, 0, float(out.width), float(out.height)}, x - r, y + r, x + r, y - r, 1.8f * s,
                     kMuted, 0.9f);
}

// 3.0.11-beta.120: the scheme's sign - a little floor plan: three light squares in an L over a dark one
void schemeSign(panel::Picture& out, float x, float y, float r, bool on) {
    const float k = on ? 1.0f : 0.7f, q = r * 0.62f, g = r * 0.16f;
    panel::roundRect(out, panel::Box{x - r, y - r, 2 * r, 2 * r}, r * 0.25f, kSchemeWall & 0xFFFFFF, k);
    panel::roundRect(out, panel::Box{x - q - g, y - q - g, q, q}, 1.0f, kSchemeReached & 0xFFFFFF, k);
    panel::roundRect(out, panel::Box{x + g, y - q - g, q, q}, 1.0f, kSchemeReached & 0xFFFFFF, k);
    panel::roundRect(out, panel::Box{x - q - g, y + g, q, q}, 1.0f, kSchemeFloor & 0xFFFFFF, k);
}

// The marks, the way, its pin and the player over the map's picture in `box` (a view's pixels from its corner)
void overMap(panel::Picture& out, const panel::Box& box, const Scene& sc, const View& v, float s, float big,
             float marks = 1.0f) {
    const auto place = [&](const Cell& c, float& x, float& y) {
        cellToPanel(v, sc.map, c, x, y);
        x += box.x;
        y += box.y;
    };
    const auto inBox = [&](float x, float y, float r) {
        return x >= box.x - r && y >= box.y - r && x <= box.x + box.w + r && y <= box.y + box.h + r;
    };
    float x = 0, y = 0;
    for (const size_t i : drawOrder(sc.marks)) {
        const Mark& m = sc.marks[i];
        place(m.cell, x, y);
        float r = radiusOf(m, s) * big * marks;  // 3.0.11-beta.201: the minimap's by its zoom
        if (!m.letter.empty() && m.glyph == Glyph::Dot && big > 1) r *= 1.7f;  // 3.0.18-beta.240: room for «$»
        if (!inBox(x, y, r)) continue;
        if (int(i) == sc.hoverMark) ring(out, x, y, r + 3 * s, 1.6f * s, kWhite, 0.95f);
        switch (m.glyph) {
            case Glyph::Warp:  // a portal: a dark disc, its blue ring, a white heart
                panel::circle(out, x, y, r + 1, kBlack, 0.6f);
                panel::circle(out, x, y, r, kPortal, 0.95f);
                ring(out, x, y, r - 0.8f * s, 1.5f * s, kPortalRing, 1.0f);
                panel::circle(out, x, y, r * 0.3f, kWhite, 1.0f);
                break;
            case Glyph::Kafra:
                if (m.picture && m.picture->width > 0) {  // 3.0.11-beta.196: its face in a round, a pink ring
                    panel::circle(out, x, y, r + 1.5f, kBlack, 0.7f);
                    panel::circle(out, x, y, r, kDark, 1.0f);
                    faceIn(out, *m.picture, x, y, r - 1.2f * s);
                    ring(out, x, y, r - 0.6f * s, 1.6f * s, kKafra, 1.0f);
                } else {  // the concept's pink «K» (its letter over it by GDI)
                    panel::roundRect(out, panel::Box{x - r, y - r, 2 * r, 2 * r}, 3 * s * big, kKafra, 1.0f, kWhite,
                                     0.9f, std::max(1.0f, s));
                }
                break;
            case Glyph::Dot:
                panel::circle(out, x, y, r + 1.2f, kBlack, 0.8f);
                panel::circle(out, x, y, r, m.rgb, 1.0f);
                break;
            case Glyph::Spot:  // 3.0.11-beta.201: a server's mark - its colour round a white middle, a dark edge
                panel::circle(out, x, y, r + 1.5f, kBlack, 0.75f);
                panel::circle(out, x, y, r, m.rgb, 1.0f);
                panel::circle(out, x, y, r * 0.32f, kWhite, 1.0f);
                break;
            case Glyph::Guild:  // 3.0.11-beta.219: a guild mate - a pink triangle pointing up, a dark edge
                if (sc.classicMarks) {  // 3.0.11-beta.222: a round dot, as the game drew it
                    panel::circle(out, x, y, r + 1.2f, kBlack, 0.8f);
                    panel::circle(out, x, y, r, kGuildRing, 1.0f);
                } else {
                    guildSign(out, x, y, r, s);
                }
                break;
            case Glyph::Member:  // 3.0.11-beta.83: its face in a dark round, a blue ring (a dead one's red)
                if (sc.classicMarks) {  // 3.0.11-beta.222: a round dot, as the game drew it
                    panel::circle(out, x, y, r * 0.6f + 1.2f, kBlack, 0.8f);
                    panel::circle(out, x, y, r * 0.6f, m.dead ? kDeadRing : kMemberRing, 1.0f);
                    break;
                }
                panel::circle(out, x, y, r + 1.5f, kBlack, 0.7f);
                panel::circle(out, x, y, r, kDark, 1.0f);
                if (m.picture && m.picture->width > 0) faceIn(out, *m.picture, x, y, r - 1.2f * s);
                ring(out, x, y, r - 0.6f * s, 1.6f * s, m.dead ? kDeadRing : kMemberRing, 1.0f);
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
    for (const Cell& w : sc.waypoints) {  // 3.0.11-beta.71: the points set on the way - gold dots
        place(w, x, y);
        if (!inBox(x, y, 0)) continue;
        panel::circle(out, x, y, 4.2f * s * big + 1.2f, kBlack, 0.6f);
        panel::circle(out, x, y, 4.2f * s * big, kRouteRgb, 1.0f);
    }
    if (sc.pin) {
        place(sc.pinCell, x, y);
        if (inBox(x, y, 0)) pin(out, x, y, 5 * s * big);
    }
    place(sc.player, x, y);
    if (sc.showPlayer && inBox(x, y, 0))
        player(out, box, x, y, sc.viewFan ? sc.lookX : 0.0f, sc.viewFan ? sc.lookY : 0.0f,
               sc.viewFan || std::hypot(sc.headX, sc.headY) > 1e-4f ? sc.headX : sc.lookX,
               sc.viewFan || std::hypot(sc.headX, sc.headY) > 1e-4f ? sc.headY : sc.lookY, 9.5f * s * big);
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

bool facingOf(float degrees, float& east, float& north) {
    if (!std::isfinite(degrees) || std::fabs(degrees) > 3600) return false;
    const float r = degrees * 3.14159265f / 180.0f;
    east = std::sin(r);
    north = -std::cos(r);
    return true;
}

namespace {

bool cellUnder(const View& v, const MapSize& map, float px, float py, Cell& out) {
    if (v.cellPx <= 0 || map.w <= 0 || map.h <= 0) return false;
    const int x = int(std::floor(v.left + px / v.cellPx)), y = int(std::floor(v.top - py / v.cellPx));
    if (x < 0 || y < 0 || x >= map.w || y >= map.h) return false;
    out = Cell{x, y};
    return true;
}

}  // namespace

float markScale(int cells, const MapSize& map, float partScale) {
    const int whole = std::max(1, std::max(map.w, map.h));
    const float t = cells >= whole ? 1.0f  // the whole map: as big as they get
                                   : std::clamp((std::log(float(std::max(cells, 1))) - std::log(40.0f)) /
                                                    (std::log(300.0f) - std::log(40.0f)),
                                                0.0f, 1.0f);
    const float plain = 0.85f + 0.45f * t;
    const float grow = std::clamp(1.0f / std::max(partScale, 0.01f), 1.0f, 2.0f);  // .262: a small window's more
    return std::max(0.5f, 1.0f + (plain - 1.0f) * grow);
}

int cellsAt(int step, const MapSize& map) {
    step = std::clamp(step, 0, kZoomSteps - 1);
    const int whole = std::max(1, std::max(map.w, map.h));
    if (step == kZoomSteps - 1) return whole;
    return std::min(kZoomCells[step], whole);  // 3.0.11-beta.187: never farther out than the whole map
}

int zoomStep(int from, int by, const MapSize& map) {
    const int last = kZoomSteps - 1, whole = std::max(1, std::max(map.w, map.h));
    const auto own = [&](int step) { return step == last || kZoomCells[step] < whole; };  // not the whole map again
    int at = std::clamp(from, 0, last);
    if (!own(at)) at = last;
    for (int step = at + (by < 0 ? -1 : 1); by != 0 && step >= 0 && step <= last; step += by < 0 ? -1 : 1)
        if (own(step)) return step;
    return at;
}

std::vector<uint32_t> schemePixels(const walkgrid::Grid& g, const std::vector<uint8_t>* reach, int px, int& width,
                                   int& height) {
    px = std::max(1, px);
    width = g.w * px;
    height = g.h * px;
    std::vector<uint32_t> out(size_t(width) * size_t(height), kSchemeWall);
    const auto floor = [&](int x, int y) { return g.at(x, y) != walkgrid::kWall; };
    const bool lit = reach && reach->size() == size_t(g.w) * size_t(g.h);
    for (int y = 0; y < g.h; ++y)
        for (int x = 0; x < g.w; ++x) {
            if (!floor(x, y)) continue;
            const uint32_t base = lit && (*reach)[size_t(y) * size_t(g.w) + size_t(x)] ? kSchemeReached : kSchemeFloor;
            const bool west = !floor(x - 1, y), east = !floor(x + 1, y);
            const bool north = !floor(x, y + 1), south = !floor(x, y - 1);
            const int top = (g.h - 1 - y) * px, left = x * px;  // north first
            for (int r = 0; r < px; ++r) {
                uint32_t* row = out.data() + size_t(top + r) * size_t(width) + size_t(left);
                for (int c = 0; c < px; ++c) {
                    const bool rim = (west && c == 0) || (east && c == px - 1) || (north && r == 0) ||
                                     (south && r == px - 1);
                    row[c] = rim && px > 1 ? kSchemeEdge : base;
                }
            }
        }
    return out;
}

Layout layout(int targetW, int targetH, float scale, bool route, int listLines, bool net, bool found, float grow) {
    Layout L;
    const float s = std::max(0.5f, scale);
    L.s = s;
    L.grow = std::clamp(grow, 1.0f, 3.0f);
    const float sb = s * L.grow;  // the buttons' scale
    const float side = std::round(kSide * s), gap = std::round(kGap * s), b = std::round(kButton * sb);
    L.w = int(side);
    L.map = panel::Box{0, 0, side, side};
    const float inset = std::round(6 * sb), bx = inset;  // the buttons: a column in the square's bottom left corner
    L.expand = panel::Box{bx, side - inset - b, b, b};
    L.zoomOut = panel::Box{bx, L.expand.y - std::round(kButtonGap * sb) - b, b, b};
    L.zoomIn = panel::Box{bx, L.zoomOut.y - std::round(kButtonGap * sb) - b, b, b};
    for (int i = 0; i < kLayers; ++i)  // 3.0.11-beta.119: the layers' row in the top left corner
        L.layers[i] = panel::Box{bx + float(i) * (b + std::round(kButtonGap * sb)), inset, b, b};
    L.style = panel::Box{bx + float(kLayers) * (b + std::round(kButtonGap * sb)) + std::round(4 * sb), inset, b, b};
    float y = side + gap;
    const float bar = std::round(kBarH * s);
    L.caption = panel::Box{0, y, side, bar};
    L.fold = panel::Box{side - std::round(26 * s), y + std::round(4 * s), std::round(22 * s), bar - std::round(8 * s)};
    y += bar;
    if (net) {  // 3.0.11-beta.127: the round trip's plate, its own under the bar
        y += std::round(4 * s);
        const float w = std::round((kNetPad + kNetText + 6 + kNetGraphW + kNetPad) * s);
        L.net = panel::Box{0, y, w, std::round(kNetH * s)};
        y += L.net.h;
    }
    if (found) {  // 3.0.11-beta.213: a search's plate, as wide as the map
        y += std::round(4 * s);
        const float h = std::round(kNetH * s);
        L.found = panel::Box{0, y, side, h};
        L.foundClear = panel::Box{side - std::round(26 * s), y + std::round(2 * s), std::round(22 * s),
                                  h - std::round(4 * s)};
        y += h;
    }
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

panel::Box netGraphBox(const Layout& L) {
    if (L.net.w <= 0) return panel::Box{};
    const float s = L.s, w = std::round(kNetGraphW * s), h = std::round(kNetGraphH * s);
    return panel::Box{L.net.x + L.net.w - std::round(kNetPad * s) - w, L.net.y + std::round((L.net.h - h) / 2), w, h};
}

int layerAt(const Layout& L, float x, float y) {
    for (int i = 0; i < kLayers; ++i)
        if (L.layers[i].w > 0 && L.layers[i].contains(x, y)) return i;
    return -1;
}

Part partAt(const Layout& L, float x, float y) {
    if (L.foundClear.w > 0 && L.foundClear.contains(x, y)) return Part::FoundClear;  // 3.0.11-beta.213
    if (L.found.w > 0 && L.found.contains(x, y)) return Part::Found;
    if (layerAt(L, x, y) >= 0) return Part::Layer;  // 3.0.11-beta.119: over the map's square
    if (L.style.w > 0 && L.style.contains(x, y)) return Part::Style;  // 3.0.11-beta.120
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

std::vector<Words> mapWords(const Layout& L, const Scene& sc) {
    std::vector<Words> out;
    const float s = L.s;
    const View v = view(sc.map, sc.player, sc.cells, int(L.map.w));
    for (const Mark& m : sc.marks) {  // the Kafras' «K» (3.0.11-beta.196: not over a face)
        if (m.glyph != Glyph::Kafra || m.picture) continue;
        float x = 0, y = 0;
        cellToPanel(v, sc.map, m.cell, x, y);
        const float r = markRadius(m.glyph, s) * L.grow;  // 3.0.18-beta.51: as its mark
        if (x - r < 0 || y - r < 0 || x + r > L.map.w || y + r > L.map.h) continue;
        out.push_back(Words{panel::Box{x - r, y - r, 2 * r, 2 * r}, "K", kWhite, 9 * s * L.grow, true, Align::Center});
    }
    return out;
}

std::vector<Words> frameWords(const Layout& L, const Scene& sc, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    const panel::Box& kafra = L.layers[int(Layer::Kafra)];  // 3.0.11-beta.119: the Kafras' button's «K»
    if (kafra.w > 0 && !sc.kafraFace)  // 3.0.11-beta.201: not over the face
        out.push_back(Words{kafra, "K", sc.layers.on[int(Layer::Kafra)] ? kWhite : kMuted, 8 * s, true,
                            Align::Center});
    const panel::Box& c = L.caption;
    const bool hovered = sc.hoverMark >= 0 && size_t(sc.hoverMark) < sc.marks.size();
    const bool layer = sc.hover == Part::Layer && sc.hoverLayer >= 0 && sc.hoverLayer < kLayers;
    if (sc.hover == Part::Style && !t.style.empty()) {  // 3.0.11-beta.120: the picture's switch names itself
        out.push_back(Words{panel::Box{c.x + 10 * s, c.y, L.fold.x - 4 * s - c.x - 10 * s, c.h}, t.style,
                            sc.scheme ? kAccent2 : kMuted, 12.5f * s, true, Align::Left});
    } else if (layer) {  // 3.0.11-beta.119: a layer's button pointed at names its layer on the bar
        out.push_back(Words{panel::Box{c.x + 10 * s, c.y, L.fold.x - 4 * s - c.x - 10 * s, c.h},
                            t.layers[sc.hoverLayer], sc.layers.on[sc.hoverLayer] ? kAccent2 : kMuted, 12.5f * s, true,
                            Align::Left});
    } else if (hovered) {  // the mark under the pointer names itself on the bar
        out.push_back(Words{panel::Box{c.x + 10 * s, c.y, L.fold.x - 4 * s - c.x - 10 * s, c.h},
                            sc.marks[size_t(sc.hoverMark)].label, kAccent2, 12.5f * s, true, Align::Left});
    } else {
        const float cellW = std::round(62 * s);
        out.push_back(Words{panel::Box{c.x + 10 * s, c.y, L.fold.x - 6 * s - cellW - c.x - 10 * s, c.h}, t.name,
                            kText, 13 * s, true, Align::Left});
        out.push_back(Words{panel::Box{L.fold.x - 4 * s - cellW, c.y, cellW, c.h}, t.cell, kMuted, 12 * s, false,
                            Align::Right});
    }
    out.push_back(Words{L.fold, t.folded ? "▸" : "▾", sc.hover == Part::Fold ? kText : kMuted, 12 * s, false,
                        Align::Center});
    if (L.found.w > 0 && !t.found.empty()) {  // 3.0.11-beta.213: a search's words and its ✕
        out.push_back(Words{panel::Box{L.found.x + std::round(kNetPad * s), L.found.y,
                                       L.foundClear.x - L.found.x - std::round(kNetPad * s), L.found.h},
                            "\xE2\x97\x86 " + t.found, kFoundRgb, 11.5f * s, true, Align::Left});  // ◆
        out.push_back(Words{L.foundClear, "\xE2\x9C\x95", kText, 11 * s, false, Align::Center});  // ✕
    }
    if (L.net.w > 0 && !t.net.empty())  // 3.0.11-beta.127: the round trip on its plate (.122: on the bar)
        out.push_back(Words{panel::Box{L.net.x + std::round(kNetPad * s), L.net.y, std::round(kNetText * s), L.net.h},
                            "\xE2\x97\x8F " + t.net, t.netRgb ? t.netRgb : kMuted, 11.5f * s, true, Align::Left});
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
        const float reach =  // a hand more
            radiusOf(sc.marks[i], L.s) * markScale(sc.cells, sc.map, L.s) * L.grow + 4 * L.s;
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
        const float r = radiusOf(m, s) * 1.5f, w = guess(m.label) + 14 * s, h = 22 * s;
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
    // 3.0.18-beta.240 (the owner 13.11: «подписи только у выходов»): the exits' words always (a Kafra its «K»), the
    // rest's under the pointer
    for (size_t i = 0; i < sc.marks.size(); ++i)
        if (sc.marks[i].glyph == Glyph::Warp && sc.marks[i].exit) add(i, false);
    if (sc.hoverMark >= 0 && size_t(sc.hoverMark) < sc.marks.size()) add(size_t(sc.hoverMark), true);
    return out;
}

}  // namespace

bool cellAt(const Layout& L, const Scene& sc, float x, float y, Cell& out) {
    if (!L.map.contains(x, y)) return false;
    return cellUnder(view(sc.map, sc.player, sc.cells, int(L.map.w)), sc.map, x - L.map.x, y - L.map.y, out);
}

bool bigCellAt(const Big& b, const Scene& sc, float x, float y, Cell& out) {
    if (!b.map.contains(x, y)) return false;
    return cellUnder(bigView(b, sc), sc.map, x - b.map.x, y - b.map.y, out);
}

Big bigLayout(int targetW, int targetH, float scale, const MapSize& map, const float* chipWidths, float styleWidth) {
    Big b;
    const float s = std::max(0.5f, scale);
    b.s = s;
    const float margin = kBigMargin * s, header = std::round(kBigHeader * s), pad = std::round(kBigPad * s);
    const float footer = chipWidths ? std::round(kBigFooter * s) : 0;  // 3.0.11-beta.119: the layers' chips
    const float roomW = float(targetW) - 2 * margin - 2 * pad;
    const float roomH = float(targetH) - 2 * margin - header - pad - footer;
    const float cell = std::max(0.1f, std::min(roomW / float(std::max(1, map.w)), roomH / float(std::max(1, map.h))));
    const float mapW = std::round(float(std::max(1, map.w)) * cell);
    const float mapH = std::round(float(std::max(1, map.h)) * cell);
    b.w = int(mapW + 2 * pad);
    b.h = int(header + mapH + (chipWidths ? footer : pad));
    b.x = std::max(0, (targetW - b.w) / 2);
    b.y = std::max(0, (targetH - b.h) / 2);
    b.header = panel::Box{0, 0, float(b.w), header};
    b.close = panel::Box{float(b.w) - std::round(10 * s) - std::round(32 * s), (header - std::round(32 * s)) / 2,
                         std::round(32 * s), std::round(32 * s)};
    b.map = panel::Box{pad, header, mapW, mapH};
    if (chipWidths) {  // a row in the middle under the map
        const float gap = std::round(8 * s), h = std::round(28 * s);
        float total = gap * float(kLayers - 1) + (styleWidth > 0 ? styleWidth + 2 * gap : 0);
        for (int i = 0; i < kLayers; ++i) total += chipWidths[i];
        float x = std::max(pad, (float(b.w) - total) / 2);
        const float y = header + mapH + (footer - h) / 2;
        for (int i = 0; i < kLayers; ++i) {
            b.chips[i] = panel::Box{x, y, chipWidths[i], h};
            x += chipWidths[i] + gap;
        }
        if (styleWidth > 0) b.styleChip = panel::Box{x + gap, y, styleWidth, h};  // 3.0.11-beta.120: a gap more
    }
    return b;
}

int bigLayerAt(const Big& b, float x, float y) {
    for (int i = 0; i < kLayers; ++i)
        if (b.chips[i].w > 0 && b.chips[i].contains(x, y)) return i;
    return -1;
}

BigPart bigPartAt(const Big& b, float x, float y) {
    if (b.close.contains(x, y)) return BigPart::Close;
    if (bigLayerAt(b, x, y) >= 0) return BigPart::Layer;  // 3.0.11-beta.119
    if (b.styleChip.w > 0 && b.styleChip.contains(x, y)) return BigPart::Style;  // 3.0.11-beta.120
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

std::vector<Words> bigWords(const Big& b, const Scene& sc, const std::string& title, const std::string& cell,
                            const std::string* layerNames, const std::string& styleName) {
    std::vector<Words> out;
    const float s = b.s;
    const float cellW = std::round(110 * s);
    out.push_back(Words{panel::Box{16 * s, 0, b.close.x - 16 * s - cellW - 24 * s, b.header.h}, title, kText, 16 * s,
                        true, Align::Left});
    out.push_back(Words{panel::Box{b.close.x - 12 * s - cellW, 0, cellW, b.header.h}, cell, kMuted, 13 * s, false,
                        Align::Right});
    out.push_back(Words{b.close, "✕", sc.bigHover == BigPart::Close ? kText : kMuted, 16 * s, false, Align::Center});
    const View v = bigView(b, sc);
    for (const Mark& m : sc.marks) {  // the Kafras' «K» (3.0.11-beta.196: not over a face); .240: «$», «!»
        const bool kafra = m.glyph == Glyph::Kafra && !m.picture;
        if (!kafra && (m.letter.empty() || m.glyph != Glyph::Dot)) continue;
        float x = 0, y = 0;
        cellToPanel(v, sc.map, m.cell, x, y);
        const float r = markRadius(m.glyph, s) * 1.5f * (kafra ? 1.0f : 1.7f);
        out.push_back(Words{panel::Box{b.map.x + x - r, b.map.y + y - r, 2 * r, 2 * r}, kafra ? "K" : m.letter, kWhite,
                            (kafra ? 12 : 11) * s, true, Align::Center});
    }
    for (const Label& l : bigLabels(b, sc))
        out.push_back(Words{l.box, l.text, l.rgb, 12.5f * s, true, Align::Center});
    for (int i = 0; layerNames && i < kLayers; ++i) {  // 3.0.11-beta.119: the layers' chips
        const panel::Box& c = b.chips[i];
        if (c.w <= 0) continue;
        const bool on = sc.layers.on[i];
        out.push_back(Words{panel::Box{c.x + c.h, c.y, c.w - c.h - 8 * s, c.h}, layerNames[i], on ? kText : kMuted,
                            12 * s, true, Align::Left});
        if (i == int(Layer::Kafra) && !sc.kafraFace)  // 3.0.11-beta.201: not over the face
            out.push_back(Words{panel::Box{c.x + c.h / 2 - 6 * s, c.y + c.h / 2 - 6 * s, 12 * s, 12 * s}, "K",
                                on ? kWhite : kMuted, 9 * s, true, Align::Center});
    }
    if (b.styleChip.w > 0 && !styleName.empty()) {  // 3.0.11-beta.120
        const panel::Box& c = b.styleChip;
        out.push_back(Words{panel::Box{c.x + c.h, c.y, c.w - c.h - 8 * s, c.h}, styleName, sc.scheme ? kText : kMuted,
                            12 * s, true, Align::Left});
    }
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
        const float reach = radiusOf(sc.marks[i], b.s) * 1.5f + 5 * b.s;
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


void paintMapSquare(panel::Picture& out, const Layout& L, const Scene& sc) {
    const int w = int(L.map.w), h = int(L.map.h);
    panel::blank(out, w, h);
    const panel::Box box{0, 0, float(w), float(h)};
    const View v = view(sc.map, sc.player, sc.cells, w);
    mapPixels(out, box, sc, v);
    overMap(out, box, sc, v, L.s, 1.0f, markScale(sc.cells, sc.map, L.s) * L.grow);
    roundCorners(out, box, kRadius * L.s);
}

std::vector<NetBar> netBars(const Layout& L, const Scene& sc) {
    std::vector<NetBar> out;
    const panel::Box g = netGraphBox(L);
    if (g.w <= 0 || !sc.netGraph || sc.netGraph->empty()) return out;
    const float s = L.s;
    const std::vector<int>& v = *sc.netGraph;
    int top = 150;  // ms at the graph's top: 150 or the highest
    for (int ms : v) top = std::max(top, ms);
    const float bar = g.w / kNetGraphW;
    const size_t n = std::min(v.size(), size_t(kNetGraphW));
    for (size_t i = 0; i < n; ++i) {
        const int ms = v[v.size() - n + i];
        const float x = g.x + g.w - float(n - i) * bar;
        const float h = ms < 0 ? g.h : std::max(std::max(1.0f, std::round(s)), g.h * float(ms) / float(top));
        const uint32_t rgb = ms < 0 ? kNetStall : ms < 120 ? kNetGood : ms < 250 ? kNetFair : kNetBad;
        out.push_back(NetBar{panel::Box{x, g.y + g.h - h, bar, h}, rgb});
    }
    return out;
}

void paintLayerSign(panel::Picture& out, Layer l, bool on, const panel::Picture* kafraFace, float s) {
    layerSign(out, l, float(out.width) / 2, float(out.height) / 2, 5.5f * s, s, on, kafraFace);
}

void paintSchemeSign(panel::Picture& out) {
    schemeSign(out, float(out.width) / 2, float(out.height) / 2, 5.5f * float(out.width) / 22, true);
}

void paintBigSquare(panel::Picture& out, const panel::Picture& map, const Big& b, const Scene& sc) {
    const int w = int(b.map.w), h = int(b.map.h);
    panel::blank(out, w, h);
    if (map.width == w && map.height == h) out.px = map.px;
    const panel::Box box{0, 0, float(w), float(h)};
    overMap(out, box, sc, bigView(b, sc), b.s, 1.5f);
    for (const Label& l : bigLabels(b, sc))
        panel::roundRect(out, panel::Box{l.box.x - b.map.x, l.box.y - b.map.y, l.box.w, l.box.h}, 6 * b.s, kDark,
                         0.84f);
}


// 3.0.18-beta.240 (the reference's «одна карта»): the map at any box, zoom and centre
View viewOf(const MapView& m, const MapSize& map) {
    View v;
    if (map.w <= 0 || map.h <= 0 || m.w <= 0 || m.h <= 0) return v;
    const float fit = std::min(float(m.w) / float(map.w), float(m.h) / float(map.h));
    v.cellPx = fit * std::max(1.0f, m.zoom);
    const float halfW = float(m.w) / 2 / v.cellPx, halfH = float(m.h) / 2 / v.cellPx;
    float cx = m.cx < 0 ? float(map.w) / 2 : m.cx, cy = m.cy < 0 ? float(map.h) / 2 : m.cy;
    // kept on the map: a side longer than the view's - its edges inside the view's; a shorter one in its middle
    cx = float(map.w) <= 2 * halfW ? float(map.w) / 2 : std::clamp(cx, halfW, float(map.w) - halfW);
    cy = float(map.h) <= 2 * halfH ? float(map.h) / 2 : std::clamp(cy, halfH, float(map.h) - halfH);
    v.left = cx - halfW;
    v.top = cy + halfH;
    return v;
}

float mapViewMost(bool sharpPicture) { return sharpPicture ? 4.0f : 2.0f; }

namespace {

void keepCentre(MapView& m, const MapSize& map) {  // the centre as the view keeps it (no drifting past the edges)
    if (m.zoom <= 1.0f) {
        m.cx = m.cy = -1;
        return;
    }
    const View v = viewOf(m, map);
    if (v.cellPx <= 0) return;
    m.cx = v.left + float(m.w) / 2 / v.cellPx;
    m.cy = v.top - float(m.h) / 2 / v.cellPx;
}

// The words beside the marks that always have them (the exits) and the one under the pointer, as the big map's
std::vector<Label> viewLabels(const MapView& m, const Scene& sc) {
    std::vector<Label> out;
    const float s = m.s;
    const View v = viewOf(m, sc.map);
    if (v.cellPx <= 0) return out;
    const auto guess = [&](const std::string& t) {
        float w = 0;
        for (size_t i = 0; i < t.size(); ++i)
            if ((static_cast<unsigned char>(t[i]) & 0xC0) != 0x80) w += 7.6f * s;
        return w;
    };
    const auto add = [&](size_t i, bool force) {
        const Mark& mk = sc.marks[i];
        if (mk.label.empty()) return;
        float x = 0, y = 0;
        cellToPanel(v, sc.map, mk.cell, x, y);
        if (x < 0 || y < 0 || x > float(m.w) || y > float(m.h)) return;
        const float r = radiusOf(mk, s) * 1.5f, w = guess(mk.label) + 14 * s, h = 22 * s;
        panel::Box box{x + r + 4 * s, y - h / 2, w, h};
        if (box.x + box.w > float(m.w)) box.x = x - r - 4 * s - w;  // past the right edge: on the left
        box.x = std::clamp(box.x, 0.0f, std::max(0.0f, float(m.w) - box.w));
        box.y = std::clamp(box.y, 0.0f, std::max(0.0f, float(m.h) - box.h));
        for (const Label& o : out)
            if (!force && box.x < o.box.x + o.box.w && o.box.x < box.x + box.w && box.y < o.box.y + o.box.h &&
                o.box.y < box.y + box.h)
                return;
        out.push_back(Label{box, mk.label, mk.glyph == Glyph::Warp ? 0xC7CEFF : kText});
    };
    for (size_t i = 0; i < sc.marks.size(); ++i)
        if (sc.marks[i].glyph == Glyph::Warp && sc.marks[i].exit) add(i, false);
    if (sc.hoverMark >= 0 && size_t(sc.hoverMark) < sc.marks.size()) add(size_t(sc.hoverMark), true);
    return out;
}

}  // namespace

void zoomMapView(MapView& m, const MapSize& map, float px, float py, float by, float most) {
    const View before = viewOf(m, map);
    if (before.cellPx <= 0) return;
    const float cellX = before.left + px / before.cellPx, cellY = before.top - py / before.cellPx;
    m.zoom = std::clamp(std::max(1.0f, m.zoom) * by, 1.0f, std::max(1.0f, most));
    MapView fitted = m;
    fitted.cx = fitted.cy = -1;
    const View after = viewOf(fitted, map);
    m.cx = cellX - (px - float(m.w) / 2) / after.cellPx;
    m.cy = cellY + (py - float(m.h) / 2) / after.cellPx;
    keepCentre(m, map);
}

void panMapView(MapView& m, const MapSize& map, float dx, float dy) {
    const View v = viewOf(m, map);
    if (v.cellPx <= 0 || m.zoom <= 1.0f) return;
    m.cx = v.left + float(m.w) / 2 / v.cellPx - dx / v.cellPx;
    m.cy = v.top - float(m.h) / 2 / v.cellPx + dy / v.cellPx;
    keepCentre(m, map);
}

void paintMapView(panel::Picture& out, const MapView& m, const Scene& sc) {
    panel::blank(out, std::max(1, m.w), std::max(1, m.h));
    const panel::Box box{0, 0, float(m.w), float(m.h)};
    const View v = viewOf(m, sc.map);
    if (v.cellPx <= 0) return;
    mapPixels(out, box, sc, v);
    for (const ZoneMark& z : sc.zones) {  // a monster zone: a soft red area with its edge
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        cellToPanel(v, sc.map, Cell{z.cell.x - z.rx, z.cell.y + z.ry}, x0, y0);
        cellToPanel(v, sc.map, Cell{z.cell.x + z.rx, z.cell.y - z.ry}, x1, y1);
        const panel::Box zb{x0, y0, std::max(4.0f, x1 - x0), std::max(4.0f, y1 - y0)};
        panel::roundRect(out, zb, std::min(zb.w, zb.h) / 2, 0xE05A4E, 0.16f, 0xE05A4E, 0.55f,
                         std::max(1.0f, 1.2f * m.s));
    }
    overMap(out, box, sc, v, m.s, 1.5f);
    for (const Label& l : viewLabels(m, sc)) panel::roundRect(out, l.box, 6 * m.s, kDark, 0.84f);
}

std::vector<Words> mapViewWords(const MapView& m, const Scene& sc) {
    std::vector<Words> out;
    const View v = viewOf(m, sc.map);
    if (v.cellPx <= 0) return out;
    for (const Mark& mk : sc.marks) {  // the letters: a Kafra's «K» (not over a face), «$», «!» in their dots
        const std::string letter = mk.glyph == Glyph::Kafra && !mk.picture ? std::string("K") : mk.letter;
        if (letter.empty() || (mk.glyph == Glyph::Kafra && mk.picture)) continue;
        float x = 0, y = 0;
        cellToPanel(v, sc.map, mk.cell, x, y);
        if (x < 0 || y < 0 || x > float(m.w) || y > float(m.h)) continue;
        const float r = radiusOf(mk, m.s) * 1.5f * (mk.glyph == Glyph::Dot ? 1.7f : 1.0f);
        out.push_back(Words{panel::Box{x - r, y - r, 2 * r, 2 * r}, letter, kWhite, 11 * m.s, true, Align::Center});
    }
    for (const Label& l : viewLabels(m, sc))
        out.push_back(Words{l.box, l.text, l.rgb, 12.5f * m.s, true, Align::Center});
    return out;
}

int mapViewMarkAt(const MapView& m, const Scene& sc, float x, float y) {
    const View v = viewOf(m, sc.map);
    if (v.cellPx <= 0) return -1;
    int best = -1;
    float bestD = 0;
    for (size_t i = 0; i < sc.marks.size(); ++i) {
        float px = 0, py = 0;
        cellToPanel(v, sc.map, sc.marks[i].cell, px, py);
        const float reach = std::max(radiusOf(sc.marks[i], m.s) * 1.5f + 5 * m.s, 12 * m.s);  // the owner's 24 px
        const float d = (px - x) * (px - x) + (py - y) * (py - y);
        if (d <= reach * reach && (best < 0 || d < bestD)) {
            best = int(i);
            bestD = d;
        }
    }
    return best;
}

bool mapViewCellAt(const MapView& m, const Scene& sc, float x, float y, Cell& out) {
    if (x < 0 || y < 0 || x >= float(m.w) || y >= float(m.h)) return false;
    return cellUnder(viewOf(m, sc.map), sc.map, x, y, out);
}

}  // namespace uagfx::minimap
