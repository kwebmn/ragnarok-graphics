#include "core/minimap_proto.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::minimap {
namespace {

constexpr float kSide = 160, kLine = 18, kMargin = 8;  // at scale 1, the target's pixels
constexpr uint32_t kGround = 0xFF141922, kBand = 0xFF0D1016;

}  // namespace

bool cellOf(float worldX, float worldZ, int width, int height, int cellSize, Cell& out) {
    if (width <= 0 || height <= 0 || cellSize <= 0 || !std::isfinite(worldX) || !std::isfinite(worldZ)) return false;
    const float x = worldX / float(cellSize) + float(width / 2);
    const float y = worldZ / float(cellSize) + float(height / 2);
    if (!(x >= 0) || !(y >= 0) || x >= float(width) || y >= float(height)) return false;
    out.x = int(x);  // toward zero, as __ftol (positive here)
    out.y = int(y);
    return true;
}

Geometry geometry(int targetW, int targetH, float scale) {
    const float s = std::max(0.5f, scale);
    Geometry g;
    g.size = int(std::lround(kSide * s));
    g.textH = int(std::lround(kLine * s));
    const int m = int(std::lround(kMargin * s));
    g.x = std::max(0, targetW - m - g.size);
    g.y = std::min(m, std::max(0, targetH - g.size - g.textH));
    return g;
}

bool inside(const Geometry& g, float x, float y) {
    return x >= float(g.x) && x < float(g.x + g.size) && y >= float(g.y) && y < float(g.y + g.size + g.textH);
}

View view(const MapSize& map, const Cell& player, int cells, int size) {
    View v;
    if (cells <= 0 || size <= 0) return v;
    v.cellPx = float(size) / float(cells);
    const float half = float(cells) / 2;
    v.left = std::clamp(float(player.x) + 0.5f - half, 0.0f, std::max(0.0f, float(map.w - cells)));
    v.top = std::clamp(float(player.y) + 0.5f + half, std::min(float(map.h), float(cells)), float(map.h));
    return v;
}

void cellToPanel(const View& v, const MapSize&, const Cell& c, float& px, float& py) {
    px = (float(c.x) + 0.5f - v.left) * v.cellPx;
    py = (v.top - (float(c.y) + 0.5f)) * v.cellPx;
}

void paint(panel::Picture& out, const Geometry& g, const std::vector<uint32_t>& argb, int imgW, int imgH,
           const MapSize& map, const Cell& player, int cells) {
    out.width = g.size;
    out.height = g.size + g.textH;
    out.px.assign(size_t(out.width) * size_t(out.height), kBand);
    const View v = view(map, player, cells, g.size);
    const bool picture = imgW > 0 && imgH > 0 && map.w > 0 && map.h > 0 && v.cellPx > 0 &&
                         argb.size() >= size_t(imgW) * size_t(imgH);
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            uint32_t c = kGround;
            if (picture) {
                const float cx = v.left + (float(x) + 0.5f) / v.cellPx;  // the cell under the pixel
                const float cy = v.top - (float(y) + 0.5f) / v.cellPx;
                const int ix = std::clamp(int(cx / float(map.w) * float(imgW)), 0, imgW - 1);
                const int iy = std::clamp(int((float(map.h) - cy) / float(map.h) * float(imgH)), 0, imgH - 1);
                const uint32_t s = argb[size_t(iy) * size_t(imgW) + size_t(ix)];
                if ((s & 0xFFFFFF) != 0xFF00FF && (s >> 24) != 0) c = 0xFF000000 | (s & 0xFFFFFF);  // magenta: none
            }
            out.px[size_t(y) * size_t(out.width) + size_t(x)] = c;
        }
    }
    float px = 0, py = 0;
    cellToPanel(v, map, player, px, py);
    const float r = std::max(2.5f, float(g.size) / 50.0f);
    panel::circle(out, px, py, r + 1.5f, 0x000000, 0.85f);  // the player: white with a dark edge
    panel::circle(out, px, py, r, 0xFFFFFF, 1.0f);
}

}  // namespace uagfx::minimap
