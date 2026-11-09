#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/glint.hpp"

namespace uagfx::uaui {

// 3.0.18-beta.7 (the RmlUi spike, Claude Doc «Концепция: единая библиотека интерфейса uaRO Graphics»): RmlUi's
// geometry as the game's own pretransformed vertices. A point in the target's pixels, its colour premultiplied ARGB.
struct Point {
    float x = 0, y = 0;
    uint32_t argb = 0;
    float u = 0, v = 0;
};
// The scissor in the target's pixels: [x0, x1) x [y0, y1)
struct ClipBox {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

// One draw of RmlUi's: its triangles (`indices`, three a triangle) moved by (dx, dy), through `m` (RmlUi's 4x4
// column-major transform; null none), clipped to `clip` (null none; a cut triangle a fan, its colour and uv
// interpolated), then into the game's pixels - (x - half) / k, as the driver's pictures (win/picture_draw) - and
// appended to `out` as a triangle list (z 0, rhw 1)
void triangles(const std::vector<Point>& points, const std::vector<int>& indices, float dx, float dy, const float* m,
               const ClipBox* clip, float k, float half, std::vector<glint::Vertex>& out);

// UTF-8 (RmlUi's strings) as code points; a byte that starts no character is U+FFFD
std::u32string codepoints(std::string_view text);

}  // namespace uagfx::uaui
