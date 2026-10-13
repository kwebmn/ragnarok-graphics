#include "core/twin_grid.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx {

// 1e-6: K and k are ratios of sizes, so g K can come out a hair off the whole number it equals (as in scaleRect).
int twinEdge(int g, double K) { return int(std::floor(g * K + 1e-6)); }

int ceilEdge(int x, double k) { return int(std::ceil(x * k - 1e-6)); }

TwinSpan tileSpan(int t, int n, int twinSize, double K) {
    TwinSpan s;
    s.first = std::min(twinEdge(t, K), twinSize);
    s.end = std::max(s.first, std::min(ceilEdge(t + n, K), twinSize));
    return s;
}

int tileOrigin(int x0, int t, double k, double K) { return ceilEdge(x0, k) + twinEdge(t, K); }

bool twinMatches(double K, double k) { return K > 1.0 && std::fabs(K - k) < 1e-9; }

TilePlace tilePlace(int index, int width, int height) {
    TilePlace p;
    if (index < 0 || width <= 0 || height <= 0) return p;
    const int columns = (width + 255) / 256;
    const int rows = (height + 255) / 256;
    if (index >= columns * rows) return p;  // past the last tile
    p.x = index % columns * 256;
    p.y = index / columns * 256;
    p.width = std::min(256, width - p.x);
    p.height = std::min(256, height - p.y);
    return p;
}

}  // namespace uagfx
