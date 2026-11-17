#include "core/gauge_scale.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace uagfx::gaugescale {
namespace {

// the game's gauges (core/world_windows): an actor's HP/SP one, its other, its cast's
constexpr uint32_t kGauges[] = {0x696D4C, 0x696CB4, 0x696C18};

}  // namespace

float scaleAt(float distance) {
    if (!std::isfinite(distance) || distance <= kFullAt) return 1.0f;
    return std::max(kLeast, kFullAt / distance);
}

void scaleVertices(uint8_t* vertices, uint32_t count, size_t stride, float ax, float ay, float scale) {
    if (!vertices || stride < 8) return;
    for (uint32_t i = 0; i < count; ++i) {
        float xy[2];
        std::memcpy(xy, vertices + size_t(i) * stride, sizeof xy);
        xy[0] = ax + (xy[0] - ax) * scale;
        xy[1] = ay + (xy[1] - ay) * scale;
        std::memcpy(vertices + size_t(i) * stride, xy, sizeof xy);
    }
}

int boxOf(const uint8_t* vertices, uint32_t count, size_t stride, const std::vector<std::array<int, 4>>& boxes) {
    if (!vertices || !count || stride < 8) return -1;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (uint32_t i = 0; i < count; ++i) {
        float xy[2];
        std::memcpy(xy, vertices + size_t(i) * stride, sizeof xy);
        if (!std::isfinite(xy[0]) || !std::isfinite(xy[1])) return -1;
        x0 = i ? std::min(x0, xy[0]) : xy[0];
        y0 = i ? std::min(y0, xy[1]) : xy[1];
        x1 = i ? std::max(x1, xy[0]) : xy[0];
        y1 = i ? std::max(y1, xy[1]) : xy[1];
    }
    const float mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
    for (size_t b = 0; b < boxes.size(); ++b) {
        const auto& [x, y, w, h] = boxes[b];
        // the face within the box, a pixel round it
        if (x0 >= float(x) - 1.5f && x1 <= float(x + w) + 1.5f && y0 >= float(y) - 1.5f &&
            y1 <= float(y + h) + 1.5f && mx >= float(x) && mx <= float(x + w) && my >= float(y) &&
            my <= float(y + h))
            return int(b);
    }
    return -1;
}

std::vector<std::array<float, 2>> anchors(const std::vector<std::array<int, 4>>& boxes) {
    std::vector<std::array<float, 2>> out(boxes.size());
    for (size_t i = 0; i < boxes.size(); ++i) {
        // up the stack: the gauge right over this one, while there is one
        size_t top = i;
        for (size_t steps = 0; steps < boxes.size(); ++steps) {
            const auto& [x, y, w, h] = boxes[top];
            size_t over = top;
            for (size_t j = 0; j < boxes.size(); ++j) {
                const auto& [ox, oy, ow, oh] = boxes[j];
                const int below = y - (oy + oh);  // the gap from its bottom down to this one's top
                if (j != top && std::abs((ox * 2 + ow) - (x * 2 + w)) <= 2 && below >= 0 && below <= 3 && oy < y) {
                    over = j;
                    break;
                }
            }
            if (over == top) break;
            top = over;
        }
        out[i] = {float(boxes[top][0]) + float(boxes[top][2]) / 2, float(boxes[top][1])};
    }
    return out;
}

bool gaugeClass(uint32_t vtable) {
    return std::find(std::begin(kGauges), std::end(kGauges), vtable) != std::end(kGauges);
}

}  // namespace uagfx::gaugescale
