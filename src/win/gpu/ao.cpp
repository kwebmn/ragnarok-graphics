#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

D3D12_GRAPHICS_PIPELINE_STATE_DESC fullScreenPipeline(ID3D12RootSignature* root, ID3DBlob* vs, ID3DBlob* ps,
                                                      DXGI_FORMAT format, UINT samples) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = format;
    p.SampleDesc.Count = samples;
    return p;
}

}  // namespace

bool Renderer::Impl::effectPass(ID3D12PipelineState* pso, ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState,
                                D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, int dstW, int dstH, Constants& c, UINT srv0,
                                bool scene) {
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp, 0, scene ? 5 : 2)) return false;  // before the pass records anything
    placeSamples(false);
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
    const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(srv0, srv0);
    transition(dst, dstState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(dstW), float(dstH), 0.0f, 1.0f};
    const D3D12_RECT clip{0, 0, dstW, dstH};
    list->OMSetRenderTargets(1, &dstRtv, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &clip);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(pso);
    c.targetSize[0] = float(dstW);
    c.targetSize[1] = float(dstH);
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
    if (scene) list->SetGraphicsRootDescriptorTable(kRootScene, sceneTable(sceneColorSrv, sceneDepthSrv, sceneColorSrv));
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    return true;
}

void Renderer::Impl::releaseAo() {
    Slot& s = slots[slot];  // what the open list recorded may still read them
    for (ID3D12Resource*& r : aoTarget) {
        if (r) s.garbage.push_back(r);
        r = nullptr;
    }
    aoWidth = aoHeight = 0;
}

// The shadows' two half-sized R8 targets and the pipelines; false when they cannot be made (logged once a size).
bool Renderer::Impl::ensureAo(int w, int h) {
    if (!aoPs || !aoBlurPs || !aoApplyPs) return false;  // its shaders did not compile (logged at the start)
    if (!aoPipeline) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p = fullScreenPipeline(root, presentVs, aoPs, DXGI_FORMAT_R8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&aoPipeline))))
            aoPipeline = nullptr;
        p = fullScreenPipeline(root, presentVs, aoBlurPs, DXGI_FORMAT_R8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&aoBlurPipeline))))
            aoBlurPipeline = nullptr;
        if (!aoPipeline || !aoBlurPipeline) {
            LOG_ERROR("shadows: their pipelines failed: they stay off");
            return false;
        }
    }
    const int aw = std::max(1, (w + 1) / 2), ah = std::max(1, (h + 1) / 2);
    if (aoTarget[0] && aw == aoWidth && ah == aoHeight) return true;
    if (aoFailed && aw == aoWidth && ah == aoHeight) return false;
    releaseAo();
    aoWidth = aw;
    aoHeight = ah;
    aoFailed = true;
    D3D12_CLEAR_VALUE open{};
    open.Format = DXGI_FORMAT_R8_UNORM;
    open.Color[0] = 1.0f;
    for (int i = 0; i < 2; ++i) {
        aoTarget[i] = createTexture2D(device, aw, ah, 1, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                      D3D12_RESOURCE_STATE_RENDER_TARGET, &open);
        aoState[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (!aoTarget[i]) {
            LOG_ERROR("shadows: no video memory for them at %dx%d: they stay off at this size", aw, ah);
            releaseAo();
            aoWidth = aw;  // remembered as failed
            aoHeight = ah;
            return false;
        }
        device->CreateRenderTargetView(aoTarget[i], nullptr, rtvHandle(kRtvAo + UINT(i)));
        device->CreateShaderResourceView(aoTarget[i], nullptr, cpuSrvHandle(aoSrv[i]));
    }
    aoFailed = false;
    LOG_INFO("shadows: %dx%d, from the depth of the %dx%d world", aw, ah, w, h);
    return true;
}

// Multiplied into the target (its samples) or the world under DLSS (one sample).
ID3D12PipelineState* Renderer::Impl::aoApplyPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (aoApplyPipelines[code]) return aoApplyPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, aoApplyPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;  // the picture times the shadow
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
                                                   reinterpret_cast<void**>(&aoApplyPipelines[code])))) {
        LOG_ERROR("shadows: the pipeline onto %u samples failed", 1u << code);
        aoApplyPipelines[code] = nullptr;
    }
    return aoApplyPipelines[code];
}

bool Renderer::Impl::applyAo() {
    aoDone = true;  // tried once a frame, whatever comes of it
    if (!ensureScene("shadows")) return true;
    const int w = sceneWidth, h = sceneHeight;
    ID3D12PipelineState* apply = aoApplyPipeline(upscale ? 1 : samples);
    if (!ensureAo(w, h) || !apply) return true;
    if (!copySceneDepth()) return false;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(aoWidth);
    c.uiPixels[3] = float(aoHeight);
    // A cell of the map is ten units: the shadows reach 1.5 to 9 of them from a corner.
    c.footprint = 1.5f + 7.5f * float(std::clamp(effects.aoSize, 0, 100)) / 100.0f;
    c.sharpen = float(std::clamp(effects.aoStrength, 0, 100)) / 100.0f;
    if (!effectPass(aoPipeline, aoTarget[0], aoState[0], rtvHandle(kRtvAo), aoWidth, aoHeight, c, sceneDepthSrv,
                    true))  // t0 unread: the depth, in its shader state already
        return false;
    for (int i = 0; i < 2; ++i) {  // across, then down
        transition(aoTarget[i], aoState[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        c.sourceRect[0] = i == 0 ? 1.0f : 0.0f;
        c.sourceRect[1] = i == 0 ? 0.0f : 1.0f;
        if (!effectPass(aoBlurPipeline, aoTarget[1 - i], aoState[1 - i], rtvHandle(kRtvAo + UINT(1 - i)), aoWidth,
                        aoHeight, c, aoSrv[i], true))
            return false;
    }
    transition(aoTarget[0], aoState[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return upscale ? effectPass(apply, world, worldState, rtvHandle(kRtvWorld), w, h, c, aoSrv[0], false)
                   : effectPass(apply, target, targetState, rtvHandle(kRtvTarget), w, h, c, aoSrv[0], false);
}

// Into the target (its samples) or the world under DLSS (one sample), as it is: the colours are made in the shader.
bool Renderer::Impl::applyGrade() {
    gradeDone = true;  // tried once a frame, whatever comes of it
    const UINT n = upscale ? 1 : samples;
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (!gradePs || !ensureScene("colour")) return true;
    if (!gradePipelines[code]) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
            fullScreenPipeline(root, presentVs, gradePs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
        p.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&gradePipelines[code])))) {
            gradePipelines[code] = nullptr;
            LOG_ERROR("colour: the pipeline onto %u samples failed", 1u << code);
            return true;
        }
    }
    if (!copySceneColor()) return false;
    Constants c{};
    c.sharpen = float(std::clamp(effects.gradeSaturation, 0, 200)) / 100.0f;
    c.footprint = float(std::clamp(effects.gradeContrast, 50, 150)) / 100.0f;
    c.twin[0] = float(std::clamp(effects.gradeBrightness, 50, 150)) / 100.0f;
    c.twin[1] = float(std::clamp(effects.gradeWarmth, 0, 100) - 50) / 50.0f;
    c.twin[2] = float(std::clamp(effects.gradeVignette, 0, 100)) / 100.0f;
    c.uiPixels[0] = float(sceneWidth);
    c.uiPixels[1] = float(sceneHeight);
    return upscale ? effectPass(gradePipelines[code], world, worldState, rtvHandle(kRtvWorld), sceneWidth,
                                sceneHeight, c, sceneColorSrv, true)
                   : effectPass(gradePipelines[code], target, targetState, rtvHandle(kRtvTarget), sceneWidth,
                                sceneHeight, c, sceneColorSrv, true);
}

bool Renderer::Impl::finishWorld() {
    if (aoDue() && !applyAo()) return false;
    return !gradeDue() || applyGrade();
}

}  // namespace uagfx::gpu
