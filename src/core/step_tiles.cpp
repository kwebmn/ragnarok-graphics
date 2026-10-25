#include "core/step_tiles.hpp"

#include <algorithm>
#include <cmath>

#include "core/round.hpp"

namespace uagfx::step_tiles {

uint64_t Tiles::at(int column, int row) const {
    if (column < 0 || row < 0 || column >= columns || row >= rows) return 0;
    const size_t i = (size_t(row) * size_t(columns) + size_t(column)) * 4;
    return uint64_t(masks[i]) | uint64_t(masks[i + 1]) << 16 | uint64_t(masks[i + 2]) << 32 |
           uint64_t(masks[i + 3]) << 48;
}

Tiles build(const std::deque<footsteps::Step>& steps, const View& v) {
    Tiles t;
    if (v.width <= 0 || v.height <= 0) return t;
    t.columns = std::min((v.width + kTile - 1) / kTile, kMaxColumns);
    t.rows = std::min((v.height + kTile - 1) / kTile, kMaxRows);
    t.masks.assign(size_t(t.columns) * size_t(t.rows) * 4, 0);
    // the world into the view: the inverse of the view's ways (as ground.cpp's applySeams turns its strips)
    const float* m = v.viewToWorld;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return t;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    int ux0 = v.width, uy0 = v.height, ux1 = 0, uy1 = 0;
    size_t index = 0;
    for (const footsteps::Step& s : steps) {
        if (index >= footsteps::Tracker::kMax) break;
        const size_t step = index++;
        float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
        int behind = 0;
        for (int corner = 0; corner < 8; ++corner) {  // the box of its reach, each corner into the pass's pixels
            const float x = s.x + (corner & 1 ? kReach : -kReach) - m[9];
            const float y = s.y + (corner & 2 ? kRise : -kRise) - m[10];
            const float z = s.z + (corner & 4 ? kReach : -kReach) - m[11];
            const float vx = x * inv[0] + y * inv[3] + z * inv[6], vy = x * inv[1] + y * inv[4] + z * inv[7];
            const float vz = x * inv[2] + y * inv[5] + z * inv[8];
            if (vz <= v.nearZ) {
                ++behind;
                continue;
            }
            const float px = (vx / vz * v.f0 + v.cx) * v.scale + v.jitterX;
            const float py = (vy / vz * v.f1 + v.cy) * v.scale + v.jitterY;
            lo[0] = std::min(lo[0], px);
            lo[1] = std::min(lo[1], py);
            hi[0] = std::max(hi[0], px);
            hi[1] = std::max(hi[1], py);
        }
        if (behind == 8) continue;  // all of it behind the camera
        int x0 = 0, y0 = 0, x1 = v.width, y1 = v.height;  // across the camera's plane: it may show anywhere
        if (!behind) {
            // clamped first (a corner close to the camera's plane far off the screen), then whole pixels
            const auto clampTo = [](float p, int most) { return std::clamp(p, -1.0f, float(most) + 1.0f); };
            x0 = std::max(0, floorInt(clampTo(lo[0], v.width)) - 1);
            y0 = std::max(0, floorInt(clampTo(lo[1], v.height)) - 1);
            x1 = std::min(v.width, ceilInt(clampTo(hi[0], v.width)) + 2);
            y1 = std::min(v.height, ceilInt(clampTo(hi[1], v.height)) + 2);
        }
        if (x1 <= x0 || y1 <= y0) continue;  // off the screen
        ux0 = std::min(ux0, x0);
        uy0 = std::min(uy0, y0);
        ux1 = std::max(ux1, x1);
        uy1 = std::max(uy1, y1);
        const int c0 = std::min(x0 / kTile, t.columns - 1), c1 = std::min((x1 - 1) / kTile, t.columns - 1);
        const int r0 = std::min(y0 / kTile, t.rows - 1), r1 = std::min((y1 - 1) / kTile, t.rows - 1);
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                t.masks[(size_t(r) * size_t(t.columns) + size_t(c)) * 4 + step / 16] |= uint16_t(1u << (step % 16));
    }
    if (ux1 > ux0 && uy1 > uy0) {
        t.x0 = ux0;
        t.y0 = uy0;
        t.x1 = ux1;
        t.y1 = uy1;
    }
    return t;
}

size_t fill(const std::deque<footsteps::Step>& steps, float now, float life, const Tiles& tiles, float* out) {
    const size_t row = size_t(kTexelsPerRow) * 4;
    std::fill(out, out + row * size_t(1 + tiles.rows), 0.0f);
    size_t n = 0;
    for (const footsteps::Step& s : steps) {
        if (n >= footsteps::Tracker::kMax) break;
        float* texel = out + 8 * n;
        texel[0] = s.x;
        texel[1] = s.y;
        texel[2] = s.z;
        texel[3] = s.angle;
        texel[4] = life > 0.0f ? std::clamp((now - s.born) / life, 0.0f, 1.0f) : 1.0f;
        ++n;
    }
    for (int r = 0; r < tiles.rows; ++r)
        for (int c = 0; c < tiles.columns; ++c)
            for (int part = 0; part < 4; ++part)
                out[row * size_t(1 + r) + size_t(c) * 4 + size_t(part)] =
                    float(tiles.masks[(size_t(r) * size_t(tiles.columns) + size_t(c)) * 4 + size_t(part)]);
    return n;
}

}  // namespace uagfx::step_tiles
