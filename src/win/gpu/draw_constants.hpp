#pragma once
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "core/frame.hpp"
#include "core/game_camera.hpp"
#include "core/trees.hpp"
#include "win/gpu/shaders.hpp"

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

// 1.7, the trees (core/trees.hpp): a draw of leaves lit as a round crown, lit through towards the sun, its own shade
// and the season's colours (shaders.cpp leafLight; 2.7.10: the gusts in their sway gone). sun: the way the shadows
// fall on the screen (core/daylight.hpp).
struct LeafLook {
    bool volume = false, through = false, tints = false;
    float sun[2] = {0.83f, 0.56f};
    float throughLight = 0.0f;  // trees::throughLight
    trees::Season season = trees::Season::Green;
    const float* viewToWorld = nullptr;  // null: no camera, no shade of its own
    camera::Projection projection;
};

inline void leafShading(const FrameRecord& rec, const DrawCmd& d, const LeafLook& look, Constants& c) {
    if (!look.volume && !look.through && !look.tints) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    float x0 = FLT_MAX, y0 = FLT_MAX, x1 = -FLT_MAX, y1 = -FLT_MAX, rhw = 0.0f;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d.vertexCount && d.vertexOffset + (i + 1) * stride <= v.size(); ++i, ++n) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + i * stride, sizeof xyzw);
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3];
    }
    if (!n || x1 - x0 < 2.0f || y1 - y0 < 2.0f) return;
    rhw /= float(n);
    if (look.volume || look.through || look.tints) c.stage1[1] |= 1u << 27;
    c.sourceRect[0] = x0;
    c.sourceRect[1] = y0;
    c.sourceRect[2] = x1;
    c.sourceRect[3] = y1;
    const float len = std::sqrt(look.sun[0] * look.sun[0] + look.sun[1] * look.sun[1]);
    c.uiPixels[0] = len > 1e-4f ? -look.sun[0] / len : 0.0f;  // towards the sun
    c.uiPixels[1] = len > 1e-4f ? -look.sun[1] / len : -1.0f;
    c.uiPixels[2] = look.volume ? 1.0f : 0.0f;
    c.reproject[4] = look.through ? look.throughLight : 0.0f;
    c.sourceClamp[0] = c.sourceClamp[1] = c.sourceClamp[2] = 1.0f;
    if (!look.tints || !look.viewToWorld || rhw <= 0.0f) return;
    // 3.0.4-beta.18: the shade looked up where its leaves lie in the world (trees::crownPlace) - the box's middle on
    // the screen went round the crown as the camera turned, and the tree changed its shade with it
    float wx = 0.0f, wz = 0.0f;
    if (!trees::crownPlace(v.data() + d.vertexOffset, stride, n, look.viewToWorld, look.projection, wx, wz)) return;
    const trees::Shade sh = trees::shadeAt(wx, wz, look.season);
    for (int k = 0; k < 3; ++k) {
        c.sourceClamp[k] = sh.tint[k];
        c.reproject[k] = sh.autumn[k];
    }
    c.reproject[3] = sh.autumnAmount;
    c.reproject[5] = sh.rime;
}

}  // namespace uagfx::gpu
