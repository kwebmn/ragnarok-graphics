#include "core/loading_bar.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::loadingbar {

Layout layout(int x, int y, int w, int h, double k, float progress) {
    Layout L;
    const float s = k > 0 ? float(k) : 1.0f;
    L.track = panel::Box{float(x) * s, float(y) * s, float(std::max(w, 0)) * s, float(std::max(h, 0)) * s};
    L.radius = L.track.h / 2;
    L.edge = std::max(1.0f, s * 0.75f);
    const float p = std::isfinite(progress) ? std::clamp(progress, 0.0f, 100.0f) : 0.0f;
    const float inset = std::min(std::max(1.0f, 2 * s), L.track.h / 4);  // the fill a little in from the edge
    const float innerW = std::max(0.0f, L.track.w - 2 * inset), innerH = std::max(0.0f, L.track.h - 2 * inset);
    L.fill = panel::Box{L.track.x + inset, L.track.y + inset, innerW * p / 100.0f, innerH};
    L.percent = std::to_string(int(p)) + "%";
    L.px = std::clamp(L.track.h * 0.6f, 8.5f, 22.0f);
    return L;
}

}  // namespace uagfx::loadingbar
