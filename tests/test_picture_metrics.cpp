#include <cmath>
#include <cstdint>
#include <vector>

#include "core/ai/picture_metrics.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t grey(int v, int a = 255) {
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    return uint32_t(a) << 24 | uint32_t(v) << 16 | uint32_t(v) << 8 | uint32_t(v);
}

// 32 x 32 of noise about the middle grey (a hash, the same every run)
std::vector<uint32_t> noise32() {
    std::vector<uint32_t> p(32 * 32);
    uint32_t h = 2166136261u;
    for (uint32_t& v : p) {
        h = (h ^ 0x9E3779B9u) * 16777619u;
        v = grey(64 + int(h >> 24) / 2);
    }
    return p;
}

std::vector<uint32_t> nearest(const std::vector<uint32_t>& p, int w, int h, int pw, int ph) {
    std::vector<uint32_t> out(size_t(pw) * size_t(ph));
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x) out[size_t(y * pw + x)] = p[size_t((y * h / ph) * w + x * w / pw)];
    return out;
}

// a box blur of radius r (each channel; alpha left)
std::vector<uint32_t> blurred(const std::vector<uint32_t>& p, int w, int h, int r) {
    std::vector<uint32_t> out(p.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int sum[3] = {0, 0, 0}, n = 0;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    const uint32_t v = p[size_t(((y + dy + h) % h) * w + (x + dx + w) % w)];
                    for (int c = 0; c < 3; ++c) sum[c] += int(v >> (8 * c) & 0xFF);
                    ++n;
                }
            out[size_t(y * w + x)] = (p[size_t(y * w + x)] & 0xFF000000u) | uint32_t(sum[2] / n) << 16 |
                                     uint32_t(sum[1] / n) << 8 | uint32_t(sum[0] / n);
        }
    return out;
}

}  // namespace

TEST(metrics_flag_a_blurred_picture) {
    // 2.3, the studio (T3): a picture smoothed by its network - its detail, shrunk back to the original's size, lost
    const std::vector<uint32_t> o = noise32();
    const std::vector<uint32_t> sharp = nearest(o, 32, 32, 128, 128);
    const ai::Metrics good = ai::measure(o.data(), 32, 32, sharp.data(), 128, 128, true);
    CHECK(good.detail >= 0.9f);
    CHECK(!(ai::flagsOf(good) & ai::Smoothed));
    const std::vector<uint32_t> soft = blurred(sharp, 128, 128, 6);
    const ai::Metrics bad = ai::measure(o.data(), 32, 32, soft.data(), 128, 128, true);
    CHECK(bad.detail < 0.45f);
    CHECK(ai::flagsOf(bad) & ai::Smoothed);
}

TEST(metrics_flag_a_tone_shift) {
    const std::vector<uint32_t> o = noise32();
    std::vector<uint32_t> p = nearest(o, 32, 32, 128, 128);
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, true).tone < 0.01f);
    for (uint32_t& v : p) v = (v & 0xFF00FFFFu) | uint32_t(std::min(255, int(v >> 16 & 0xFF) + 30)) << 16;
    const ai::Metrics m = ai::measure(o.data(), 32, 32, p.data(), 128, 128, true);
    CHECK(m.tone > 0.04f);
    CHECK(ai::flagsOf(m) & ai::Washed);
}

TEST(metrics_flag_banding) {
    // a gentle gradient (100 to 140 across) made into flat bands four levels apart
    std::vector<uint32_t> o(32 * 32), smooth(128 * 128), banded(128 * 128);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) o[size_t(y * 32 + x)] = grey(100 + x * 40 / 32);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            const float v = 100.0f + float(x) * 40.0f / 128.0f;
            smooth[size_t(y * 128 + x)] = grey(int(v + 0.5f));
            banded[size_t(y * 128 + x)] = grey(int(v / 4.0f) * 4);
        }
    const ai::Metrics s = ai::measure(o.data(), 32, 32, smooth.data(), 128, 128, false);
    CHECK(s.banding < 0.15f && !(ai::flagsOf(s) & ai::Banded));
    const ai::Metrics b = ai::measure(o.data(), 32, 32, banded.data(), 128, 128, false);
    CHECK(b.banding > 0.15f && (ai::flagsOf(b) & ai::Banded));
}

TEST(metrics_flag_ringing) {
    // an edge from 80 to 170; the picture with halos past either side of it
    std::vector<uint32_t> o(32 * 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) o[size_t(y * 32 + x)] = grey(x < 16 ? 80 : 170);
    std::vector<uint32_t> p = nearest(o, 32, 32, 128, 128);
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, false).ringing < 0.01f);
    for (int y = 0; y < 128; ++y) {
        p[size_t(y * 128 + 62)] = p[size_t(y * 128 + 63)] = grey(20);
        p[size_t(y * 128 + 64)] = p[size_t(y * 128 + 65)] = grey(230);
    }
    const ai::Metrics m = ai::measure(o.data(), 32, 32, p.data(), 128, 128, false);
    CHECK(m.ringing > 0.08f && (ai::flagsOf(m) & ai::Ringing));
}

TEST(metrics_flag_magenta_fringe) {
    // a green blob on the colour key; the picture's texels beside the key gone pink
    std::vector<uint32_t> o(32 * 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const bool in = (x - 16) * (x - 16) + (y - 16) * (y - 16) < 100;
            o[size_t(y * 32 + x)] = in ? 0xFF30A030u : 0x00FF00FFu;
        }
    std::vector<uint32_t> p = nearest(o, 32, 32, 128, 128);
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, false).fringe < 0.01f);
    for (int y = 1; y < 127; ++y)
        for (int x = 1; x < 127; ++x) {
            uint32_t& v = p[size_t(y * 128 + x)];
            const auto key = [&](int xx, int yy) { return (p[size_t(yy * 128 + xx)] >> 24) == 0; };
            const bool keyBeside = key(x - 1, y) || key(x + 1, y) || key(x, y - 1) || key(x, y + 1);
            if ((v >> 24) && keyBeside) v = 0xFFD050C0u;
        }
    const ai::Metrics m = ai::measure(o.data(), 32, 32, p.data(), 128, 128, false);
    CHECK(m.fringe > 0.05f && (ai::flagsOf(m) & ai::Fringe));
}

TEST(metrics_flag_spoiled_alpha) {
    std::vector<uint32_t> o(32 * 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) o[size_t(y * 32 + x)] = x < 8 ? 0x00FF00FFu : grey(120);
    std::vector<uint32_t> p = nearest(o, 32, 32, 128, 128);
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, false).alpha < 0.01f);
    for (uint32_t& v : p) v |= 0xFF000000u;  // the key gone opaque
    const ai::Metrics m = ai::measure(o.data(), 32, 32, p.data(), 128, 128, false);
    CHECK(m.alpha > 0.03f && (ai::flagsOf(m) & ai::AlphaSpoiled));
}

TEST(metrics_flag_seams) {
    const std::vector<uint32_t> o = noise32();
    std::vector<uint32_t> p = nearest(o, 32, 32, 128, 128);
    const ai::Metrics good = ai::measure(o.data(), 32, 32, p.data(), 128, 128, true);
    CHECK(good.seams < 2.0f && !(ai::flagsOf(good) & ai::Seams));
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 8; ++x) {
            uint32_t& v = p[size_t(y * 128 + x)];
            v = grey(0);  // its left edge darker than the picture ever is inside
        }
    const ai::Metrics bad = ai::measure(o.data(), 32, 32, p.data(), 128, 128, true);
    CHECK(bad.seams > 2.0f && (ai::flagsOf(bad) & ai::Seams));
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, false).seams == 0.0f);  // not a world texture
}

TEST(metrics_take_any_scale) {
    // x2 runs, a lower detail's picture, a picture a texel short: scaled, never read past
    const std::vector<uint32_t> o = noise32();
    for (int size : {64, 96, 128, 127, 32, 31, 1}) {
        const std::vector<uint32_t> p = nearest(o, 32, 32, size, size);
        const ai::Metrics m = ai::measure(o.data(), 32, 32, p.data(), size, size, true);
        CHECK(std::isfinite(m.detail) && std::isfinite(m.tone) && std::isfinite(m.banding) && std::isfinite(m.seams));
        if (size == 64) CHECK(m.detail >= 0.9f);
    }
    const ai::Metrics none = ai::measure(o.data(), 32, 32, nullptr, 0, 0, true);
    CHECK(none.detail == 1.0f && none.tone == 0.0f);
    CHECK(std::string(ai::flagName(ai::Fringe)) == "fringe");
}
