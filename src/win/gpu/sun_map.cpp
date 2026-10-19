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

}  // namespace

// 1.12: an opaque draw of the world (not the interface, not blended: the map, its models, the crowns): its
// triangles in the world with their texture coordinates - since 2.2 for the mirror under the water (mirror.cpp).
void Renderer::Impl::noteCaster(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    if (sunVerts.size() / 6 >= kMostSunVerts || (d.primitive != d3d7::PT_TRIANGLELIST &&
                                                 d.primitive != d3d7::PT_TRIANGLESTRIP))
        return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) return;
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
    if (c.count) sunCasters.push_back(c);
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
