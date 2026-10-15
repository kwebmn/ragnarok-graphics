#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.34: blended over the target (its samples) or the world under DLSS (one sample) by its alpha.
ID3D12PipelineState* Renderer::Impl::hazePipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (hazePipelines[code] || !hazePs) return hazePipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, hazePs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
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
                                                   reinterpret_cast<void**>(&hazePipelines[code])))) {
        LOG_ERROR("haze: the pipeline onto %u samples failed: none", 1u << code);
        hazePipelines[code] = nullptr;
        releaseCom(hazePs);  // not tried again
    }
    return hazePipelines[code];
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

}  // namespace uagfx::gpu
