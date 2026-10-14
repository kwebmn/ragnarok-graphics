#include "core/ai/kinds.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::ai {

const char* kindName(Kind k) {
    switch (k) {
        case Kind::Ground: return "ground";
        case Kind::Objects: return "objects";
        case Kind::Sprites: return "sprites";
        case Kind::Effects: return "effects";
        case Kind::Icons: return "icons";
        case Kind::Lightmap:
        case Kind::None: break;
    }
    return "";
}

bool parseKind(const std::string& name, Kind& out) {
    for (int i = 0; i < kKinds; ++i)
        if (name == kindName(Kind(i))) {
            out = Kind(i);
            return true;
        }
    return false;
}

Kind drawKind(const DrawFacts& f) {
    if (f.stage != 0) return f.uvSets >= 2 ? Kind::Lightmap : Kind::None;
    if (f.additive) return Kind::Effects;
    if (f.uvSets >= 2) return Kind::Ground;
    return f.flatQuad ? Kind::Sprites : Kind::Objects;
}

bool flatQuad(const uint8_t* vertices, size_t stride, uint32_t vertexCount, uint32_t indexCount) {
    if (!vertices || stride < 16 || vertexCount != 4 || (indexCount != 0 && indexCount != 6)) return false;
    float p[4][4];  // x, y, z, rhw
    for (int i = 0; i < 4; ++i) std::memcpy(p[i], vertices + size_t(i) * stride, sizeof p[i]);
    const auto same = [](float a, float b) { return std::fabs(a - b) <= 1e-5f * std::max(1.0f, std::fabs(a)); };
    bool oneDepth = true;
    for (int i = 1; i < 4; ++i) oneDepth = oneDepth && same(p[i][2], p[0][2]) && same(p[i][3], p[0][3]);
    if (oneDepth) return true;
    // A rectangle along the axes: two x values and two y values, each corner a pair of them.
    float xs[2] = {p[0][0], p[0][0]}, ys[2] = {p[0][1], p[0][1]};
    for (int i = 1; i < 4; ++i) {
        xs[0] = std::min(xs[0], p[i][0]);
        xs[1] = std::max(xs[1], p[i][0]);
        ys[0] = std::min(ys[0], p[i][1]);
        ys[1] = std::max(ys[1], p[i][1]);
    }
    if (same(xs[0], xs[1]) || same(ys[0], ys[1])) return false;
    int corners = 0;
    for (int i = 0; i < 4; ++i) {
        const bool x = same(p[i][0], xs[0]) || same(p[i][0], xs[1]);
        const bool y = same(p[i][1], ys[0]) || same(p[i][1], ys[1]);
        if (!x || !y) return false;
        corners |= 1 << ((same(p[i][0], xs[1]) ? 1 : 0) | (same(p[i][1], ys[1]) ? 2 : 0));
    }
    return corners == 15;
}

}  // namespace uagfx::ai
