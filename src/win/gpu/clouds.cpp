#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 0.33: multiplied into the target (its samples) or the world under DLSS (one sample); 0.39.3: from their mask.
ID3D12PipelineState* Renderer::Impl::cloudsPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (cloudsPipelines[code] || !cloudsPs || !cloudsApplyPs) return cloudsPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, cloudsApplyPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
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

void Renderer::Impl::releaseCloudsMask() {
    if (cloudsMask) slots[slot].garbage.push_back(cloudsMask);  // what the open list recorded may still read it
    cloudsMask = nullptr;
    cloudsMaskW = cloudsMaskH = 0;
}

// 0.39.3: the mask the clouds are worked out into - a quarter of a big world's size, half of a smaller one's: their
// shadows are wide and soft, and their noise cost 0.55 ms a frame at 4K at the full size. False when it cannot be made
// (logged once a size).
bool Renderer::Impl::ensureCloudsMask(int w, int h) {
    if (!cloudsMaskPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p = fullScreenPipeline(root, presentVs, cloudsPs, DXGI_FORMAT_R8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&cloudsMaskPipeline)))) {
            LOG_ERROR("clouds: their mask's pipeline failed: none");
            cloudsMaskPipeline = nullptr;
            releaseCom(cloudsPs);  // not tried again
            return false;
        }
    }
    const int div = w >= 3000 ? 4 : 2;
    const int mw = std::max(1, (w + div - 1) / div), mh = std::max(1, (h + div - 1) / div);
    if (cloudsMask && mw == cloudsMaskW && mh == cloudsMaskH) return true;
    if (cloudsMaskFailed && mw == cloudsMaskW && mh == cloudsMaskH) return false;
    releaseCloudsMask();
    cloudsMaskW = mw;
    cloudsMaskH = mh;
    cloudsMask = createTexture2D(device, mw, mh, 1, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                 D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr);
    cloudsMaskState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cloudsMaskFailed = !cloudsMask;  // its size remembered as failed
    if (!cloudsMask) {
        LOG_ERROR("clouds: no video memory for their mask at %dx%d: none at this size", mw, mh);
        return false;
    }
    device->CreateRenderTargetView(cloudsMask, nullptr, rtvHandle(kRtvClouds));
    device->CreateShaderResourceView(cloudsMask, nullptr, cpuSrvHandle(cloudsMaskSrv));
    LOG_INFO("clouds: worked out at %dx%d for the %dx%d world", mw, mh, w, h);
    return true;
}

// The clouds' shadows over the ground and what stands on it - before the characters and the effects are drawn: each
// pixel's place in the world from the picture's depth and the camera, darker where the clouds' noise is high there.
bool Renderer::Impl::applyClouds() {
    cloudsDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeClouds);  // 0.39.2
    if (!ensureScene("clouds")) return true;
    ID3D12PipelineState* pso = cloudsPipeline(upscale ? 1 : samples);
    const int w = sceneWidth, h = sceneHeight;
    if (!pso || !ensureCloudsMask(w, h)) return true;
    if (!copySceneDepth()) return false;
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
    c.uiPixels[2] = float(cloudsMaskW);
    c.uiPixels[3] = float(cloudsMaskH);
    c.sharpen = waterTime();
    c.footprint = float(std::clamp(effects.cloudsStrength, 0, 100)) / 100.0f;
    if (windShared()) {  // 1.22: drifting with the shared wind
        c.sourceRect[0] = cloudsDrift.x;
        c.sourceRect[1] = cloudsDrift.y + 1e-3f;
    }
    if (!cloudsLogged) LOG_INFO("clouds: their shadows over %s", mapName.c_str());
    cloudsLogged = true;
    if (!effectPass(cloudsMaskPipeline, cloudsMask, cloudsMaskState, rtvHandle(kRtvClouds), cloudsMaskW, cloudsMaskH,
                    c, sceneDepthSrv, true))  // t0 unread: the depth, in its shader state already
        return false;
    transition(cloudsMask, cloudsMaskState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return worldPass(pso, w, h, c, cloudsMaskSrv, false);
}

}  // namespace uagfx::gpu
