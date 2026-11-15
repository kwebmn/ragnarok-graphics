#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/daylight.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr size_t kMostSunVerts = 600000;
// 3.0.19-beta.26 (R-24ZKYP: from behind at moc_fild10's oasis the casters 1.85-2.42 ms a frame - every palm to the
// sight's 1200 into the world): in the 3D view a draw's first corner past this is no caster - a palm that far shows in
// the water near the horizon, hazed
constexpr float kCasterReachWide = 600.0f;

}  // namespace

// 1.12: an opaque draw of the world (not the interface, not blended: the map, its models, the crowns): its
// triangles in the world with their texture coordinates - since 2.2 for the mirror under the water (mirror.cpp).
// 3.0.20-beta.12 (stage 3 of «Модели нашим рендером»): the game's still models we drew this picture, a caster a node's
// faces of one texture (its box from the capture) - their corners in the world as noteCaster unprojects the game's;
// the casters' room and, in the 3D view, their reach as the game's draws'
void Renderer::Impl::noteGameCasters() {
    if (!mirrorCollecting() && !shadowCollecting()) return;
    const gamemodels::Frame& frame = gameModelsNow.frame();
    if (frame.empty() || !ownState.modelLikeKnown) return;
    gameCards.runsOf(frame.ids, gameRuns);
    const DrawSetup& s = ownState.modelLike;
    const bool wide = camera::wideView();
    const float eye[3] = {viewToWorld[9], viewToWorld[10], viewToWorld[11]};
    const bool leavesApart = leavesOwnWay();  // 3.0.20-beta.13: their casters noted as the game's (noteCaster)
    // a run into the casters - `toWorld` a moving node's (its own space; 3.0.20-beta.14); false: no room left
    const auto note = [&](const gamemodels::CardRun& r, const float* toWorld) {
        if (leavesApart && !leavesFromCard() && leafyTexture(r.texture)) return true;  // .33: the card's - here
        if (sunVerts.size() / 6 + size_t(r.count) * 3 > kMostSunVerts) return false;
        float lo[3], hi[3];
        if (toWorld) {
            gamemodels::boxBy(toWorld, r.lo, r.hi, lo, hi);
        } else {
            std::copy(r.lo, r.lo + 3, lo);
            std::copy(r.hi, r.hi + 3, hi);
        }
        if (wide) {  // far off in the 3D view - none, as the game's draws there
            float apart = 0.0f;
            for (int a = 0; a < 3; ++a) {
                const float d = std::max({lo[a] - eye[a], 0.0f, eye[a] - hi[a]});
                apart += d * d;
            }
            if (apart > kCasterReachWide * kCasterReachWide) return true;
        }
        SunCaster c;
        c.first = uint32_t(sunVerts.size() / 6);
        gameCards.cornersOf(r, sunVerts, toWorld);
        c.count = uint32_t(sunVerts.size() / 6) - c.first;
        if (!c.count) return true;
        c.texture = r.texture;
        c.alphaTest = s.alphaTest || s.colorKey;
        c.ref = s.alphaTest ? float(s.alphaRef) / 255.0f : 0.5f;
        std::copy(lo, lo + 3, c.lo);
        std::copy(hi, hi + 3, c.hi);
        sunCasters.push_back(c);
        return true;
    };
    for (const gamemodels::CardRun& r : gameRuns)
        if (!note(r, nullptr)) return;
    // 3.0.20-beta.14 (stage 4): the moving ones - into the world by their matrices this frame, then the camera's
    for (size_t k = 0; k < frame.moving.size() && frame.matrices.size() >= 12 * (k + 1); ++k) {
        const std::vector<gamemodels::CardRun>* runs = gameCards.runsOfNode(frame.moving[k]);
        if (!runs || !gameCards.moving(frame.moving[k])) continue;
        float toWorld[12];
        gamemodels::compose(&frame.matrices[12 * k], viewToWorld, toWorld);
        for (const gamemodels::CardRun& r : *runs)
            if (!note(r, toWorld)) return;
    }
}

void Renderer::Impl::noteCaster(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    if (sunVerts.size() / 6 >= kMostSunVerts || (d.primitive != d3d7::PT_TRIANGLELIST &&
                                                 d.primitive != d3d7::PT_TRIANGLESTRIP))
        return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) return;
    if (camera::wideView() && d.vertexCount) {  // 3.0.19-beta.26: far off in the 3D view - none
        float rhw = 0.0f;
        std::memcpy(&rhw, vb.data() + d.vertexOffset + 12, sizeof rhw);
        if (rhw > 0.0f && 1.0f / rhw > kCasterReachWide) return;
    }
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const std::vector<uint16_t>& ix = rec.indices();
    const uint32_t n = d.indexCount ? d.indexCount : d.vertexCount;
    const bool strip = d.primitive == d3d7::PT_TRIANGLESTRIP;
    const uint32_t triangles = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
    SunCaster c;
    c.first = uint32_t(sunVerts.size() / 6);
    c.texture = d.texture[0];
    c.alphaTest = s.alphaTest || s.colorKey || d.keyed[0];
    c.ref = s.alphaTest ? float(s.alphaRef) / 255.0f : 0.5f;
    const auto put = [&](uint32_t k) {
        uint32_t at = k;
        if (d.indexCount) {
            if (d.firstIndex + k >= ix.size()) return false;
            at = ix[d.firstIndex + k];
        }
        if (at >= d.vertexCount) return false;
        float v[8];
        std::memcpy(v, vb.data() + d.vertexOffset + size_t(at) * stride, sizeof(float) * 4);
        std::memcpy(v + 4, vb.data() + d.vertexOffset + size_t(at) * stride + 24, sizeof(float) * 2);
        if (v[3] <= 0.0f) return false;
        const float vz = 1.0f / v[3], vx = (v[0] - p.cx) * vz / p.f0, vy = (v[1] - p.cy) * vz / p.f1;
        float colour;  // 1.16: the game's colour, its bits as they are (the mirror's B8G8R8A8)
        std::memcpy(&colour, vb.data() + d.vertexOffset + size_t(at) * stride + 16, sizeof colour);
        const float out[6] = {vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                              vx * m[2] + vy * m[5] + vz * m[8] + m[11], v[4], v[5], colour};
        sunVerts.insert(sunVerts.end(), out, out + 6);
        return true;
    };
    for (uint32_t t = 0; t < triangles; ++t) {
        const uint32_t base = strip ? t : t * 3;
        const size_t before = sunVerts.size();
        if (!put(base) || !put(base + 1) || !put(base + 2)) sunVerts.resize(before);
    }
    c.count = uint32_t(sunVerts.size() / 6) - c.first;
    if (!c.count) return;
    for (int k = 0; k < 3; ++k) {  // 3.0.16-beta.16: its box, for the lights it can stand in the way of
        c.lo[k] = 1e30f;
        c.hi[k] = -1e30f;
    }
    for (uint32_t i = c.first; i < c.first + c.count; ++i)
        for (int k = 0; k < 3; ++k) {
            c.lo[k] = std::min(c.lo[k], sunVerts[size_t(i) * 6 + size_t(k)]);
            c.hi[k] = std::max(c.hi[k], sunVerts[size_t(i) * 6 + size_t(k)]);
        }
    sunCasters.push_back(c);
}

// 1.17: the constants of a character's sprite (shaders.cpp spriteLight): the way to the sun on the screen and its
// height, the strength, the sun's light here.
void Renderer::Impl::spriteConstants(Constants& c, int gameWidth) const {
    c.stage0[1] |= 1u << 28;
    float sun[2];
    sunOnScreen(sun);
    const float height = mapOutdoors ? sunHeight() : 1.2f;  // 2.1.3
    c.twin[0] = -sun[0];
    c.twin[1] = -sun[1];
    c.twin[2] = std::sin(height) * 0.6f;  // twinW (1.17.1: was set into twinH, the height read as 0)
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    c.uiPixels[0] = float(std::clamp(effects.spriteLightStrength, 0, 200)) / 100.0f;
    c.uiPixels[1] = mapOutdoors ? 1.0f - 0.8f * night : 0.3f;
    c.uiPixels[2] = float(gameWidth);  // 2.6.3: its edge in the game's texels, also under an AI picture
}

}  // namespace uagfx::gpu
