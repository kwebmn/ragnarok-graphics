#include "core/windows_apart.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::windowsapart {

bool overlaps(const Box& a, const Box& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

Box apart(const Box& window, const std::vector<Box>& panels, int screenW, int screenH, bool& moved) {
    moved = false;
    const auto free = [&](const Box& b) {
        return std::none_of(panels.begin(), panels.end(), [&b](const Box& p) { return p.w > 0 && p.h > 0 &&
                                                                                       overlaps(b, p); });
    };
    if (free(window) || window.w <= 0 || window.h <= 0 || window.w > screenW || window.h > screenH) return window;
    constexpr int kStep = 8;
    Box best = window;
    long bestDistance = -1;
    for (int y = 0; y + window.h <= screenH; y += kStep)
        for (int x = 0; x + window.w <= screenW; x += kStep) {
            const Box b{x, y, window.w, window.h};
            if (!free(b)) continue;
            const long d = long(std::abs(x - window.x)) * std::abs(x - window.x) +
                           long(std::abs(y - window.y)) * std::abs(y - window.y);
            if (bestDistance < 0 || d < bestDistance) {
                bestDistance = d;
                best = b;
            }
        }
    moved = bestDistance >= 0;
    return moved ? best : window;
}

}  // namespace uagfx::windowsapart
