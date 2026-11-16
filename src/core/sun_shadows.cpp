#include "core/sun_shadows.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace uagfx::sun_shadows {
namespace {

float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// the tops' lightmaps' brightness over the map, counted: how many texels at each of 256 levels
void levelsOf(const gnd::Ground& g, size_t counts[256], size_t& all) {
    std::fill(counts, counts + 256, size_t(0));
    all = 0;
    for (const gnd::Cell& c : g.cells) {
        if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) continue;
        const size_t lm = g.surfaces[size_t(c.top)].lightmap;
        if (lm >= g.lightmaps.size()) continue;
        for (const uint8_t b : g.lightmaps[lm].brightness) ++counts[b];
        all += 64;
    }
}

int levelAt(const size_t counts[256], size_t all, float share) {
    const size_t want = size_t(std::lround(double(share) * double(all - 1)));
    size_t seen = 0;
    for (int level = 0; level < 256; ++level) {
        seen += counts[level];
        if (seen > want) return level;
    }
    return 255;
}

}  // namespace

void axesOf(const float toSun[3], float right[3], float up[3]) {
    // across the world's up (-y) and the way to the sun: (0, -1, 0) x toSun
    right[0] = -toSun[2], right[1] = 0.0f, right[2] = toSun[0];
    const float length = std::sqrt(right[0] * right[0] + right[2] * right[2]);
    if (length < 1e-5f) {  // the sun straight overhead
        right[0] = 1.0f, right[2] = 0.0f;
    } else {
        right[0] /= length, right[2] /= length;
    }
    up[0] = toSun[1] * right[2] - toSun[2] * right[1];
    up[1] = toSun[2] * right[0] - toSun[0] * right[2];
    up[2] = toSun[0] * right[1] - toSun[1] * right[0];
    const float tall = std::sqrt(dot3(up, up));
    if (tall > 1e-6f)
        for (int k = 0; k < 3; ++k) up[k] /= tall;
}

Box boxAt(const float focus[3], const float toSun[3], float half, float depth, int texels) {
    Box b;
    const float length = std::sqrt(dot3(toSun, toSun));
    for (int k = 0; k < 3; ++k) b.toSun[k] = length > 1e-6f ? toSun[k] / length : (k == 1 ? -1.0f : 0.0f);
    axesOf(b.toSun, b.right, b.up);
    b.half = half;
    b.depth = depth;
    const float texel = 2.0f * half / float(std::max(texels, 1));
    const float along = dot3(focus, b.right), tall = dot3(focus, b.up);
    const float moveRight = std::round(along / texel) * texel - along, moveUp = std::round(tall / texel) * texel - tall;
    for (int k = 0; k < 3; ++k) b.middle[k] = focus[k] + b.right[k] * moveRight + b.up[k] * moveUp;
    return b;
}

void placeIn(const Box& b, const float w[3], float out[3]) {
    const float d[3] = {w[0] - b.middle[0], w[1] - b.middle[1], w[2] - b.middle[2]};
    out[0] = 0.5f + dot3(d, b.right) / (2.0f * b.half);
    out[1] = 0.5f - dot3(d, b.up) / (2.0f * b.half);
    out[2] = 0.5f - dot3(d, b.toSun) / (2.0f * b.depth);
}

void focusOf(const float eye[3], const float forward[3], float half, float out[3]) {
    const float flat = std::sqrt(forward[0] * forward[0] + forward[2] * forward[2]);
    out[0] = eye[0], out[1] = eye[1], out[2] = eye[2];
    if (flat < 1e-3f) return;
    out[0] += forward[0] / flat * half * 0.5f;
    out[2] += forward[2] / flat * half * 0.5f;
}

float shadeOf(const gnd::Ground& g) {
    size_t counts[256], all = 0;
    levelsOf(g, counts, all);
    if (!all) return kShadeUsual;
    const int lit = levelAt(counts, all, 0.7f), dark = levelAt(counts, all, 0.1f);
    if (lit <= 0 || float(dark) > 0.97f * float(lit)) return kShadeUsual;  // no shadow painted
    return std::clamp(float(dark) / float(lit), kShadeLeast, kShadeMost);
}

Sunless sunlessOf(const gnd::Ground& g) {
    Sunless s;
    size_t counts[256], all = 0;
    levelsOf(g, counts, all);
    s.tops.assign(g.lightmaps.size() + 1, 0);
    if (!all) return s;
    s.lit = uint32_t(levelAt(counts, all, 0.7f));
    for (const gnd::Cell& c : g.cells) {
        if (c.top < 0 || size_t(c.top) >= g.surfaces.size()) continue;
        const size_t lm = g.surfaces[size_t(c.top)].lightmap;
        if (lm < g.lightmaps.size()) s.tops[lm] = 1;
    }
    s.tops[g.lightmaps.size()] = 1;  // the mean cell after them all (terrain::meanLightCell): the land's
    return s;
}

void sunlessRows(const terrain::Atlas& atlas, const Sunless& s, int row, int rows, uint32_t* out) {
    const int cols = atlas.width / 8;
    for (int y = 0; y < rows; ++y) {
        const int ay = row + y;
        if (ay < 0 || ay >= atlas.height) continue;
        const uint32_t* from = atlas.argb.data() + size_t(ay) * size_t(atlas.width);
        uint32_t* to = out + size_t(y) * size_t(atlas.width);
        for (int x = 0; x < atlas.width; ++x) {
            const size_t lightmap = size_t(ay / 8) * size_t(std::max(cols, 1)) + size_t(x / 8);
            const bool top = cols > 0 && x / 8 < cols && lightmap < s.tops.size() && s.tops[lightmap];
            to[x] = top && (from[x] >> 24) < s.lit ? (from[x] & 0x00FFFFFFu) | (s.lit << 24) : from[x];
        }
    }
}

terrain::Atlas sunlessAtlas(const gnd::Ground& g, const terrain::Atlas& atlas) {
    terrain::Atlas out = atlas;
    if (atlas.width > 0 && atlas.height > 0 && atlas.argb.size() >= size_t(atlas.width) * size_t(atlas.height))
        sunlessRows(atlas, sunlessOf(g), 0, atlas.height, out.argb.data());
    return out;
}

}  // namespace uagfx::sun_shadows
