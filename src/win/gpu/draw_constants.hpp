#pragma once
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "core/critters.hpp"
#include "core/foliage.hpp"
#include "core/frame.hpp"
#include "core/game_camera.hpp"
#include "core/game_models_card.hpp"
#include "core/own_ground.hpp"
#include "core/model_fade.hpp"
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

// 3.0.13-beta.30: [effects] silhouette - a sprite of the game's sprite cache (spriteTexture) standing where the
// character stands: its feet at the hole's, its depth the character's. The draw kept with a state that draws it only
// where something nearer hides it, laid over, writing no depth (shaders.cpp seeThrough's silhouette)
inline bool ownSprite(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s, const float hole[8],
                      bool spriteTexture) {
    if (hole[3] <= 0.0f || !spriteTexture || s.ui || !s.blend || d.fvf != d3d7::FVF_TL1 || d.vertexCount != 4)
        return false;
    // 3.0.17-beta.21 (the owner: «этот же силует появляется на асумтио ... и на других еффектах»): a game effect's
    // picture round the character (Assumptio's, a buff's) is not the character - its back half, behind the body, came
    // out blue over the character; nor one added on as light
    if (d.effect != 0 || d.effectId != 0 || s.dstBlend != d3d7::BLEND_INVSRCALPHA) return false;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > v.size()) return false;
    float x0 = FLT_MAX, y0 = FLT_MAX, x1 = -FLT_MAX, y1 = -FLT_MAX, rhw = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + i * stride, sizeof xyzw);
        // 3.0.17-beta.23 (the owner: «под two hand quicken включается еффект и когда ходишь появляется силует»): a
        // see-through copy of the body (a buff's trail behind the walking character) is not the character - its
        // vertices' alpha under the body's 255
        if (v[d.vertexOffset + i * stride + 19] < 0xF0) return false;
        x0 = std::min(x0, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        x1 = std::max(x1, xyzw[0]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3] / 4.0f;
    }
    const float h = hole[2];
    // .33 (the owner: «когда идет не видно шеи, а когда останавливается видно» - the camera's target, the place
    // told, trails the walking character; a narrow head missed it): over a band round it, not across it exactly
    return rhw > 0.0f && std::fabs(1.0f / rhw - hole[3]) < 25.0f && x0 < hole[0] + 0.4f * h &&
           x1 > hole[0] - 0.4f * h && y1 > hole[1] - 1.1f * h && y0 < hole[1] + 0.4f * h;
}
// 3.0.13-beta.32 (R-83PRCU: «кусок силуета налез на саму текстуру персонажа» - the ground by the feet a little nearer
// than the sprite's bottom): the silhouette's vertices kSilhouetteMargin nearer the camera - it shows only where what
// hides the character is that much nearer (z by the game's projection, camera::kNear/kFar; rhw likewise)
// .33 (the owner: «иногда там где шея у персонажа не видно ничего» - a wall's top right before the character hid it,
// less than 8 nearer): the margin at the feet only (a vertex within a fifth of the body over them), kSilhouetteUp above
constexpr float kSilhouetteMargin = 8.0f, kSilhouetteUp = 2.0f;
// 3.0.17-beta.24 (R-BY7DGC: a light outline on the character's own head - its body's silhouette, the head 7.6 nearer
// than the body hid it): `nearestW` the character's nearest sprite this picture - none drawn farther than that less
// kSilhouetteUp, so no layer of the character hides another; 0: none known
inline void silhouetteVertices(const uint8_t* src, uint32_t count, size_t stride, const float hole[8],
                               std::vector<uint8_t>& out, float nearestW = 0.0f) {
    out.assign(src, src + size_t(count) * stride);
    const auto depthOf = [](float w) {
        return camera::kFar / (camera::kFar - camera::kNear) * (1.0f - camera::kNear / w);
    };
    for (uint32_t i = 0; i < count; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, out.data() + i * stride, sizeof xyzw);
        if (!(xyzw[3] > 0.0f)) continue;
        const float margin = xyzw[1] >= hole[1] - 0.25f * hole[2] ? kSilhouetteMargin : kSilhouetteUp;
        const float w = 1.0f / xyzw[3];
        const float own = nearestW > 0.0f ? std::min(w - margin, nearestW - kSilhouetteUp) : w - margin;
        const float nearer = std::max(own, camera::kNear + 1.0f);
        xyzw[2] = std::max(0.0f, xyzw[2] - (depthOf(w) - depthOf(nearer)));
        xyzw[3] = 1.0f / nearer;
        std::memcpy(out.data() + i * stride, xyzw, sizeof xyzw);
    }
}
// 3.0.17-beta.24: the character's sprites noted this picture (their own vertices) made into the silhouette's draws
inline void buildSilhouettes(const FrameRecord& raw, const float hole[8], FrameRecord& out) {
    const std::vector<uint8_t>& v = raw.vertexBytes();
    float nearest = FLT_MAX;
    for (const DrawCmd& d : raw.draws()) {
        const size_t stride = FrameRecord::stride(d.fvf);
        for (uint32_t i = 0; i < d.vertexCount && d.vertexOffset + (i + 1) * stride <= v.size(); ++i) {
            float rhw = 0.0f;
            std::memcpy(&rhw, v.data() + d.vertexOffset + i * stride + 12, sizeof rhw);
            if (rhw > 0.0f) nearest = std::min(nearest, 1.0f / rhw);
        }
    }
    if (nearest == FLT_MAX) nearest = 0.0f;
    std::vector<uint8_t> moved;
    for (const DrawCmd& d : raw.draws()) {
        const size_t stride = FrameRecord::stride(d.fvf);
        if (d.vertexOffset + size_t(d.vertexCount) * stride > v.size() || d.state >= raw.states().size() ||
            size_t(d.firstIndex) + d.indexCount > raw.indices().size())
            continue;
        silhouetteVertices(v.data() + d.vertexOffset, d.vertexCount, stride, hole, moved, nearest);
        DrawCmd c = d;
        c.state = out.addState(raw.states()[d.state]);
        out.draw(c, moved.data(), d.vertexCount, d.indexCount ? raw.indices().data() + d.firstIndex : nullptr,
                 d.indexCount);
    }
}
// a quad's box (x0, y0, x1, y1, game pixels) and mean depth - for the log
inline bool spriteBox(const FrameRecord& rec, const DrawCmd& d, float box[4], float& depth) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexCount != 4 || d.vertexOffset + 4 * stride > v.size()) return false;
    box[0] = box[1] = FLT_MAX;
    box[2] = box[3] = -FLT_MAX;
    float rhw = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, v.data() + d.vertexOffset + i * stride, sizeof xyzw);
        box[0] = std::min(box[0], xyzw[0]);
        box[1] = std::min(box[1], xyzw[1]);
        box[2] = std::max(box[2], xyzw[0]);
        box[3] = std::max(box[3], xyzw[1]);
        rhw += xyzw[3] / 4.0f;
    }
    depth = rhw > 0.0f ? 1.0f / rhw : 0.0f;
    return rhw > 0.0f;
}
inline DrawSetup silhouetteSetup(DrawSetup s) {
    s.zEnable = true;
    s.zWrite = false;
    s.zFunc = d3d7::CMP_GREATER;  // only where something nearer is drawn
    s.blend = true;
    s.srcBlend = d3d7::BLEND_SRCALPHA;
    s.dstBlend = d3d7::BLEND_INVSRCALPHA;
    return s;
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
// 3.0.20-beta.48: a draw's first corner's depth in the view (its vertices x, y, z and 1 / depth) - 0 none
inline float firstDepth(const FrameRecord& rec, const DrawCmd& d) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    if (!d.vertexCount || d.vertexOffset + 16 > v.size()) return 0.0f;
    float rhw = 0.0f;
    std::memcpy(&rhw, v.data() + d.vertexOffset + 12, sizeof rhw);
    return rhw > 1e-6f ? 1.0f / rhw : 0.0f;
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

// 3.0.20-beta.33 (the owner's «6,7,8,9», 8 - the leaves on the card): a crown's look by its box on the game's screen
// (x0 y0 - x1 y1) and its place in the world (crown: x, z; none - no shade of its own) - leafShading's from a draw's
// vertices, the card's leaves' from their node's box
inline void leafShadingOf(float x0, float y0, float x1, float y1, const float* crown, const LeafLook& look,
                          Constants& c) {
    if (!look.volume && !look.through && !look.tints) return;
    if (x1 - x0 < 2.0f || y1 - y0 < 2.0f) return;
    c.stage1[1] |= 1u << 27;
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
    if (!look.tints || !crown) return;
    const trees::Shade sh = trees::shadeAt(crown[0], crown[1], look.season);
    for (int k = 0; k < 3; ++k) {
        c.sourceClamp[k] = sh.tint[k];
        c.reproject[k] = sh.autumn[k];
    }
    c.reproject[3] = sh.autumnAmount;
    c.reproject[5] = sh.rime;
}

// 3.0.20-beta.34: a tree of the card's leaves (Direct3D 12's drawGameLeavesCard, Direct3D 11's drawGameLeaves11) - its
// leafy runs' box (toView: a moving node's matrix, node -> view; none: still, its runs in the world), its eight corners
// onto the game's screen: its sway (swing 0: none; the time) and its crown's look (leafShadingOf) into `out`; the crown
// the falling leaves start from into `crown` when given (false then: none). False: no leafy run, or none of its box
// on the screen (behind the eye, off it) - not drawn. 3.0.20-beta.79: `crownSun` the way to the sun in the world when a
// still tree's crown is lit by its shape ([view3d] crown_light, leaf_vs crownLight; null: not)
template <typename Leafy>
inline bool cardTreeLook(const std::vector<gamemodels::CardRun>& runs, const Leafy& leafy, const float* toView,
                         const own_ground::View& view, const camera::Projection& p, const float* viewToWorld,
                         const LeafLook& look, float swing, float time, Constants& out, critters::Crown* crown,
                         bool* crownSeen, const float* crownSun = nullptr) {
    if (crownSeen) *crownSeen = false;
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    for (const gamemodels::CardRun& r : runs) {
        if (!r.count || !leafy(r.texture)) continue;
        any = true;
        for (int a = 0; a < 3; ++a) lo[a] = std::min(lo[a], r.lo[a]), hi[a] = std::max(hi[a], r.hi[a]);
    }
    if (!any) return false;
    // its corners in the view; 3.0.20-beta.40: its place on the screen the part of it in front of the near plane
    // (gamemodels::screenBoxOf) - a tree by the eye, a corner of its box behind it, kept neither sway nor look
    float corners[8][3], mid[3] = {0.0f, 0.0f, 0.0f};
    for (int k = 0; k < 8; ++k) {
        const float w[3] = {k & 1 ? hi[0] : lo[0], k & 2 ? hi[1] : lo[1], k & 4 ? hi[2] : lo[2]};
        if (toView) {
            for (int a = 0; a < 3; ++a)
                corners[k][a] = w[0] * toView[a] + w[1] * toView[3 + a] + w[2] * toView[6 + a] + toView[9 + a];
        } else {
            own_ground::toView(view, w, corners[k]);
        }
        for (int a = 0; a < 3; ++a) mid[a] += corners[k][a] * 0.125f;
    }
    float box[4];
    if (!gamemodels::screenBoxOf(corners, p, box)) return false;  // all of it behind the eye: no face of it shows
    const float x0 = box[0], y0 = box[1], x1 = box[2], y1 = box[3];
    // off the screen whole (its sway's reach and a little more round it): none of it shows either
    // 3.0.20-beta.48: in the 3D view as far in the world wherever the tree stands (foliage::swingAt by its depth)
    if (swing > 0.0f && camera::wideView()) swing = foliage::swingAt(swing, mid[2], camera::fovNow());
    const float reach = swing + 8.0f;
    if (x1 < -reach || y1 < -reach || x0 > 2.0f * p.cx + 1.0f + reach || y0 > 2.0f * p.cy + 1.0f + reach) return false;
    out = Constants{};
    if (swing > 0.0f) {  // as the game's leaves': the swing, the crown's bottom and height, the time
        out.twin[0] = swing;
        out.twin[1] = y1;
        out.twin[2] = y1 - y0;
        out.twin[3] = time;
    }
    const float* m = viewToWorld;  // its middle back into the world: the view's axes in it, then its place
    const float world[3] = {m[9] + mid[0] * m[0] + mid[1] * m[3] + mid[2] * m[6],
                            m[10] + mid[0] * m[1] + mid[1] * m[4] + mid[2] * m[7],
                            m[11] + mid[0] * m[2] + mid[1] * m[5] + mid[2] * m[8]};
    const float place[2] = {world[0], world[2]};
    leafShadingOf(x0, y0, x1, y1, place, look, out);
    // 3.0.20-beta.51: the crown's middle in the world (leaf_vs r6-r8) - its leaves drawn again turn about it (r9-r11
    // the turn, none here: foliage::crownTurn by the caller)
    for (int k = 0; k < 3; ++k) out.reproject[6 + k] = world[k];
    // 3.0.20-beta.79 ([view3d] crown_light): a still tree's crown lit by its shape in the world (leaf_vs crownLight) -
    // its box's half sizes across and up (footprint, sharpen), the way to the sun (hole[5], hole[6], sourceClamp[3]:
    // holeUy, holeUz, sourceClamp.w); leafLight leaves its round light on the screen to it (uiPixels.w). The renderers
    // copy them with the crown's look (crownLightOf)
    // (a crown of no height or width - its leaves on one plate - none: leaf_vs divides by them)
    const float across = std::max(hi[0] - lo[0], hi[2] - lo[2]) * 0.5f, up = (hi[1] - lo[1]) * 0.5f;
    if (crownSun && !toView && across > foliage::kCrownLeastHalf && up > foliage::kCrownLeastHalf) {
        out.footprint = across;
        out.sharpen = up;
        out.hole[5] = crownSun[0];
        out.hole[6] = crownSun[1];
        out.sourceClamp[3] = crownSun[2];
        out.uiPixels[3] = 1.0f;
    }
    if (crown && crownSeen && x1 - x0 >= 8.0f && mid[2] > 0.0f) {  // as noteCrown's for the game's leaves
        crown->centre = critters::Point{world[0], world[1], world[2]};
        crown->radius = std::clamp((x1 - x0) * 0.5f * mid[2] / p.f0, 1.0f, 12.0f);
        *crownSeen = true;
    }
    return true;
}

// 3.0.20-beta.79: a card tree's crown light (cardTreeLook) into a draw's constants - after its fog's hole[] is set
inline void crownLightOf(const Constants& look, Constants& c) {
    c.footprint = look.footprint;
    c.sharpen = look.sharpen;
    c.uiPixels[3] = look.uiPixels[3];
    c.sourceClamp[3] = look.sourceClamp[3];
    c.hole[5] = look.hole[5];
    c.hole[6] = look.hole[6];
}

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
    if (!n) return;
    rhw /= float(n);
    // 3.0.4-beta.18: the shade looked up where its leaves lie in the world (trees::crownPlace) - the box's middle on
    // the screen went round the crown as the camera turned, and the tree changed its shade with it
    float crown[2] = {0.0f, 0.0f};
    const bool placed = look.tints && look.viewToWorld && rhw > 0.0f &&
                        trees::crownPlace(v.data() + d.vertexOffset, stride, n, look.viewToWorld, look.projection,
                                          crown[0], crown[1]);
    leafShadingOf(x0, y0, x1, y1, placed ? crown : nullptr, look, c);
}

}  // namespace uagfx::gpu
