#include "core/launcher_art.hpp"

#include <algorithm>
#include <cmath>

#include "core/jpeg_image.hpp"
#include "core/webp_image.hpp"

namespace uagfx::launcherart {

Image decode(const std::string& bytes) {
    Image img;
    if (!jpeg::decode(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), img.argb, img.w, img.h))
        return Image{};
    return img;
}

void cover(const Image& src, int w, int h, std::vector<uint32_t>& out) {
    out.assign(size_t(std::max(w, 0)) * size_t(std::max(h, 0)), kGround);
    if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0 || src.argb.size() < size_t(src.w) * size_t(src.h)) return;
    const double k = std::max(double(w) / src.w, double(h) / src.h);  // the larger: it fills both ways
    const double ox = (src.w * k - w) / 2.0, oy = (src.h * k - h) / 2.0;
    std::vector<int> column(size_t(w), 0);
    for (int x = 0; x < w; ++x) column[size_t(x)] = std::clamp(int((x + ox + 0.5) / k), 0, src.w - 1);
    for (int y = 0; y < h; ++y) {
        const int sy = std::clamp(int((y + oy + 0.5) / k), 0, src.h - 1);
        const uint32_t* row = src.argb.data() + size_t(sy) * size_t(src.w);
        uint32_t* to = out.data() + size_t(y) * size_t(w);
        for (int x = 0; x < w; ++x) to[x] = row[column[size_t(x)]];
    }
}

void blend(std::vector<uint32_t>& a, const std::vector<uint32_t>& b, float t) {
    const int k = std::clamp(int(std::lround(t * 256.0f)), 0, 256);
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = a[i], q = b[i];
        uint32_t out = 0xFF000000u;
        for (int shift = 0; shift <= 16; shift += 8) {
            const int x = int((p >> shift) & 255u), y = int((q >> shift) & 255u);
            out |= uint32_t((x * (256 - k) + y * k) >> 8) << shift;
        }
        a[i] = out;
    }
}

Image decodeLogo(const std::string& bytes) {
    Image img;
    if (!webp::decode(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), img.argb, img.w, img.h))
        return Image{};
    return img;
}

namespace {

struct Tap {
    int at;
    float weight;
};

// for each of `to` pixels along a line, the `from` pixels it takes and how much of each: what it covers when there
// are fewer of it (their weights adding up to 1), the two nearest by distance when there are more
std::vector<std::vector<Tap>> tapsFor(int from, int to) {
    std::vector<std::vector<Tap>> taps;
    taps.resize(size_t(to));
    const double k = double(from) / double(to);
    for (int i = 0; i < to; ++i) {
        std::vector<Tap>& t = taps[size_t(i)];
        if (to <= from) {
            const double a = i * k, b = (i + 1) * k;
            for (int j = int(std::floor(a)); j < int(std::ceil(b)) && j < from; ++j) {
                const double cover = std::min(b, double(j + 1)) - std::max(a, double(j));
                if (cover > 1e-9) t.push_back(Tap{j, float(cover / k)});
            }
        } else {
            const double u = (i + 0.5) * k - 0.5;
            const int j = int(std::floor(u));
            const float f = float(u - j);
            t.push_back(Tap{std::clamp(j, 0, from - 1), 1 - f});
            t.push_back(Tap{std::clamp(j + 1, 0, from - 1), f});
        }
    }
    return taps;
}

}  // namespace

void fitted(const Image& src, int w, int h, std::vector<uint32_t>& out) {
    out.assign(size_t(std::max(w, 0)) * size_t(std::max(h, 0)), 0u);
    if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0 || src.argb.size() < size_t(src.w) * size_t(src.h)) return;
    // premultiplied: a clear pixel's colour (whatever is under it) takes no part
    std::vector<float> pm(size_t(src.w) * size_t(src.h) * 4);
    for (size_t i = 0; i < size_t(src.w) * size_t(src.h); ++i) {
        const uint32_t c = src.argb[i];
        const float a = float(c >> 24) / 255.0f;
        pm[4 * i] = float(c >> 24);
        pm[4 * i + 1] = float((c >> 16) & 255u) * a;
        pm[4 * i + 2] = float((c >> 8) & 255u) * a;
        pm[4 * i + 3] = float(c & 255u) * a;
    }
    const std::vector<std::vector<Tap>> across = tapsFor(src.w, w), down = tapsFor(src.h, h);
    std::vector<float> rows(size_t(w) * size_t(src.h) * 4, 0.0f);  // across first: src.h rows of w
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < w; ++x) {
            float* to = &rows[(size_t(y) * size_t(w) + size_t(x)) * 4];
            for (const Tap& t : across[size_t(x)]) {
                const float* from = &pm[(size_t(y) * size_t(src.w) + size_t(t.at)) * 4];
                for (int c = 0; c < 4; ++c) to[c] += from[c] * t.weight;
            }
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float v[4] = {0, 0, 0, 0};
            for (const Tap& t : down[size_t(y)]) {
                const float* from = &rows[(size_t(t.at) * size_t(w) + size_t(x)) * 4];
                for (int c = 0; c < 4; ++c) v[c] += from[c] * t.weight;
            }
            const uint32_t a = uint32_t(std::clamp(std::lround(v[0]), 0L, 255L));
            const auto channel = [&](float f) { return std::min(a, uint32_t(std::clamp(std::lround(f), 0L, 255L))); };
            out[size_t(y) * size_t(w) + size_t(x)] = a << 24 | channel(v[1]) << 16 | channel(v[2]) << 8 | channel(v[3]);
        }
}

void over(std::vector<uint32_t>& dst, int dw, int dh, const std::vector<uint32_t>& src, int w, int h, int x, int y) {
    if (dw <= 0 || dh <= 0 || w <= 0 || h <= 0) return;
    if (dst.size() < size_t(dw) * size_t(dh) || src.size() < size_t(w) * size_t(h)) return;
    for (int j = std::max(0, -y); j < h && y + j < dh; ++j)
        for (int i = std::max(0, -x); i < w && x + i < dw; ++i) {
            const uint32_t s = src[size_t(j) * size_t(w) + size_t(i)], a = s >> 24;
            if (!a) continue;
            uint32_t& d = dst[size_t(y + j) * size_t(dw) + size_t(x + i)];
            uint32_t mixed = 0xFF000000u;
            for (int shift = 0; shift <= 16; shift += 8) {
                const uint32_t c = ((s >> shift) & 255u) + (((d >> shift) & 255u) * (255u - a) + 127u) / 255u;
                mixed |= std::min(c, 255u) << shift;
            }
            d = mixed;
        }
}

}  // namespace uagfx::launcherart
