#include <d3d11.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/daylight.hpp"
#include "core/game_camera.hpp"
#include "core/outdoors.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

constexpr int kMaterialSide11 = 256;  // Direct3D 11 is for the weaker cards: a quarter of Direct3D 12's texels

}  // namespace

// 1.14, the materials (effects.materials) as Direct3D 12's materials.cpp: for a texture of the world its relief,
// height and gloss (material_ps), made on the card from the picture drawn (the game's or the AI's), at most 256 a
// side, a level for each, once; then read by its draws (t2, shade() bit 28). Two a frame at most.
void Renderer11::dropMaterial(Texture& t) {
    drop(t.matSrv);
    drop(t.mat);
    t.matFor = nullptr;
}

bool Renderer11::ensureMaterial(Texture& t, ID3D11ShaderResourceView* source) {
    if (!source) return false;
    ID3D11Resource* res = nullptr;
    source->GetResource(&res);
    if (!res) return false;
    res->Release();  // the picture's texture stays alive while its view does: only compared and read here
    if (t.mat && t.matFor == res) return true;
    if (materialsMade_ >= 2 || !materialPs_) return false;  // the rest in the next frames
    ++materialsMade_;
    dropMaterial(t);
    ID3D11Texture2D* picture = nullptr;
    if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&picture))) || !picture)
        return false;
    D3D11_TEXTURE2D_DESC sd{};
    picture->GetDesc(&sd);
    picture->Release();
    const int sw = int(sd.Width), sh = int(sd.Height), sourceLevels = std::max<int>(1, int(sd.MipLevels));
    // The material's size: the picture's, halved down to kMaterialSide11; `shift` source levels above its first.
    int shift = 0;
    while ((sw >> shift) > kMaterialSide11 || (sh >> shift) > kMaterialSide11) ++shift;
    const int mw = std::max(1, sw >> shift), mh = std::max(1, sh >> shift);
    int levels = 1;
    while ((mw >> levels) >= 4 && (mh >> levels) >= 4) ++levels;
    D3D11_TEXTURE2D_DESC md{};
    md.Width = UINT(mw);
    md.Height = UINT(mh);
    md.MipLevels = UINT(levels);
    md.ArraySize = 1;
    md.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    md.SampleDesc.Count = 1;
    md.Usage = D3D11_USAGE_DEFAULT;
    md.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&md, nullptr, &t.mat)) ||
        FAILED(device_->CreateShaderResourceView(t.mat, nullptr, &t.matSrv))) {
        dropMaterial(t);
        return false;
    }
    flushBatch();  // the gathered draws go before the pass binds its own
    targetBound_ = false;
    ID3D11ShaderResourceView* none[3] = {nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 3, none);
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO, D3D11_COLOR_WRITE_ENABLE_ALL), nullptr,
                          0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->PSSetShader(materialPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    bool made = true;
    for (int level = 0; level < levels && made; ++level) {
        D3D11_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rd.Texture2D.MipSlice = UINT(level);
        ID3D11RenderTargetView* rtv = nullptr;
        if (FAILED(device_->CreateRenderTargetView(t.mat, &rd, &rtv))) {
            made = false;
            break;
        }
        const int lw = std::max(1, mw >> level), lh = std::max(1, mh >> level);
        ctx_->OMSetRenderTargets(1, &rtv, nullptr);
        const D3D11_VIEWPORT vp{0.0f, 0.0f, float(lw), float(lh), 0.0f, 1.0f};
        ctx_->RSSetViewports(1, &vp);
        const D3D11_RECT clip{0, 0, LONG(lw), LONG(lh)};
        ctx_->RSSetScissorRects(1, &clip);
        Constants c{};
        c.uiPixels[0] = float(lw);
        c.uiPixels[1] = float(lh);
        c.uiPixels[2] = float(level + shift);  // the source's level it is made from
        c.uiPixels[3] = float(sourceLevels);
        c.sourceRect[0] = float(sw);
        c.sourceRect[1] = float(sh);
        setConstants(c);
        ctx_->PSSetShaderResources(0, 1, &source);
        ctx_->Draw(3, 0);
        ctx_->PSSetShaderResources(0, 1, none);
        rtv->Release();
    }
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    if (!made) {
        dropMaterial(t);  // tried again later
        return false;
    }
    t.matFor = res;  // whole: drawn with from now on
    if (!materialsLogged_)
        LOG_INFO("materials: made on the card from the pictures, %dx%d and smaller (%d levels, Direct3D 11)", mw, mh,
                 levels);
    materialsLogged_ = true;
    return true;
}

// The constants of a draw lit by its material (as Direct3D 12's materialConstants): bits 28 and 30 (no relief by the
// view's depth - bit 29 is the remaster's), the game's projection and camera, the way to the sun in the view (indoors
// from above), the strength.
void Renderer11::materialConstants(bool ground, Constants& c) const {
    c.stage1[1] |= 1u << 28;
    c.stage1[1] &= ~(1u << 26);  // the relief of 0.71 is the material's now
    if (ground) c.stage1[1] |= 1u << 30;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
    std::memcpy(c.projection, proj, sizeof proj);
    static const float kSame[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    const float* m = cameraValid_ ? viewToWorld_ : kSame;
    std::memcpy(c.reproject, m, sizeof c.reproject);
    const daylight::Light day = daylight::at(effects_.dayCycle ? effects_.dayNow : -1);
    const bool open = outdoors(map_);
    const float height = (open ? std::clamp(50.0f / day.length * 0.55f, 15.0f, 70.0f) : 75.0f) * 0.0174533f;
    // along the ground: where the shadows fall on the screen, turned into the world by the view's axes
    float g[3];
    for (int k = 0; k < 3; ++k) g[k] = day.shadow[0] * m[k] + day.shadow[1] * m[3 + k];
    g[1] = 0.0f;
    const float gl = std::max(std::sqrt(g[0] * g[0] + g[2] * g[2]), 1e-4f);
    const float toSun[3] = {-g[0] / gl * std::cos(height), -std::sin(height), -g[2] / gl * std::cos(height)};
    for (int k = 0; k < 3; ++k) c.twin[k] = toSun[0] * m[3 * k] + toSun[1] * m[3 * k + 1] + toSun[2] * m[3 * k + 2];
    c.twin[3] = float(std::clamp(effects_.materialsStrength, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    c.uiPixels[0] = open ? 1.0f - night : 0.0f;  // the sun's light here
    c.uiPixels[1] = 1.0f;
}

}  // namespace uagfx::gpu
