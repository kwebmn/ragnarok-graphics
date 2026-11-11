#include "core/mount_icon.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::mounticon {
namespace {

struct Seg {
    float x0, y0, x1, y1;
};

float distance(float px, float py, const Seg& s) {
    const float dx = s.x1 - s.x0, dy = s.y1 - s.y0, len = dx * dx + dy * dy;
    const float t = len > 0 ? std::clamp(((px - s.x0) * dx + (py - s.y0) * dy) / len, 0.0f, 1.0f) : 0.0f;
    const float cx = s.x0 + t * dx - px, cy = s.y0 + t * dy - py;
    return std::sqrt(cx * cx + cy * cy);
}

}  // namespace

void paint(std::vector<uint32_t>& px, int w, int h, uint32_t rgb) {
    if (w <= 0 || h <= 0 || px.size() < size_t(w) * size_t(h)) return;
    const float s = float(std::min(w, h)), ox = (float(w) - s) / 2, oy = (float(h) - s) / 2;
    const auto at = [&](float x, float y) { return std::pair<float, float>{ox + x * s, oy + y * s}; };
    std::vector<Seg> segs;
    const auto add = [&](float x0, float y0, float x1, float y1) {
        const auto a = at(x0, y0), b = at(x1, y1);
        segs.push_back(Seg{a.first, a.second, b.first, b.second});
    };
    add(0.42f, 0.2f, 0.22f, 0.2f);  // the bracket: top, upright, bottom
    add(0.22f, 0.2f, 0.22f, 0.8f);
    add(0.22f, 0.8f, 0.42f, 0.8f);
    add(0.38f, 0.5f, 0.8f, 0.5f);  // the arrow's shaft and its head
    add(0.64f, 0.34f, 0.8f, 0.5f);
    add(0.64f, 0.66f, 0.8f, 0.5f);
    const float half = std::max(0.9f, s * 0.055f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float d = 1e9f;
            for (const Seg& g : segs) d = std::min(d, distance(float(x) + 0.5f, float(y) + 0.5f, g));
            const float cover = std::clamp(half + 0.5f - d, 0.0f, 1.0f);
            if (cover <= 0) continue;
            const uint32_t a = uint32_t(std::lround(cover * 255));
            const auto ch = [&](int shift) { return (((rgb >> shift) & 0xFF) * a + 127) / 255; };
            px[size_t(y) * size_t(w) + size_t(x)] = a << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

}  // namespace uagfx::mounticon
