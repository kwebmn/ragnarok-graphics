#include "core/ai/recolour.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::ai {

void recolour(std::vector<uint32_t>& pic, int W, int H, const uint32_t* own, const uint32_t* dyed, int w, int h) {
    if (W <= 0 || H <= 0 || w <= 0 || h <= 0 || pic.size() < size_t(W) * size_t(H)) return;
    // Each texel's change by channel, and whether it is seen.
    std::vector<int> delta(size_t(w) * size_t(h) * 3);
    std::vector<uint8_t> seen(size_t(w) * size_t(h));
    for (size_t i = 0; i < seen.size(); ++i) {
        seen[i] = own[i] >> 24 >= 128 ? 1 : 0;
        for (int c = 0; c < 3; ++c)
            delta[i * 3 + size_t(c)] = int(dyed[i] >> (16 - 8 * c) & 255) - int(own[i] >> (16 - 8 * c) & 255);
    }
    const double kx = double(w) / W, ky = double(h) / H;
    for (int Y = 0; Y < H; ++Y) {
        const double sy = (Y + 0.5) * ky - 0.5;
        const int y0 = int(std::floor(sy));
        const double fy = sy - y0;
        const int ya = std::clamp(y0, 0, h - 1), yb = std::clamp(y0 + 1, 0, h - 1);
        for (int X = 0; X < W; ++X) {
            const double sx = (X + 0.5) * kx - 0.5;
            const int x0 = int(std::floor(sx));
            const double fx = sx - x0;
            const int xa = std::clamp(x0, 0, w - 1), xb = std::clamp(x0 + 1, 0, w - 1);
            const size_t t[4] = {size_t(ya) * size_t(w) + size_t(xa), size_t(ya) * size_t(w) + size_t(xb),
                                 size_t(yb) * size_t(w) + size_t(xa), size_t(yb) * size_t(w) + size_t(xb)};
            const double wt[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
            double sum = 0, d[3] = {0, 0, 0};
            for (int k = 0; k < 4; ++k) {
                if (!seen[t[k]] || wt[k] <= 0) continue;
                sum += wt[k];
                for (int c = 0; c < 3; ++c) d[c] += wt[k] * delta[t[k] * 3 + size_t(c)];
            }
            if (sum <= 0) continue;
            uint32_t& p = pic[size_t(Y) * size_t(W) + size_t(X)];
            uint32_t out = p & 0xFF000000u;
            for (int c = 0; c < 3; ++c) {
                const int shift = 16 - 8 * c;
                const int v = int(p >> shift & 255) + int(std::lround(d[c] / sum));
                out |= uint32_t(std::clamp(v, 0, 255)) << shift;
            }
            p = out;
        }
    }
}

}  // namespace uagfx::ai
