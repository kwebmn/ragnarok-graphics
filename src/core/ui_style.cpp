#include "core/ui_style.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace uagfx::uistyle {
namespace {

std::atomic<int> g_panel{kPanelDefault}, g_window{kWindowDefault}, g_generation{0};

}  // namespace

void set(int panelPercent, int windowPercent) {
    const int p = std::clamp(panelPercent, kPanelLeast, 100), w = std::clamp(windowPercent, kWindowLeast, 100);
    if (p == g_panel && w == g_window) return;
    g_panel = p;
    g_window = w;
    ++g_generation;
}

float panel() { return float(g_panel) / 100.0f; }
float window() { return float(g_window) / 100.0f; }
float chat() { return std::max(0.8f, panel()); }
int generation() { return g_generation; }

panel::Picture vignette(int w, int h, uint32_t rgb, float alpha, float depth) {
    panel::Picture p;
    if (w <= 0 || h <= 0 || depth <= 0) return p;
    p.width = w;
    p.height = h;
    p.px.assign(size_t(w) * size_t(h), 0u);
    const float reach = depth * float(std::min(w, h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float edge = std::min(std::min(float(x) + 0.5f, float(w) - float(x) - 0.5f),
                                        std::min(float(y) + 0.5f, float(h) - float(y) - 0.5f));
            if (edge >= reach) continue;
            const float t = 1 - edge / reach;
            const float a = std::clamp(alpha * t * t, 0.0f, 1.0f);
            const auto ch = [&](int sh) { return uint32_t(std::lround(float((rgb >> sh) & 0xFF) * a)); };
            p.px[size_t(y) * size_t(w) + size_t(x)] =
                uint32_t(std::lround(a * 255)) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
    return p;
}

panel::Picture shadow(int w, int h, float radius, float spread, float alpha) {
    panel::Picture p;
    const int m = int(std::ceil(std::max(0.0f, spread)));
    if (w <= 0 || h <= 0 || m <= 0) return p;
    p.width = w + 2 * m;
    p.height = h + 2 * m;
    p.px.assign(size_t(p.width) * size_t(p.height), 0u);
    const float hx = float(w) / 2, hy = float(h) / 2, cx = float(m) + hx, cy = float(m) + hy;
    const float r = std::clamp(radius, 0.0f, std::min(hx, hy));
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x) {
            // the distance out of the rounded box (its signed distance, outside only)
            const float qx = std::fabs(float(x) + 0.5f - cx) - (hx - r);
            const float qy = std::fabs(float(y) + 0.5f - cy) - (hy - r);
            const float out =
                std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
            if (out <= 0 || out >= spread) continue;
            const float t = 1 - out / spread;
            const uint32_t a = uint32_t(std::lround(std::clamp(alpha * t * t, 0.0f, 1.0f) * 255));
            p.px[size_t(y) * size_t(p.width) + size_t(x)] = a << 24;  // black, premultiplied
        }
    return p;
}

}  // namespace uagfx::uistyle
