#include <d3d11.h>

#include <algorithm>

#include "core/d3d7.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr DXGI_FORMAT kGlowFormat11 = DXGI_FORMAT_R11G11B10_FLOAT;  // light past 1 where effects pile up
constexpr uint8_t kAll = D3D11_COLOR_WRITE_ENABLE_ALL;
constexpr uint8_t kRgb = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

// 1.0: the glow target at the world's size and its chain of halves, cleared; from now on the world's draws write into
// it too (bindTarget). False: it cannot be made at this size - no glow then.
bool Renderer11::startGlow() {
    const int w = target_.width, h = target_.height;
    if (w <= 0 || h <= 0) return false;
    if (glow_.width != w || glow_.height != h) {
        if (glowFailed_ && glow_.width == w && glow_.height == h) return false;
        dropTarget(glow_);
        for (Target& t : glowChain_) dropTarget(t);
        glowLevels_ = 0;
        for (int side = std::min(w, h) / 2; side >= 8 && glowLevels_ < kGlowLevels11; side /= 2) ++glowLevels_;
        glowLevels_ = std::max(glowLevels_, 1);
        bool made = makeTarget(glow_, w, h, kGlowFormat11);
        int lw = w, lh = h;
        for (int i = 0; made && i < glowLevels_; ++i) {
            lw = std::max(1, (lw + 1) / 2);
            lh = std::max(1, (lh + 1) / 2);
            made = makeTarget(glowChain_[i], lw, lh, kGlowFormat11);
        }
        glowFailed_ = !made;
        if (!made) {
            LOG_ERROR("glow: cannot be drawn at %dx%d with Direct3D 11 - it stays off at this size", w, h);
            dropTarget(glow_);
            for (Target& t : glowChain_) dropTarget(t);
            glow_.width = w;  // remembered as failed
            glow_.height = h;
            return false;
        }
        LOG_INFO("glow: %dx%d, %d levels (Direct3D 11)", w, h, glowLevels_);
    }
    flushBatch();  // the draws before it are not in the glow
    const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx_->ClearRenderTargetView(glow_.rtv, none);
    glowActive_ = true;
    targetBound_ = false;  // bound again with the glow as the second target
    return true;
}

void Renderer11::glowPass(ID3D11PixelShader* ps, ID3D11ShaderResourceView* src, int srcW, int srcH,
                          ID3D11RenderTargetView* dst, int dstW, int dstH, ID3D11BlendState* blend, float factor,
                          float strength, bool first) {
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 2, none);
    ctx_->OMSetRenderTargets(1, &dst, nullptr);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(dstW), float(dstH), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    const D3D11_RECT clip{0, 0, dstW, dstH};
    ctx_->RSSetScissorRects(1, &clip);
    Constants c{};
    c.targetSize[0] = float(dstW);
    c.targetSize[1] = float(dstH);
    c.sourceRect[2] = c.sourceRect[3] = 1.0f;
    c.sourceClamp[0] = 1.0f / float(srcW);
    c.sourceClamp[1] = 1.0f / float(srcH);
    c.footprint = strength;
    c.sharpen = first ? 1.0f : 0.0f;
    setConstants(c);
    const float factors[4] = {factor, factor, factor, factor};
    ctx_->OMSetBlendState(blend, factors, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    StageSetup linear;
    linear.addressU = linear.addressV = d3d7::TADDRESS_CLAMP;
    linear.magFilter = d3d7::TFG_LINEAR;
    linear.minFilter = d3d7::TFN_LINEAR;
    linear.mipFilter = d3d7::TFP_NONE;
    ID3D11SamplerState* sampler = samplerState(linear, 0);
    ctx_->PSSetSamplers(0, 1, &sampler);
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->PSSetShader(ps, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetShaderResources(0, 1, &src);
    ctx_->Draw(3, 0);
    ctx_->PSSetShaderResources(0, 2, none);
}

// At the interface (or the frame's end): the glow shrunk level by level, blurred back up, its wide level's light
// onto what is around in its colour, then all of it added onto the picture (as Direct3D 12's addGlow).
void Renderer11::addGlow() {
    flushBatch();
    EffectSpan span(*this, kTimeGlow11);  // 2.5.9: the glow's own passes, not the draws before them
    const bool drawn = glowDrawn_;
    glowActive_ = glowDrawn_ = false;
    targetBound_ = false;  // the game's draws bind the target alone again
    if (!drawn || !glow_.texture || !glowLevels_ || !glowWanted()) return;
    gradeNow();  // 3.0.5-beta.4: the colours put off meet the world before its glow is added, as before
    ID3D11ShaderResourceView* src = glow_.srv;
    int sw = glow_.width, sh = glow_.height;
    ID3D11BlendState* replace = blendState(false, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, kAll);
    for (int i = 0; i < glowLevels_; ++i) {
        glowPass(glowDownPs_, src, sw, sh, glowChain_[i].rtv, glowChain_[i].width, glowChain_[i].height, replace,
                 1.0f, 0.0f, i == 0);
        src = glowChain_[i].srv;
        sw = glowChain_[i].width;
        sh = glowChain_[i].height;
    }
    const float spread = 0.5f + 0.45f * float(std::clamp(effects_.glowSize, 0, 100)) / 100.0f;
    ID3D11BlendState* mix = blendState(true, D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR, kAll);
    for (int i = glowLevels_ - 2; i >= 0; --i)
        glowPass(glowUpPs_, glowChain_[i + 1].srv, glowChain_[i + 1].width, glowChain_[i + 1].height,
                 glowChain_[i].rtv, glowChain_[i].width, glowChain_[i].height, mix, spread, 0.0f, false);
    const float light = float(std::clamp(effects_.glowLight, 0, 100)) / 100.0f;
    const int wide = std::min(2, glowLevels_ - 1);
    if (light > 0.0f)  // the light on what is around: the picture times the wide glow, added
        glowPass(glowAddPs_, glowChain_[wide].srv, glowChain_[wide].width, glowChain_[wide].height, target_.rtv,
                 target_.width, target_.height, blendState(true, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_ONE, kRgb),
                 1.0f, light, false);
    const float strength = float(std::clamp(effects_.glowStrength, 0, 200)) / 100.0f;
    glowPass(glowAddPs_, glowChain_[0].srv, glowChain_[0].width, glowChain_[0].height, target_.rtv, target_.width,
             target_.height, blendState(true, D3D11_BLEND_ONE, D3D11_BLEND_ONE, kRgb), 1.0f, strength, false);
}

}  // namespace uagfx::gpu
