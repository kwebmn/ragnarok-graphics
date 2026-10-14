#include <windows.h>

#include <d3d12.h>

#include "core/d3d7.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// The target's depth is R32 typeless (createTarget): D32 to draw with, R32 for the water to read.
void Renderer::Impl::makeDepthViews(UINT n) {
    D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
    dv.Format = DXGI_FORMAT_D32_FLOAT;
    dv.ViewDimension = n > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depth, &dv, dsvHandle());
    D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = DXGI_FORMAT_R32_FLOAT;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (n > 1) {
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    } else {
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Texture2D.MipLevels = 1;
    }
    device->CreateShaderResourceView(depth, &sv, cpuSrvHandle(depthTargetSrv));
    depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

void Renderer::Impl::releaseScene() {
    Slot& s = slots[slot];  // what the open list recorded may still read them
    if (sceneColor) s.garbage.push_back(sceneColor);
    if (sceneDepth) s.garbage.push_back(sceneDepth);
    sceneColor = sceneDepth = nullptr;
    sceneWidth = sceneHeight = 0;
    waterReady = false;
}

float Renderer::Impl::waterTime() {
    LARGE_INTEGER now, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    if (!waterClock.QuadPart) waterClock = now;
    const double s = double(now.QuadPart - waterClock.QuadPart) / double(freq.QuadPart);
    return float(s - 3600.0 * double(int64_t(s / 3600.0)));  // an hour round: the floats keep their precision
}

bool Renderer::Impl::copyScene() {
    if (waterCopied) return waterReady;
    waterCopied = true;
    waterReady = false;
    // The world's own size and samples: the target's, or under DLSS (part 3) the world drawn smaller.
    const int w = upscale ? renderWidth : width;
    const int h = upscale ? renderHeight : height;
    if (!waterVs || !waterPs || !waterGlowPs || !depthCopyPs || !depthCopyMsPs || w <= 0 || h <= 0 || !open)
        return false;  // its shaders did not compile (logged at the start)
    if (!viewToWorldValid) {
        static bool logged = false;
        if (!logged) LOG_INFO("water: the game's camera is not known, so the water stays the game's");
        logged = true;
        return false;
    }
    if (!sceneColor || w != sceneWidth || h != sceneHeight) {
        if (sceneFailed && w == sceneWidth && h == sceneHeight) return false;
        releaseScene();
        sceneWidth = w;
        sceneHeight = h;
        sceneFailed = true;  // until everything below is made
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
        p.pRootSignature = root;
        p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
        p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        p.SampleMask = UINT_MAX;
        p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        p.RasterizerState.DepthClipEnable = TRUE;
        p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        p.NumRenderTargets = 1;
        p.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
        p.SampleDesc.Count = 1;
        for (ID3D12PipelineState** pso : {&depthCopyPipeline, &depthCopyMsPipeline}) {
            if (*pso) continue;
            ID3DBlob* ps = pso == &depthCopyPipeline ? depthCopyPs : depthCopyMsPs;
            p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
            if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                           reinterpret_cast<void**>(pso)))) {
                *pso = nullptr;
                LOG_ERROR("water: the depth copy's pipeline failed: the water stays the game's");
                return false;
            }
        }
        D3D12_CLEAR_VALUE farthest{};
        farthest.Format = DXGI_FORMAT_R32_FLOAT;
        farthest.Color[0] = 1.0f;
        sceneColor = createTexture2D(device, w, h, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_NONE,
                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        sceneDepth = createTexture2D(device, w, h, 1, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                     D3D12_RESOURCE_STATE_RENDER_TARGET, &farthest);
        if (!sceneColor || !sceneDepth) {
            LOG_ERROR("water: no video memory for the picture under it at %dx%d: the water stays the game's", w, h);
            releaseScene();
            sceneWidth = w;  // remembered as failed
            sceneHeight = h;
            return false;
        }
        sceneColorState = D3D12_RESOURCE_STATE_COPY_DEST;
        sceneDepthState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        device->CreateShaderResourceView(sceneColor, nullptr, cpuSrvHandle(sceneColorSrv));
        device->CreateShaderResourceView(sceneDepth, nullptr, cpuSrvHandle(sceneDepthSrv));
        device->CreateRenderTargetView(sceneDepth, nullptr, rtvHandle(kRtvSceneDepth));
        sceneFailed = false;
        LOG_INFO("water: drawn anew, from the picture under it at %dx%d", w, h);
    }
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp, 0, 3)) return false;  // a new list starts before the copies
    // The picture so far, one sample per pixel.
    const bool several = !upscale && samples > 1;
    if (several) {
        transition(target, targetState, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        transition(sceneColor, sceneColorState, D3D12_RESOURCE_STATE_RESOLVE_DEST);
        list->ResolveSubresource(sceneColor, 0, target, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
    } else {
        ID3D12Resource* src = upscale ? world : target;
        transition(src, upscale ? worldState : targetState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(sceneColor, sceneColorState, D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyResource(sceneColor, src);
    }
    // Its depth, drawn into an R32 target (a depth buffer cannot be resolved): with several samples the first.
    ID3D12Resource* d = upscale ? worldDepth : depth;
    transition(d, upscale ? worldDepthState : depthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    placeSamples(false);
    const UINT src = upscale ? depthSrv : depthTargetSrv;
    const D3D12_GPU_DESCRIPTOR_HANDLE table =
        several ? sceneTable(sceneColorSrv, sceneColorSrv, src) : sceneTable(sceneColorSrv, src, sceneColorSrv);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(kRtvSceneDepth);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    const D3D12_RECT clip{0, 0, w, h};
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &clip);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(several ? depthCopyMsPipeline : depthCopyPipeline);
    Constants c{};
    c.targetSize[0] = float(w);
    c.targetSize[1] = float(h);
    c.sourceRect[2] = c.sourceRect[3] = 1.0f;
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootScene, table);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    transition(sceneColor, sceneColorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(sceneDepth, sceneDepthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    waterReady = true;
    return true;
}

}  // namespace uagfx::gpu
