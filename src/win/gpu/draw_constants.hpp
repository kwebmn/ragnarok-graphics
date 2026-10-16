#pragma once
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "core/frame.hpp"

namespace uagfx::gpu {

// 0.89: the AI's pictures of the interface (icons, the windows' pictures) only where the interface is enlarged - at
// the game's own size a picture four times larger is shrunk back with smoothing, and the game's sharp pixel icons
// came out soft (the status icons on the right, at 1600x900 on a Radeon HD 6570).
inline bool aiInterfaceAt(double scale) { return scale >= 1.2; }

// A texture stage for the shaders' stage0/stage1 (shaders.cpp): the Direct3D 12 renderer's draws and (0.46) the
// Direct3D 11 one's.
inline void packStage(const StageSetup& st, bool keyed, uint32_t out[2]) {
    out[0] = uint32_t(st.colorOp) | uint32_t(st.colorArg1) << 8 | uint32_t(st.colorArg2) << 16 |
             uint32_t(st.alphaOp) << 24;
    out[1] = uint32_t(st.alphaArg1) | uint32_t(st.alphaArg2) << 8 | uint32_t(st.texCoord) << 16 |
             uint32_t(keyed ? 1 : 0) << 24;
}

// The game pixels an interface quad covers (stage 3, review C1): Direct3D 7 draws pixel x when x0 <= x < x1.
inline void uiPixels(const FrameRecord& rec, const DrawCmd& d, float out[4]) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    float x0 = FLT_MAX, y0 = FLT_MAX, x1 = -FLT_MAX, y1 = -FLT_MAX;
    for (uint32_t i = 0; i < d.vertexCount && d.vertexOffset + (i + 1) * stride <= v.size(); ++i) {
        float xy[2];
        std::memcpy(xy, v.data() + d.vertexOffset + i * stride, sizeof xy);
        x0 = std::min(x0, xy[0]);
        y0 = std::min(y0, xy[1]);
        x1 = std::max(x1, xy[0]);
        y1 = std::max(y1, xy[1]);
    }
    out[0] = std::ceil(x0);
    out[1] = std::ceil(y0);
    out[2] = std::ceil(x1) - 1.0f;
    out[3] = std::ceil(y1) - 1.0f;
}

// 0.64: a draw's top and bottom (game pixels) - the leaves' sway (core/foliage.hpp); false when it has no height.
inline bool yExtent(const FrameRecord& rec, const DrawCmd& d, float& top, float& bottom) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    top = FLT_MAX;
    bottom = -FLT_MAX;
    for (uint32_t i = 0; i < d.vertexCount && d.vertexOffset + (i + 1) * stride <= v.size(); ++i) {
        float xy[2];
        std::memcpy(xy, v.data() + d.vertexOffset + i * stride, sizeof xy);
        top = std::min(top, xy[1]);
        bottom = std::max(bottom, xy[1]);
    }
    return bottom - top > 2.0f;
}

}  // namespace uagfx::gpu
