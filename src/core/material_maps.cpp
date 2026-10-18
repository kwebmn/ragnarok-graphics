#include "core/material_maps.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::material {
namespace {

float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct Level {
    int w = 0, h = 0;
    std::vector<float> r, g, b;
};

}  // namespace

void mapsOf(const uint32_t* argb, int w, int h, const pack::MaterialScales& scales, std::vector<uint32_t>& out,
            int& ow, int& oh) {
    out.clear();
    ow = oh = 0;
    if (!argb || w <= 0 || h <= 0) return;
    // The picture's levels down to one texel (each the average of four), as the card's mips.
    std::vector<Level> levels(1);
    levels[0].w = w;
    levels[0].h = h;
    for (size_t i = 0; i < size_t(w) * size_t(h); ++i) {
        levels[0].r.push_back(float(argb[i] >> 16 & 0xFF) / 255.0f);
        levels[0].g.push_back(float(argb[i] >> 8 & 0xFF) / 255.0f);
        levels[0].b.push_back(float(argb[i] & 0xFF) / 255.0f);
    }
    while (levels.back().w > 1 || levels.back().h > 1) {
        const Level& a = levels.back();
        Level n;
        n.w = std::max(1, a.w / 2);
        n.h = std::max(1, a.h / 2);
        for (int y = 0; y < n.h; ++y)
            for (int x = 0; x < n.w; ++x) {
                float s[3] = {0, 0, 0};
                int k = 0;
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) {
                        const int sx = std::min(a.w - 1, x * 2 + dx), sy = std::min(a.h - 1, y * 2 + dy);
                        const size_t i = size_t(sy) * size_t(a.w) + size_t(sx);
                        s[0] += a.r[i], s[1] += a.g[i], s[2] += a.b[i];
                        ++k;
                    }
                n.r.push_back(s[0] / float(k));
                n.g.push_back(s[1] / float(k));
                n.b.push_back(s[2] / float(k));
            }
        levels.push_back(std::move(n));
    }
    const int have = int(levels.size()) - 1;
    const auto texel = [&](int x, int y, int at, float rgb[3]) {  // tex0.Load, wrapped
        const Level& l = levels[size_t(at)];
        x = ((x % l.w) + l.w) % l.w;
        y = ((y % l.h) + l.h) % l.h;
        const size_t i = size_t(y) * size_t(l.w) + size_t(x);
        rgb[0] = l.r[i], rgb[1] = l.g[i], rgb[2] = l.b[i];
    };
    const auto luma = [&](int x, int y, int lod) {  // matLuma
        const int at = std::min(lod, have);
        float rgb[3];
        texel(x << (lod - at), y << (lod - at), at, rgb);
        return rgb[0] * 0.299f + rgb[1] * 0.587f + rgb[2] * 0.114f;
    };
    int shift = 0;  // the source's level the material is made from: at most 512 a side
    while ((w >> shift) > kMostSide || (h >> shift) > kMostSide) ++shift;
    ow = std::max(1, w >> shift);
    oh = std::max(1, h >> shift);
    out.resize(size_t(ow) * size_t(oh));
    const int base = shift, most = base + 5;
    for (int py = 0; py < oh; ++py)
        for (int px = 0; px < ow; ++px) {
            const float far = luma(px >> (most - base), py >> (most - base), most);
            float sx = 0, sy = 0, hsum = 0, weights = 0;
            for (int s = 0; s < 4; ++s) {
                const int lod = std::min(base + s, most);
                const int qx = px >> (lod - base), qy = py >> (lod - base);
                const float wgt = s == 0 ? 1.0f : s == 1 ? 0.75f : s == 2 ? 0.55f : 0.4f;
                const float gx = luma(qx + 1, qy, lod) - luma(qx - 1, qy, lod);
                const float gy = luma(qx, qy + 1, lod) - luma(qx, qy - 1, lod);
                sx += gx * 0.5f * wgt / float(1 << (lod - base));
                sy += gy * 0.5f * wgt / float(1 << (lod - base));
                hsum += (luma(qx, qy, lod) - far) * wgt;
                weights += wgt;
            }
            float nx = -sx * 5.0f * scales.relief, ny = -sy * 5.0f * scales.relief, nz = 1.0f;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len, ny /= len;
            const int at = std::min(base, have);
            float rgb[3];
            texel(px << (base - at), py << (base - at), at, rgb);
            const float sat = std::max({rgb[0], rgb[1], rgb[2]}) - std::min({rgb[0], rgb[1], rgb[2]});
            const float lum = rgb[0] * 0.299f + rgb[1] * 0.587f + rgb[2] * 0.114f;
            float gloss = std::clamp((0.22f - sat) * 4.0f, 0.0f, 1.0f) * smoothstep(0.2f, 0.6f, lum) * 0.7f;
            gloss = std::min(gloss * scales.gloss, 0.7f);
            const float local = luma(px >> 2, py >> 2, base + 2), wide = luma(px >> 4, py >> 4, base + 4);
            const float pane = smoothstep(0.1f, 0.2f, wide - local) * smoothstep(0.35f, 0.22f, local) *
                               smoothstep(0.08f, 0.03f, std::fabs(lum - local));
            if (pane > 0.3f && scales.gloss > 0.0f) gloss = 0.75f + 0.25f * pane;
            const float height = std::clamp(0.5f + hsum / weights * 1.6f * scales.height, 0.0f, 1.0f);
            const auto byte = [](float v) { return uint32_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
            out[size_t(py) * size_t(ow) + size_t(px)] =
                byte(nx * 0.5f + 0.5f) | byte(ny * 0.5f + 0.5f) << 8 | byte(height) << 16 | byte(gloss) << 24;
        }
}

}  // namespace uagfx::material
