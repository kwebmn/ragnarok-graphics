#include "core/water.hpp"

#include <cmath>
#include <cstring>

namespace uagfx::water {
namespace {

// A UV on the quarter grid, 0 to 1.
bool onGrid(float v) {
    const float q = v / kStep;
    return v >= -1e-4f && v <= 1.0f + 1e-4f && std::fabs(q - std::round(q)) < 1e-3f;
}

}  // namespace

bool isWaterQuad(const uint8_t* v, size_t stride, uint32_t vertexCount, uint32_t indexCount, bool strip) {
    if (!v || !strip || vertexCount != 4 || indexCount != 0 || stride < 32) return false;
    float uv[4][2];
    for (int i = 0; i < 4; ++i) {
        uint32_t diffuse = 0;
        std::memcpy(&diffuse, v + size_t(i) * stride + 16, 4);
        if (diffuse != kDiffuse) return false;
        std::memcpy(uv[i], v + size_t(i) * stride + 24, sizeof uv[i]);
        if (!onGrid(uv[i][0]) || !onGrid(uv[i][1])) return false;
    }
    // The cell's corners in the order the game puts them: (u, v), (u + 1/4, v), (u, v + 1/4), (u + 1/4, v + 1/4).
    const auto same = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };
    return same(uv[1][0] - uv[0][0], kStep) && same(uv[1][1], uv[0][1]) && same(uv[2][0], uv[0][0]) &&
           same(uv[2][1] - uv[0][1], kStep) && same(uv[3][0], uv[1][0]) && same(uv[3][1], uv[2][1]);
}

}  // namespace uagfx::water
