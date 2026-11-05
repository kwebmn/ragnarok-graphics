#include "core/net_smooth.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::netsmooth {

bool eases(int cells) { return cells >= 1 && cells <= 3; }

uint32_t glideMs(int cells) { return uint32_t(120 + 40 * std::clamp(cells, 0, 3)); }

float eased(float t) {
    const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
    return 1.0f - u * u * u;
}

bool at(const Glide& g, uint64_t nowMs, float& x, float& z) {
    const float t = g.durationMs ? float(nowMs > g.startMs ? nowMs - g.startMs : 0) / float(g.durationMs) : 1.0f;
    if (t >= 1.0f) {
        x = g.toX;
        z = g.toZ;
        return false;
    }
    const float k = eased(t);
    x = g.fromX + (g.toX - g.fromX) * k;
    z = g.fromZ + (g.toZ - g.fromZ) * k;
    return true;
}

bool easesJump(float cells) { return cells >= 0.4f && cells <= 4.0f; }

bool stranded(int shownX, int shownY, int toX, int toY, uint64_t stillMs) {
    return stillMs >= kStrandMs && std::max(std::abs(shownX - toX), std::abs(shownY - toY)) >= 1;
}

bool easesFar(float cells) { return cells >= 0.4f && cells <= 12.0f; }

uint32_t farFadeMs(float cells) { return uint32_t(120 + 35 * std::min(cells, 10.0f)); }

bool offsetAt(const Offset& o, uint64_t nowMs, float& x, float& z) {
    const float t = o.durationMs ? float(nowMs > o.startMs ? nowMs - o.startMs : 0) / float(o.durationMs) : 1.0f;
    if (t >= 1.0f) {
        x = z = 0;
        return false;
    }
    const float k = 1.0f - eased(t);
    x = o.offX * k;
    z = o.offZ * k;
    return true;
}

void cellMiddle(int cellX, int cellY, int width, int height, int cellSize, float& x, float& z) {
    x = (float(cellX - width / 2) + 0.5f) * float(cellSize);
    z = (float(cellY - height / 2) + 0.5f) * float(cellSize);
}

}  // namespace uagfx::netsmooth
