#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.34: blended over the target (its samples) or the world under DLSS (one sample) by its alpha.
ID3D12PipelineState* Renderer::Impl::hazePipeline(UINT n) {
    return blendedPassPipeline(hazePs, hazePipelines, n, "haze");
}

ID3D12PipelineState* Renderer::Impl::blendedPassPipeline(ID3DBlob*& ps, ID3D12PipelineState* (&cache)[4], UINT n,
                                                         const char* user) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (cache[code] || !ps) return cache[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, ps, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&cache[code])))) {
        LOG_ERROR("%s: the pipeline onto %u samples failed: none", user, 1u << code);
        cache[code] = nullptr;
        releaseCom(ps);  // not tried again
    }
    return cache[code];
}

// Once the world is drawn and lit (before its colours): far off it goes over to the haze, over the dungeons' low
// ground a fog drifts (core/haze.hpp: by the map).
bool Renderer::Impl::applyHaze() {
    hazeDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeHaze);  // 0.39.2
    if (!ensureScene("haze")) return true;
    ID3D12PipelineState* pso = hazePipeline(upscale ? 1 : samples);
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    const float user = float(std::clamp(effects.hazeStrength, 0, 100)) / 50.0f;  // 50 % the look's own
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[0] = mapLook.hazeStart;
    c.twin[1] = mapLook.hazeEnd;
    c.twin[3] = std::min(1.0f, mapLook.haze * user);
    std::copy(mapLook.hazeColour, mapLook.hazeColour + 3, c.fogColor);
    std::copy(mapLook.fogColour, mapLook.fogColour + 3, c.sourceRect);
    c.sourceRect[3] = std::min(1.0f, mapLook.fog * user);
    c.sharpen = waterTime();
    if (!hazeLogged) LOG_INFO("haze: over %s%s", mapName.c_str(), mapLook.fog > 0.0f ? ", with a low fog" : "");
    hazeLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneDepthSrv, true)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneDepthSrv, true);
}

// 0.69: past the map's edge on a map under the open sky - the sky, its clouds, at night its stars (sky_ps).
bool Renderer::Impl::applySky() {
    skyDone = true;
    EffectTime timing(*this, kTimeHaze);
    if (!ensureScene("sky")) return true;
    ID3D12PipelineState* pso = blendedPassPipeline(skyPs, skyPipelines, upscale ? 1 : samples, "sky");
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    // The horizon in the haze's colour where the map has one (the void it filled goes over into the sky), a pale blue
    // otherwise; the zenith a deeper blue; both darker and bluer at night.
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    const bool hazy = hazeWanted() && mapLook.haze > 0.0f;
    const float pale[3] = {0.72f, 0.80f, 0.91f}, deep[3] = {0.33f, 0.52f, 0.86f}, dark[3] = {0.03f, 0.05f, 0.11f};
    for (int k = 0; k < 3; ++k) {
        const float horizon = hazy ? mapLook.hazeColour[k] : pale[k];
        c.fogColor[k] = horizon + (dark[k] * 2.0f - horizon) * night;
        c.sourceRect[k] = deep[k] + (dark[k] - deep[k]) * night;
    }
    c.twin[0] = float(std::clamp(effects.skyClouds, 0, 100)) / 100.0f;
    c.twin[1] = float(std::clamp(effects.skyStrength, 0, 100)) / 100.0f;
    c.twin[3] = night;
    c.sharpen = waterTime();
    if (!skyLogged) LOG_INFO("sky: over %s", mapName.c_str());
    skyLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneDepthSrv, true)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneDepthSrv, true);
}

}  // namespace uagfx::gpu
