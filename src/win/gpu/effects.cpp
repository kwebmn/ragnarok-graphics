#include <windows.h>

#include <d3d12.h>

#include <algorithm>

#include "core/d3d7.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr DXGI_FORMAT kGlowFormat = DXGI_FORMAT_R11G11B10_FLOAT;  // light past 1 where effects pile up

// The glow's smaller copies: halved down to about eight pixels on the short side, so that it spreads as far on any
// screen size (eight levels at 4K).
int glowLevelsFor(int w, int h) {
    int n = 0;
    for (int side = std::min(w, h) / 2; side >= 8 && n < kGlowLevels; side /= 2) ++n;
    return std::max(n, 1);
}

// The glow's format drawn into and blended, with n samples, and resolved (Direct3D 11 hardware has it all; asked
// anyway).
bool glowFormatWorks(ID3D12Device* device, UINT n) {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT f{};
    f.Format = kGlowFormat;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &f, sizeof f))) return false;
    UINT need = UINT(D3D12_FORMAT_SUPPORT1_RENDER_TARGET) | UINT(D3D12_FORMAT_SUPPORT1_BLENDABLE);
    if (n > 1)
        need |= UINT(D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET) | UINT(D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE);
    if ((UINT(f.Support1) & need) != need) return false;
    if (n <= 1) return true;
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS q{};
    q.Format = kGlowFormat;
    q.SampleCount = n;
    return SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &q, sizeof q)) &&
           q.NumQualityLevels > 0;
}

}  // namespace

bool Renderer::Impl::createGlowPipelines() {
    if (glowDownPipeline && glowUpPipeline) return true;
    if (!glowDownPs || !glowUpPs || !glowAddPs || !drawGlowPs) return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = kGlowFormat;
    p.SampleDesc.Count = 1;
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (!glowDownPipeline) {
        p.PS = {glowDownPs->GetBufferPointer(), glowDownPs->GetBufferSize()};
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&glowDownPipeline)))) {
            glowDownPipeline = nullptr;
            LOG_ERROR("the glow's pipelines failed: it stays off");
            return false;
        }
    }
    p.PS = {glowUpPs->GetBufferPointer(), glowUpPs->GetBufferSize()};
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;  // the smaller level blurred, mixed with what this one holds by the blend factor
    b.SrcBlend = D3D12_BLEND_BLEND_FACTOR;
    b.DestBlend = D3D12_BLEND_INV_BLEND_FACTOR;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ONE;
    b.DestBlendAlpha = D3D12_BLEND_ZERO;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&glowUpPipeline)))) {
        glowUpPipeline = nullptr;
        LOG_ERROR("the glow's pipelines failed: it stays off");
        return false;
    }
    return true;
}

// Onto the target (B8G8R8A8, its samples) or the upscaled world (R8G8B8A8, one sample), added.
ID3D12PipelineState* Renderer::Impl::glowAddPipeline(UINT n, bool upscaledFormat) {
    const UINT code = upscaledFormat ? 4 : n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (glowAddPipelines[code]) return glowAddPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
    p.PS = {glowAddPs->GetBufferPointer(), glowAddPs->GetBufferSize()};
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_ONE;
    b.DestBlend = D3D12_BLEND_ONE;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = upscaledFormat ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    p.SampleDesc.Count = upscaledFormat ? 1 : 1u << code;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&glowAddPipelines[code])))) {
        LOG_ERROR("the glow's pipeline onto %s failed", upscaledFormat ? "the upscaled world" : "the target");
        glowAddPipelines[code] = nullptr;
    }
    return glowAddPipelines[code];
}

void Renderer::Impl::releaseGlow() {
    Slot& s = slots[slot];  // what the open list recorded may still read them
    if (glow) s.garbage.push_back(glow);
    if (glowResolved) s.garbage.push_back(glowResolved);
    for (ID3D12Resource*& r : glowChain) {
        if (r) s.garbage.push_back(r);
        r = nullptr;
    }
    glow = glowResolved = nullptr;
    glowLevels = 0;
    glowWidth = glowHeight = 0;
    glowSamples = 0;
    glowActive = glowDrawn = false;
}

void Renderer::Impl::dropGlow() { glowActive = glowDrawn = false; }

bool Renderer::Impl::startGlow() {
    // The world's own size and samples: the target's, or under DLSS (part 3) the world drawn smaller.
    const int w = upscale ? renderWidth : width;
    const int h = upscale ? renderHeight : height;
    const UINT n = upscale ? 1 : samples;
    if (w <= 0 || h <= 0 || !open) return false;
    if (!glow || w != glowWidth || h != glowHeight || n != glowSamples) {
        if (glowFailed && w == glowWidth && h == glowHeight && n == glowSamples) return false;
        releaseGlow();
        glowWidth = w;
        glowHeight = h;
        glowSamples = n;
        glowFailed = true;  // until everything below is made
        if (!createGlowPipelines() || !glowAddPipeline(n, false) || !glowFormatWorks(device, n)) {
            LOG_ERROR("the glow cannot be drawn at %dx%d with %u samples: it stays off at this size", w, h, n);
            return false;
        }
        D3D12_CLEAR_VALUE none{};
        none.Format = kGlowFormat;
        glow = createTexture2D(device, w, h, 1, kGlowFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                               D3D12_RESOURCE_STATE_RENDER_TARGET, &none, n);
        if (n > 1)
            glowResolved = createTexture2D(device, w, h, 1, kGlowFormat, D3D12_RESOURCE_FLAG_NONE,
                                           D3D12_RESOURCE_STATE_RESOLVE_DEST, nullptr);
        glowLevels = glowLevelsFor(w, h);
        int lw = w, lh = h;
        bool made = glow && (n == 1 || glowResolved);
        for (int i = 0; made && i < glowLevels; ++i) {
            lw = std::max(1, (lw + 1) / 2);
            lh = std::max(1, (lh + 1) / 2);
            glowChain[i] = createTexture2D(device, lw, lh, 1, kGlowFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET, &none);
            glowChainState[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
            glowChainWidth[i] = lw;
            glowChainHeight[i] = lh;
            made = glowChain[i] != nullptr;
        }
        if (!made) {
            LOG_ERROR("no video memory for the glow at %dx%d with %u samples: it stays off at this size", w, h, n);
            releaseGlow();
            glowWidth = w;  // remembered as failed
            glowHeight = h;
            glowSamples = n;
            return false;
        }
        glowState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        glowResolvedState = D3D12_RESOURCE_STATE_RESOLVE_DEST;
        device->CreateRenderTargetView(glow, nullptr, rtvHandle(kRtvGlow));
        if (n == 1) device->CreateShaderResourceView(glow, nullptr, cpuSrvHandle(glowSrv));
        if (glowResolved) device->CreateShaderResourceView(glowResolved, nullptr, cpuSrvHandle(glowResolvedSrv));
        for (int i = 0; i < glowLevels; ++i) {
            device->CreateRenderTargetView(glowChain[i], nullptr, rtvHandle(kRtvGlowChain + UINT(i)));
            device->CreateShaderResourceView(glowChain[i], nullptr, cpuSrvHandle(glowChainSrv[i]));
        }
        glowFailed = false;
        LOG_INFO("glow: %dx%d with %u sample%s, %d levels", w, h, n, n == 1 ? "" : "s", glowLevels);
    }
    transition(glow, glowState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    list->ClearRenderTargetView(rtvHandle(kRtvGlow), none, 0, nullptr);
    glowActive = true;
    return true;
}

bool Renderer::Impl::glowPass(ID3D12PipelineState* pso, ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState,
                              UINT srcSrv, int srcW, int srcH, ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState,
                              D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, int dstW, int dstH, float strength, bool first,
                              float blendFactor) {
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp, 0)) return false;  // a new list starts before this pass records anything
    placeSamples(false);
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp, 0);
    const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(srcSrv, srcSrv);
    transition(src, srcState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(dst, dstState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(dstW), float(dstH), 0.0f, 1.0f};
    const D3D12_RECT clip{0, 0, dstW, dstH};
    list->OMSetRenderTargets(1, &dstRtv, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &clip);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(pso);
    const float factor[4] = {blendFactor, blendFactor, blendFactor, blendFactor};
    list->OMSetBlendFactor(factor);
    Constants c{};
    c.targetSize[0] = float(dstW);
    c.targetSize[1] = float(dstH);
    c.sourceRect[2] = c.sourceRect[3] = 1.0f;
    c.sourceClamp[0] = 1.0f / float(srcW);
    c.sourceClamp[1] = 1.0f / float(srcH);
    c.footprint = strength;
    c.sharpen = first ? 1.0f : 0.0f;
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    return true;
}

bool Renderer::Impl::addGlow(ID3D12Resource* dst, D3D12_RESOURCE_STATES& dstState, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv,
                             int dstW, int dstH, UINT dstSamples, bool upscaledFormat) {
    const bool drawn = glowDrawn;
    glowActive = glowDrawn = false;  // the next frame's world starts it again
    if (!drawn || !glow || !glowLevels || !glowWanted()) return true;
    ID3D12PipelineState* add = glowAddPipeline(dstSamples, upscaledFormat);
    if (!add || !glowDownPipeline || !glowUpPipeline) return true;
    // One sample per pixel, shrunk level by level.
    ID3D12Resource* src = glow;
    D3D12_RESOURCE_STATES* srcState = &glowState;
    UINT srcSrv = glowSrv;
    if (glowSamples > 1) {
        transition(glow, glowState, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        transition(glowResolved, glowResolvedState, D3D12_RESOURCE_STATE_RESOLVE_DEST);
        list->ResolveSubresource(glowResolved, 0, glow, 0, kGlowFormat);
        src = glowResolved;
        srcState = &glowResolvedState;
        srcSrv = glowResolvedSrv;
    }
    int sw = glowWidth, sh = glowHeight;
    for (int i = 0; i < glowLevels; ++i) {
        if (!glowPass(glowDownPipeline, src, *srcState, srcSrv, sw, sh, glowChain[i], glowChainState[i],
                      rtvHandle(kRtvGlowChain + UINT(i)), glowChainWidth[i], glowChainHeight[i], 0.0f, i == 0))
            return false;
        src = glowChain[i];
        srcState = &glowChainState[i];
        srcSrv = glowChainSrv[i];
        sw = glowChainWidth[i];
        sh = glowChainHeight[i];
    }
    // Back up: each level takes this much of the blurred smaller one - more spreads the glow further.
    const float spread = 0.5f + 0.45f * float(std::clamp(effects.glowSize, 0, 100)) / 100.0f;
    for (int i = glowLevels - 2; i >= 0; --i)
        if (!glowPass(glowUpPipeline, glowChain[i + 1], glowChainState[i + 1], glowChainSrv[i + 1],
                      glowChainWidth[i + 1], glowChainHeight[i + 1], glowChain[i], glowChainState[i],
                      rtvHandle(kRtvGlowChain + UINT(i)), glowChainWidth[i], glowChainHeight[i], 0.0f, false, spread))
            return false;
    const float strength = float(std::clamp(effects.glowStrength, 0, 200)) / 100.0f;
    return glowPass(add, glowChain[0], glowChainState[0], glowChainSrv[0], glowChainWidth[0], glowChainHeight[0], dst,
                    dstState, dstRtv, dstW, dstH, strength, false);
}

bool Renderer::Impl::addGlowToTarget() {
    return addGlow(target, targetState, rtvHandle(kRtvTarget), width, height, samples, false);
}

}  // namespace uagfx::gpu
