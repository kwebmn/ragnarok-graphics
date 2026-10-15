#include "core/twin_text.hpp"

#include <algorithm>
#include <cmath>

#include "core/twin.hpp"
#include "core/twin_grid.hpp"

namespace uagfx {

int letterPen(int x, int advance, double K) { return twinEdge(x + advance, K); }

std::vector<int> letterPens(int x, const std::vector<int>& ends, const std::vector<int>& advances, double K) {
    const size_t n = std::min(ends.size(), advances.size());
    std::vector<int> pens(n);
    if (n == 0) return pens;
    const int start = twinEdge(x, K);
    double own = 0.0;
    for (size_t i = 0; i < n; ++i) own += advances[i];
    const double spread = (double(twinEdge(x + ends[n - 1], K) - start) - own) / double(n);
    double at = 0.0;
    for (size_t i = 0; i < n; ++i) {
        pens[i] = start + int(std::lround(at));
        at += advances[i] + spread;
    }
    return pens;
}

int baselineRow(int y, int ascent, double K) { return twinEdge(y + ascent, K); }

void drawGlyph(TwinPicture& t, const GlyphBitmap& g, int penX, int baseline, uint32_t rgb) {
    if (g.width <= 0 || g.height <= 0 || g.coverage.size() < size_t(g.width) * size_t(g.height)) return;
    const int left = penX + g.originX;
    const int top = baseline - g.originY;
    const int tw = t.twinWidth();
    const int th = t.twinHeight();
    std::vector<uint32_t>& px = t.twin();
    const uint32_t r = rgb >> 16 & 255, gr = rgb >> 8 & 255, b = rgb & 255;
    for (int y = std::max(0, -top); y < g.height && top + y < th; ++y)
        for (int x = std::max(0, -left); x < g.width && left + x < tw; ++x) {
            const uint32_t c = std::min<uint32_t>(g.coverage[size_t(y) * size_t(g.width) + size_t(x)], 64);
            if (c == 0) continue;
            const uint32_t a = (c * 255 + 32) / 64;
            const uint32_t src =
                a << 24 | (r * a + 127) / 255 << 16 | (gr * a + 127) / 255 << 8 | (b * a + 127) / 255;
            uint32_t& d = px[size_t(top + y) * size_t(tw) + size_t(left + x)];
            d = over(src, d);
        }
    t.touch(Rect{left, top, left + g.width, top + g.height});
}

Rect textRect(int x, int y, int extent, int cellHeight, int margin) {
    return Rect{x - margin, y - margin, x + extent + margin, y + cellHeight + margin};
}

uint32_t textColor(uint32_t c) { return (c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF); }

int fitFontHeight(int start, int least, double target, const std::function<int(int)>& widthAt) {
    if (start <= 0 || target <= 0.0) return start;
    const int floor = std::max(least, 1);
    for (int h = start; h > floor; --h) {
        const int w = widthAt(h);
        if (w <= 0) return start;
        if (w <= target) return h;
    }
    return std::min(start, floor);
}

}  // namespace uagfx
