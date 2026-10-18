#include "core/ai/picture_metrics.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace uagfx::ai {
namespace {

float channel(uint32_t p, int c) { return float(p >> (8 * c) & 0xFF); }  // 0 blue, 1 green, 2 red, 3 alpha
float lum(uint32_t p) { return 0.299f * channel(p, 2) + 0.587f * channel(p, 1) + 0.114f * channel(p, 0); }
bool opaque(uint32_t p) { return (p >> 24) >= 128; }

// For each of n cells across a line of m texels: the texels it covers and how much of each (box weights; a cell
// smaller than a texel takes part of one or two).
std::vector<std::vector<std::pair<int, float>>> weights(int m, int n) {
    std::vector<std::vector<std::pair<int, float>>> out(size_t(std::max(n, 0)));
    const double k = double(m) / double(n);
    for (int i = 0; i < n; ++i) {
        const double a = double(i) * k, b = double(i + 1) * k;
        for (int t = int(std::floor(a)); t < int(std::ceil(b)) && t < m; ++t) {
            const double part = std::min(b, double(t + 1)) - std::max(a, double(t));
            if (part > 0) out[size_t(i)].emplace_back(std::max(t, 0), float(part / (b - a)));
        }
    }
    return out;
}

// A plane (sw x sh) made dw x dh by area averages.
std::vector<float> resized(const std::vector<float>& src, int sw, int sh, int dw, int dh) {
    const auto wx = weights(sw, dw), wy = weights(sh, dh);
    std::vector<float> rows(size_t(dw) * size_t(sh)), out(size_t(dw) * size_t(dh));
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < dw; ++x) {
            float s = 0;
            for (const auto& t : wx[size_t(x)]) s += src[size_t(y) * size_t(sw) + size_t(t.first)] * t.second;
            rows[size_t(y) * size_t(dw) + size_t(x)] = s;
        }
    for (int y = 0; y < dh; ++y)
        for (int x = 0; x < dw; ++x) {
            float s = 0;
            for (const auto& t : wy[size_t(y)]) s += rows[size_t(t.first) * size_t(dw) + size_t(x)] * t.second;
            out[size_t(y) * size_t(dw) + size_t(x)] = s;
        }
    return out;
}

// The mean squared Laplacian of a plane over the texels `use` takes (their four neighbours there: wrapped when
// wraps, else inside).
double laplacian(const std::vector<float>& v, int w, int h, const std::vector<char>& use, bool wraps) {
    double sum = 0;
    size_t n = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (!use[size_t(y) * size_t(w) + size_t(x)]) continue;
            if (!wraps && (x == 0 || y == 0 || x == w - 1 || y == h - 1)) continue;
            const auto at = [&](int xx, int yy) { return v[size_t((yy + h) % h) * size_t(w) + size_t((xx + w) % w)]; };
            const double l = 4.0 * at(x, y) - at(x - 1, y) - at(x + 1, y) - at(x, y - 1) - at(x, y + 1);
            sum += l * l;
            ++n;
        }
    return n ? sum / double(n) : 0.0;
}

float linear(float c) {
    c /= 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float srgb(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return 255.0f * (c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
}

// A line's steps between flat runs (with the original gentle there): those of 1-8 levels, and of them those of 2-8
// between runs of 4 or more - the bands.
void stepsOf(const std::vector<int>& line, const std::vector<char>& gentle, int& steps, int& bands) {
    size_t start = 0;
    int lastValue = 0;
    size_t lastLen = 0;
    bool have = false;
    for (size_t i = 1; i <= line.size(); ++i) {
        if (i < line.size() && line[i] == line[i - 1]) continue;
        const size_t len = i - start;
        const int value = line[start];
        if (have && gentle[start]) {
            const int d = std::abs(value - lastValue);
            if (d >= 1 && d <= 8 && (len >= 2 || lastLen >= 2)) {
                ++steps;
                if (d >= 2 && len >= 4 && lastLen >= 4) ++bands;
            }
        }
        have = value >= 0;
        lastValue = value;
        lastLen = len;
        start = i;
    }
}

}  // namespace

void oklabOf(uint32_t argb, float lab[3]) {
    const float r = linear(channel(argb, 2)), g = linear(channel(argb, 1)), b = linear(channel(argb, 0));
    const float l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    const float m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    const float s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    lab[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    lab[1] = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    lab[2] = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}

uint32_t argbOfOklab(const float lab[3], uint32_t alpha) {
    const float l = lab[0] + 0.3963377774f * lab[1] + 0.2158037573f * lab[2];
    const float m = lab[0] - 0.1055613458f * lab[1] - 0.0638541728f * lab[2];
    const float s = lab[0] - 0.0894841775f * lab[1] - 1.2914855480f * lab[2];
    const float l3 = l * l * l, m3 = m * m * m, s3 = s * s * s;
    const float r = 4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
    const float g = -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
    const float b = -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;
    const auto byte = [](float c) { return uint32_t(std::lround(srgb(c))); };
    return (alpha & 0xFFu) << 24 | byte(r) << 16 | byte(g) << 8 | byte(b);
}

Metrics measure(const uint32_t* original, int w, int h, const uint32_t* picture, int pw, int ph, bool wraps) {
    Metrics m;
    if (!original || !picture || w <= 0 || h <= 0 || pw <= 0 || ph <= 0) return m;
    const size_t on = size_t(w) * size_t(h), pn = size_t(pw) * size_t(ph);
    const auto source = [&](int x, int y) {  // the original's texel a picture texel comes from
        const int ox = std::min(w - 1, int(int64_t(x) * w / pw)), oy = std::min(h - 1, int(int64_t(y) * h / ph));
        return size_t(oy) * size_t(w) + size_t(ox);
    };
    std::vector<float> ol(on), pl(pn), pa(pn);
    for (size_t i = 0; i < on; ++i) ol[i] = lum(original[i]);
    for (size_t i = 0; i < pn; ++i) pl[i] = lum(picture[i]), pa[i] = channel(picture[i], 3) / 255.0f;
    // The original's look about each texel: all its 3x3 opaque, its range there, the key beside it, gentle.
    std::vector<char> inside(on, 0), keyBeside(on, 0), gentle(on, 0);
    std::vector<float> lo(on, 0), hi(on, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * size_t(w) + size_t(x);
            bool all = true, key = false;
            float a = 1e9f, b = -1e9f;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int xx = x + dx, yy = y + dy;
                    if (wraps) xx = (xx + w) % w, yy = (yy + h) % h;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    const uint32_t v = original[size_t(yy) * size_t(w) + size_t(xx)];
                    if (!opaque(v)) {
                        all = false;
                        key = true;
                        continue;
                    }
                    a = std::min(a, lum(v));
                    b = std::max(b, lum(v));
                }
            inside[i] = all && opaque(original[i]);
            keyBeside[i] = key && opaque(original[i]);
            lo[i] = a;
            hi[i] = b;
            gentle[i] = all && b - a <= 16.0f;
        }
    // detail kept and alpha: the picture shrunk to the original's size
    const std::vector<float> shrunk = resized(pl, pw, ph, w, h), shrunkAlpha = resized(pa, pw, ph, w, h);
    const double before = laplacian(ol, w, h, inside, wraps);
    m.detail = before > 1e-3 ? float(std::min(2.0, laplacian(shrunk, w, h, inside, wraps) / before)) : 1.0f;
    size_t alphaOff = 0;
    for (size_t i = 0; i < on; ++i)
        alphaOff += std::fabs(shrunkAlpha[i] - (opaque(original[i]) ? 1.0f : 0.0f)) > 0.375f;
    m.alpha = float(alphaOff) / float(on);
    // tone: the mean colours of what shows
    double sum[2][3] = {};
    size_t count[2] = {};
    for (size_t i = 0; i < on; ++i)
        if (opaque(original[i])) {
            for (int c = 0; c < 3; ++c) sum[0][c] += channel(original[i], c);
            ++count[0];
        }
    for (size_t i = 0; i < pn; ++i)
        if (opaque(picture[i])) {
            for (int c = 0; c < 3; ++c) sum[1][c] += channel(picture[i], c);
            ++count[1];
        }
    if (count[0] && count[1]) {
        float lab[2][3];
        for (int k = 0; k < 2; ++k) {
            uint32_t mean = 0xFF000000u;
            for (int c = 0; c < 3; ++c) mean |= uint32_t(std::lround(sum[k][c] / double(count[k]))) << (8 * c);
            oklabOf(mean, lab[k]);
        }
        m.tone = std::sqrt((lab[0][0] - lab[1][0]) * (lab[0][0] - lab[1][0]) +
                           (lab[0][1] - lab[1][1]) * (lab[0][1] - lab[1][1]) +
                           (lab[0][2] - lab[1][2]) * (lab[0][2] - lab[1][2]));
    }
    // banding, ringing and the fringe, texel by texel of the picture
    int steps = 0, bands = 0;
    std::vector<int> line;
    std::vector<char> soft;
    for (int pass = 0; pass < 2; ++pass) {  // rows, then columns
        const int lines = pass ? pw : ph, len = pass ? ph : pw;
        for (int a = 0; a < lines; ++a) {
            line.assign(size_t(len), 0);
            soft.assign(size_t(len), 0);
            for (int b = 0; b < len; ++b) {
                const int x = pass ? a : b, y = pass ? b : a;
                const size_t i = size_t(y) * size_t(pw) + size_t(x);
                line[size_t(b)] = opaque(picture[i]) ? int(pl[i] + 0.5f) : -1000 - (b & 1);
                soft[size_t(b)] = gentle[source(x, y)];
            }
            stepsOf(line, soft, steps, bands);
        }
    }
    m.banding = steps >= 16 ? float(bands) / float(steps) : 0.0f;
    double over = 0;
    size_t edges = 0, beside = 0, pink = 0;
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x) {
            const size_t i = size_t(y) * size_t(pw) + size_t(x), o = source(x, y);
            if (!opaque(picture[i])) continue;
            if (inside[o] && hi[o] - lo[o] > 32.0f) {
                over += std::max(0.0f, pl[i] - hi[o]) + std::max(0.0f, lo[o] - pl[i]);
                ++edges;
            }
            if (keyBeside[o]) {
                ++beside;
                const uint32_t p = picture[i], q = original[o];
                const float r = channel(p, 2), g = channel(p, 1), b = channel(p, 0);
                const float lean = (r - g) + (b - g);
                const float was = (channel(q, 2) - channel(q, 1)) + (channel(q, 0) - channel(q, 1));
                pink += r > g && b > g && lean - was > 60.0f;
            }
        }
    m.ringing = edges ? float(over / double(edges) / 255.0) : 0.0f;
    m.fringe = beside ? float(pink) / float(beside) : 0.0f;
    // seams: across the wrap against inside (at the same place between the original's texels when it is a whole
    // multiple: a nearest picture changes only there)
    if (wraps && pw > 1 && ph > 1) {
        float ratio = 0;
        for (int pass = 0; pass < 2; ++pass) {
            const int len = pass ? ph : pw, lines = pass ? pw : ph, orig = pass ? h : w;
            const int step = len % orig == 0 ? len / orig : 1;
            const auto at = [&](int along, int across) {
                return pass ? pl[size_t(along) * size_t(pw) + size_t(across)]
                            : pl[size_t(across) * size_t(pw) + size_t(along)];
            };
            double wrap = 0, in = 0;
            size_t ins = 0;
            for (int a = 0; a < lines; ++a) {
                wrap += std::fabs(at(len - 1, a) - at(0, a));
                for (int b = step - 1; b + 1 < len; b += step) in += std::fabs(at(b, a) - at(b + 1, a)), ++ins;
            }
            wrap /= double(lines);
            in = ins ? in / double(ins) : 0.0;
            ratio = std::max(ratio, float(wrap / std::max(in, 0.5)));
        }
        m.seams = ratio;
    }
    return m;
}

uint16_t flagsOf(const Metrics& m) {
    uint16_t f = 0;
    if (m.detail < 0.45f) f |= Smoothed;
    if (m.tone > 0.04f) f |= Washed;
    if (m.banding > 0.15f) f |= Banded;
    if (m.ringing > 0.08f) f |= Ringing;
    if (m.fringe > 0.05f) f |= Fringe;
    if (m.alpha > 0.03f) f |= AlphaSpoiled;
    if (m.seams > 2.0f) f |= Seams;
    return f;
}

const char* flagName(Flag f) {
    switch (f) {
        case Smoothed: return "smoothed";
        case Washed: return "washed";
        case Banded: return "banded";
        case Ringing: return "ringing";
        case Fringe: return "fringe";
        case AlphaSpoiled: return "alpha";
        case Seams: return "seams";
    }
    return "";
}

}  // namespace uagfx::ai
