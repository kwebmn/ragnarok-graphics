#include "core/atlas_tiles.hpp"

#include <cmath>

namespace uagfx::atlas_tiles {
namespace {

// How many cells (4, 2 or 1) a way every rectangle's range [lo, hi] lies inside one of.
int cellsOf(const std::vector<map_textures::UvRect>& rects, bool across) {
    constexpr float kSlack = 1.0f / 512.0f;
    for (int n : {4, 2}) {
        bool fits = true;
        for (const map_textures::UvRect& r : rects) {
            const float lo = across ? r.u0 : r.v0, hi = across ? r.u1 : r.v1;
            const float a = std::floor((lo + kSlack) * float(n)), b = std::floor((hi - kSlack) * float(n));
            if (a != b || a < 0 || a >= float(n)) {
                fits = false;
                break;
            }
        }
        if (fits) return n;
    }
    return 1;
}

}  // namespace

Grid gridOf(const map_textures::GroundUse& use) {
    if (use.rects.empty() || (use.pairs && use.joined * 2 > use.pairs)) return Grid{};
    return Grid{cellsOf(use.rects, true), cellsOf(use.rects, false)};
}

std::vector<uint32_t> cutTile(const uint32_t* argb, int w, int h, Grid g, int col, int row, int pad, int& tw, int& th) {
    const int cw = w / std::max(1, g.cols), ch = h / std::max(1, g.rows);
    if (cw <= 0 || ch <= 0 || pad < 0) {
        tw = th = 0;
        return {};
    }
    tw = cw + 2 * pad;
    th = ch + 2 * pad;
    std::vector<uint32_t> out(size_t(tw) * size_t(th));
    for (int y = 0; y < th; ++y) {
        const int sy = row * ch + ((y - pad) % ch + ch) % ch;
        for (int x = 0; x < tw; ++x) {
            const int sx = col * cw + ((x - pad) % cw + cw) % cw;
            out[size_t(y) * size_t(tw) + size_t(x)] = argb[size_t(sy) * size_t(w) + size_t(sx)];
        }
    }
    return out;
}

void placeTile(std::vector<uint32_t>& out, int W, int H, Grid g, int col, int row, const std::vector<uint32_t>& tile,
               int tw, int th, int padScaled) {
    const int cw = W / std::max(1, g.cols), ch = H / std::max(1, g.rows);
    if (tile.size() < size_t(tw) * size_t(th) || out.size() < size_t(W) * size_t(H)) return;
    for (int y = 0; y < ch && y + padScaled < th; ++y)
        for (int x = 0; x < cw && x + padScaled < tw; ++x)
            out[size_t(row * ch + y) * size_t(W) + size_t(col * cw + x)] =
                tile[size_t(y + padScaled) * size_t(tw) + size_t(x + padScaled)];
}

}  // namespace uagfx::atlas_tiles
