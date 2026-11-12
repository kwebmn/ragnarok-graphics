#include "core/sharp_specks.hpp"

#include <algorithm>
#include <cmath>

#include "core/full_colour.hpp"

namespace uagfx::sharpspecks {
namespace {

constexpr float kLit = 0.05f;   // a dot's texel: its light from here
constexpr float kDark = 0.04f;  // the ground's: under it
constexpr float kTop = 0.08f;   // a dot's brightest texel at least
constexpr float kNeck = 0.75f;  // two tops one dot while the way between them stays this bright (of the dimmer one)
constexpr int kFewest = 8, kLongest = 512, kMostSide = 1024;
constexpr size_t kMostParts = 4096;
constexpr float kMean = 0.85f;  // the drawn dot's light over its ellipse: 1 - 0.3 d^2 on average
constexpr float kPi = 3.14159265f;

float lightOf(uint32_t p) {
    const uint32_t most = std::max({(p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF});
    return float(most) / 255.0f * float(p >> 24) / 255.0f;
}

struct Sum {
    double e = 0, x = 0, y = 0, xx = 0, xy = 0, yy = 0, r = 0, g = 0, b = 0;
    float peak = 0;
    void add(float light, float cx, float cy, uint32_t p) {
        e += light;
        x += light * cx, y += light * cy;
        xx += light * cx * cx, xy += light * cx * cy, yy += light * cy * cy;
        r += light * double((p >> 16) & 0xFF), g += light * double((p >> 8) & 0xFF), b += light * double(p & 0xFF);
        peak = std::max(peak, light);
    }
    Speck speck() const {
        Speck s;
        if (e <= 0) return s;
        s.x = float(x / e), s.y = float(y / e);
        s.sxx = float(xx / e - double(s.x) * s.x) + 1.0f / 12, s.syy = float(yy / e - double(s.y) * s.y) + 1.0f / 12;
        s.sxy = float(xy / e - double(s.x) * s.y);
        s.energy = float(e), s.peak = peak;
        const double most = std::max({r, g, b});
        if (most > 0) s.r = float(r / most), s.g = float(g / most), s.b = float(b / most);
        return s;
    }
};

// The dimmest light on the straight way between two texels (their middles), each half texel
float neck(const std::vector<float>& light, int w, int a, int b) {
    const float ax = float(a % w) + 0.5f, ay = float(a / w) + 0.5f, bx = float(b % w) + 0.5f, by = float(b / w) + 0.5f;
    const int steps = std::max(1, int(std::ceil(std::hypot(bx - ax, by - ay) * 2)));
    float least = 1e9f;
    for (int i = 0; i <= steps; ++i) {
        const float t = float(i) / float(steps);
        const int x = int(ax + (bx - ax) * t), y = int(ay + (by - ay) * t);
        least = std::min(least, light[size_t(y) * size_t(w) + size_t(x)]);
    }
    return least;
}

}  // namespace

std::vector<Speck> specksOf(const uint32_t* argb, int w, int h) {
    std::vector<Speck> out;
    if (!argb || w <= 0 || h <= 0 || w > kMostSide || h > kMostSide) return out;
    const size_t n = size_t(w) * size_t(h);
    std::vector<float> light(n);
    size_t dark = 0;
    for (size_t i = 0; i < n; ++i) {
        light[i] = lightOf(argb[i]);
        if (light[i] < kDark) ++dark;
    }
    if (dark * 10 < n * 6) return out;  // the ground not dark: no sheet of dots
    // the dots: the lit texels' 8-connected parts
    std::vector<int> part(n, -1);
    std::vector<std::vector<int>> parts;
    std::vector<int> stack;
    for (size_t i = 0; i < n; ++i) {
        if (part[i] >= 0 || light[i] < kLit) continue;
        if (parts.size() >= kMostParts) return {};  // noise, not dots
        const int id = int(parts.size());
        parts.emplace_back();
        part[i] = id;
        stack.assign(1, int(i));
        while (!stack.empty()) {
            const int at = stack.back();
            stack.pop_back();
            parts[size_t(id)].push_back(at);
            const int x = at % w, y = at / w;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    const size_t j = size_t(ny) * size_t(w) + size_t(nx);
                    if (part[j] < 0 && light[j] >= kLit) {
                        part[j] = id;
                        stack.push_back(int(j));
                    }
                }
        }
    }
    if (parts.size() < size_t(kFewest)) return out;
    std::vector<size_t> areas;
    for (const std::vector<int>& p : parts) areas.push_back(p.size());
    std::sort(areas.begin(), areas.end());
    if (areas.back() * 40 > n || areas[areas.size() / 2] * 120 > n) return out;  // a glow among them
    for (const std::vector<int>& p : parts) {
        // its tops, brightest first: one apart from those kept unless the way to one of them stays bright (a plateau,
        // a dot's own soft slope); the texels each to the nearest top
        std::vector<int> candidates;
        for (int at : p) {
            if (light[size_t(at)] < kTop) continue;
            const int x = at % w, y = at / w;
            bool top = true;
            for (int dy = -1; dy <= 1 && top; ++dy)
                for (int dx = -1; dx <= 1 && top; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if ((dx || dy) && nx >= 0 && ny >= 0 && nx < w && ny < h &&
                        light[size_t(ny) * size_t(w) + size_t(nx)] > light[size_t(at)])
                        top = false;
                }
            if (top) candidates.push_back(at);
        }
        std::stable_sort(candidates.begin(), candidates.end(),
                         [&](int a, int b) { return light[size_t(a)] > light[size_t(b)]; });
        struct Top {
            int at;            // its brightest texel
            double x, y, n;    // the middle of its plateau: the tops joined into it
        };
        std::vector<Top> tops;
        for (int c : candidates) {
            const double cx = double(c % w) + 0.5, cy = double(c / w) + 0.5;
            Top* joined = nullptr;
            for (Top& t : tops)
                if (neck(light, w, c, t.at) >= kNeck * std::min(light[size_t(c)], light[size_t(t.at)])) {
                    joined = &t;
                    break;
                }
            if (joined) {
                joined->x += cx, joined->y += cy, joined->n += 1;
            } else if (tops.size() < 16) {
                tops.push_back(Top{c, cx, cy, 1});
            }
        }
        if (tops.empty()) {
            const int at = *std::max_element(p.begin(), p.end(),
                                             [&](int a, int b) { return light[size_t(a)] < light[size_t(b)]; });
            tops.push_back(Top{at, double(at % w) + 0.5, double(at / w) + 0.5, 1});
        }
        std::vector<Sum> sums(tops.size());
        for (int at : p) {
            const float x = float(at % w) + 0.5f, y = float(at / w) + 0.5f;
            size_t best = 0;
            double bestD = 1e18;
            for (size_t k = 0; k < tops.size(); ++k) {
                const double tx = tops[k].x / tops[k].n - x, ty = tops[k].y / tops[k].n - y;
                const double d = tx * tx + ty * ty;
                if (d < bestD) bestD = d, best = k;
            }
            sums[best].add(light[size_t(at)], x, y, argb[size_t(at)]);
        }
        for (const Sum& s : sums)
            if (s.e > 0) out.push_back(s.speck());
    }
    return out;
}

std::vector<uint32_t> redraw(const std::vector<Speck>& specks, int w, int h, int scale) {
    if (w <= 0 || h <= 0 || scale <= 0 || w > kMostSide || h > kMostSide) return {};
    const int W = w * scale, H = h * scale;
    const float sc = float(scale);
    std::vector<float> sum(size_t(W) * size_t(H) * 4, 0.0f);  // its light, its colours' light
    for (const Speck& s : specks) {
        if (s.energy <= 0 || s.peak <= 0) continue;
        // its axes: the spread's own, the ellipse as large as its light needs (kMean of its peak over it)
        const float tr = s.sxx + s.syy, det = s.sxx * s.syy - s.sxy * s.sxy;
        const float disc = std::sqrt(std::max(0.0f, tr * tr / 4 - det));
        const float l1 = std::max(tr / 2 + disc, 0.02f), l2 = std::max(tr / 2 - disc, 0.02f);
        float ex = 1, ey = 0;
        if (std::fabs(s.sxy) > 1e-6f) {
            ex = l1 - s.syy, ey = s.sxy;
        } else if (s.syy > s.sxx) {
            ex = 0, ey = 1;
        }
        const float len = std::hypot(ex, ey);
        ex /= len, ey /= len;
        const float k = std::clamp(std::sqrt(s.energy / (kMean * kPi * s.peak * std::sqrt(l1 * l2))), 0.8f, 3.0f);
        const float a = k * std::sqrt(l1), b = k * std::sqrt(l2);  // texels
        const float reach = a + 1.0f / sc;
        const int x0 = std::max(0, int(std::floor((s.x - reach) * sc)));
        const int x1 = std::min(W, int(std::ceil((s.x + reach) * sc)));
        const int y0 = std::max(0, int(std::floor((s.y - reach) * sc)));
        const int y1 = std::min(H, int(std::ceil((s.y + reach) * sc)));
        for (int py = y0; py < y1; ++py)
            for (int px = x0; px < x1; ++px) {
                const float tx = (float(px) + 0.5f) / sc - s.x, ty = (float(py) + 0.5f) / sc - s.y;
                const float u = (tx * ex + ty * ey) / a, v = (-tx * ey + ty * ex) / b;
                const float d = std::sqrt(u * u + v * v);
                // its edge over one pixel of the copy: how far d goes in one
                const float gx = (u * ex / a - v * ey / b), gy = (u * ey / a + v * ex / b);
                const float edge = d > 1e-3f ? std::hypot(gx, gy) / d / sc : 1.0f / (sc * b);
                const float cover = std::clamp((1.0f - d) / std::max(edge, 1e-4f) + 0.5f, 0.0f, 1.0f);
                if (cover <= 0) continue;
                const float dd = std::min(d, 1.0f);
                const float val = s.peak * (1.0f - 0.3f * dd * dd) * cover;
                float* out = &sum[(size_t(py) * size_t(W) + size_t(px)) * 4];
                out[0] += val, out[1] += val * s.r, out[2] += val * s.g, out[3] += val * s.b;
            }
    }
    std::vector<uint32_t> px(size_t(W) * size_t(H), 0);
    for (size_t i = 0; i < px.size(); ++i) {
        const float* s = &sum[i * 4];
        if (s[0] <= 0) continue;
        const float a = std::min(1.0f, s[0]);
        const auto c = [&](float v) { return uint32_t(std::lround(std::clamp(v / a, 0.0f, 1.0f) * 255)); };
        px[i] = uint32_t(std::lround(a * 255)) << 24 | c(s[1]) << 16 | c(s[2]) << 8 | c(s[3]);
    }
    return px;
}

Copy makeCopy(const uint32_t* argb, int w, int h) {
    Copy c;
    const std::vector<Speck> specks = specksOf(argb, w, h);
    if (specks.empty()) return c;
    int scale = 8;
    while (scale > 2 && std::max(w, h) * scale > kLongest) scale /= 2;
    c.scale = scale;
    c.width = w * scale;
    c.height = h * scale;
    c.specks = int(specks.size());
    c.levels.push_back(redraw(specks, w, h, scale));
    int lw = c.width, lh = c.height;
    while (lw > w || lh > h) {  // its halves to the texture's own size
        std::vector<uint32_t> next;
        int nw = 0, nh = 0;
        full_colour::halve(c.levels.back(), lw, lh, next, nw, nh);
        if (next.empty() || (nw == lw && nh == lh)) break;
        c.levels.push_back(std::move(next));
        lw = nw;
        lh = nh;
    }
    return c;
}

}  // namespace uagfx::sharpspecks
