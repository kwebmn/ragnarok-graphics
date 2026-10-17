#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/daylight.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 1.8, the materials (effects.materials): for a texture of the world its relief, height and gloss (material_ps) - made
// on the card from the picture drawn (the game's or the AI's), at most `materialSide` a side, a level for each of its
// own, once; a draw of the ground or a model is then lit by them (shaders.cpp shade(), bit 28). A few a frame.
ID3D12PipelineState* Renderer::Impl::materialPipeline() {
    if (materialPso || !materialPs) return materialPso;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, materialPs, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&materialPso)))) {
        LOG_ERROR("materials: the pipeline failed: none");
        materialPso = nullptr;
        releaseCom(materialPs);
    }
    return materialPso;
}

void Renderer::Impl::dropMaterial(Texture& t) {
    if (!t.mat) return;
    slots[slot].garbage.push_back(t.mat);
    freeSrv.push_back(t.matSrv);
    t.mat = nullptr;
    t.matSrv = 0;
    t.matFor = nullptr;
}

bool Renderer::Impl::ensureMaterial(Texture& t, bool fromAi) {
    ID3D12Resource* source = fromAi ? t.ai : t.resource;
    if (!source) return false;
    if (t.mat && t.matFor == source) return true;
    if (materialsMade >= 2 || !materialPipeline()) return false;  // the rest in the next frames
    ++materialsMade;
    crumb("materials");
    dropMaterial(t);
    const D3D12_RESOURCE_DESC sd = source->GetDesc();
    const int sw = int(sd.Width), sh = int(sd.Height), sourceLevels = std::max<int>(1, sd.MipLevels);
    // The material's size: the picture's, halved down to materialSide; `shift` source levels above its first.
    int shift = 0;
    while ((sw >> shift) > materialSide || (sh >> shift) > materialSide) ++shift;
    const int mw = std::max(1, sw >> shift), mh = std::max(1, sh >> shift);
    int levels = 1;
    while ((mw >> levels) >= 4 && (mh >> levels) >= 4) ++levels;  // 1.8.1: also past the source's own levels
    ID3D12Resource* mat = createTexture2D(device, mw, mh, levels, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                          D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr);
    const UINT srvIndex = mat ? allocSrv() : kNoSrv;
    if (!mat || srvIndex == kNoSrv) {
        releaseCom(mat);
        return false;
    }
    device->CreateShaderResourceView(mat, nullptr, cpuSrvHandle(srvIndex));
    t.mat = mat;
    t.matSrv = srvIndex;
    t.matState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    const UINT sourceSrv = fromAi ? t.aiSrv : t.srv;
    if (fromAi)
        transition(t.ai, t.aiState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    else
        transition(t.resource, t.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    for (int level = 0; level < levels; ++level) {
        D3D12_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rd.Texture2D.MipSlice = UINT(level);
        device->CreateRenderTargetView(mat, &rd, rtvHandle(kRtvMaterial));  // read as the pass is recorded
        const int lw = std::max(1, mw >> level), lh = std::max(1, mh >> level);
        Constants c{};
        c.uiPixels[0] = float(lw);
        c.uiPixels[1] = float(lh);
        c.uiPixels[2] = float(level + shift);  // the source's level it is made from
        c.uiPixels[3] = float(sourceLevels);
        c.sourceRect[0] = float(sw);
        c.sourceRect[1] = float(sh);
        if (!effectPass(materialPso, t.mat, t.matState, rtvHandle(kRtvMaterial), lw, lh, c, sourceSrv, false)) {
            dropMaterial(t);  // tried again later
            return false;
        }
    }
    transition(t.mat, t.matState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    t.matFor = source;  // whole: drawn with from now on
    if (!materialsLogged)
        LOG_INFO("materials: made on the card from the pictures, %dx%d and smaller (%d levels)", mw, mh, levels);
    materialsLogged = true;
    return true;
}

// The constants of a draw lit by its material (shaders.cpp shade(), materialLight): bits 28-30, the game's projection
// and camera, the way to the sun in the view (as the sun's shadows find it: indoors from above), the strength.
void Renderer::Impl::materialConstants(bool ground, Constants& c) const {
    c.stage1[1] |= 1u << 28;
    c.stage1[1] &= ~(1u << 26);  // the relief of 0.71 is the material's now
    if (remaster()) c.stage1[1] |= 1u << 29;
    if (ground) c.stage1[1] |= 1u << 30;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
    std::memcpy(c.projection, proj, sizeof proj);
    static const float kSame[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    std::memcpy(c.reproject, viewToWorldValid ? viewToWorld : kSame, sizeof c.reproject);
    const float* m = viewToWorldValid ? viewToWorld : kSame;
    float toSun[3];
    sunInWorld(toSun);
    for (int k = 0; k < 3; ++k) c.twin[k] = toSun[0] * m[3 * k] + toSun[1] * m[3 * k + 1] + toSun[2] * m[3 * k + 2];
    c.twin[3] = float(std::clamp(effects.materialsStrength, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    c.uiPixels[0] = mapOutdoors ? 1.0f - night : 0.0f;  // the sun's light here
    c.uiPixels[1] = 1.0f;
    // 1.21: the windows lit as the night comes (night_windows; outdoors)
    if (effects.nightWindows && mapOutdoors) c.uiPixels[2] = std::clamp((night - 0.3f) / 0.4f, 0.0f, 1.0f);
}

// 1.15: the way to the sun in the world (up along -y): along the ground where the shadows fall on the screen,
// turned into the world by the view's axes, at the sun's height by the time of day (indoors from above).
void Renderer::Impl::sunInWorld(float out[3]) const {
    const daylight::Light day = daylight::at(effects.dayCycle ? effects.dayNow : -1);
    const float height = (mapOutdoors ? std::clamp(50.0f / day.length * 0.55f, 15.0f, 70.0f) : 75.0f) * 0.0174533f;
    float g[3];
    sunGround(g);  // 2.0.1: fixed in the world
    const float gl = std::max(std::sqrt(g[0] * g[0] + g[2] * g[2]), 1e-4f);
    out[0] = -g[0] / gl * std::cos(height);
    out[1] = -std::sin(height);
    out[2] = -g[2] / gl * std::cos(height);
}

}  // namespace uagfx::gpu
