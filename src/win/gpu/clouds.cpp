#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.33: multiplied into the target (its samples) or the world under DLSS (one sample).
ID3D12PipelineState* Renderer::Impl::cloudsPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (cloudsPipelines[code] || !cloudsPs) return cloudsPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, cloudsPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;  // the picture times the light the clouds leave
    b.SrcBlend = D3D12_BLEND_ZERO;
    b.DestBlend = D3D12_BLEND_SRC_COLOR;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&cloudsPipelines[code])))) {
        LOG_ERROR("clouds: the pipeline onto %u samples failed: none", 1u << code);
        cloudsPipelines[code] = nullptr;
        releaseCom(cloudsPs);  // not tried again
    }
    return cloudsPipelines[code];
}

// The clouds' shadows over the ground and what stands on it - before the characters and the effects are drawn: each
// pixel's place in the world from the picture's depth and the camera, darker where the clouds' noise is high there.
bool Renderer::Impl::applyClouds() {
    cloudsDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeClouds);  // 0.39.2
    if (!ensureScene("clouds")) return true;
    ID3D12PipelineState* pso = cloudsPipeline(upscale ? 1 : samples);
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
    c.sharpen = waterTime();
    c.footprint = float(std::clamp(effects.cloudsStrength, 0, 100)) / 100.0f;
    if (!cloudsLogged) LOG_INFO("clouds: their shadows over %s", mapName.c_str());
    cloudsLogged = true;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneDepthSrv, true)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneDepthSrv, true);
}

}  // namespace uagfx::gpu
