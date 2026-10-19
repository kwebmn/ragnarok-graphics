#include <windows.h>

#include <d3d11.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

// 2.5.9: the world's reflections in the water with Direct3D 11, as Direct3D 12's mirror.cpp - the frame's opaque world
// noted (sun_map.cpp noteCaster), the next frame's mirror drawn from it before its first water.
namespace uagfx::gpu {
namespace {

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

constexpr size_t kMostCasterVerts = 600000;

}  // namespace

// An opaque draw of the world (not the interface, not blended: the map, its models, the crowns): its triangles in the
// world with their texture coordinates and the game's colour (six floats a corner), for the next frame's mirror.
void Renderer11::noteCaster(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    if (casterVerts_.size() / 6 >= kMostCasterVerts ||
        (d.primitive != d3d7::PT_TRIANGLELIST && d.primitive != d3d7::PT_TRIANGLESTRIP))
        return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) return;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float* m = viewToWorld_;
    const std::vector<uint16_t>& ix = rec.indices();
    const uint32_t n = d.indexCount ? d.indexCount : d.vertexCount;
    const bool strip = d.primitive == d3d7::PT_TRIANGLESTRIP;
    const uint32_t triangles = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
    Caster11 c;
    c.first = uint32_t(casterVerts_.size() / 6);
    c.texture = d.texture[0];
    c.alphaTest = s.alphaTest || s.colorKey || d.keyed[0];
    c.ref = s.alphaTest ? float(s.alphaRef) / 255.0f : 0.5f;
    const auto put = [&](uint32_t k) {
        uint32_t at = k;
        if (d.indexCount) {
            if (d.firstIndex + k >= ix.size()) return false;
            at = ix[d.firstIndex + k];
        }
        if (at >= d.vertexCount) return false;
        const uint8_t* corner = vb.data() + d.vertexOffset + size_t(at) * stride;
        float v[6];
        std::memcpy(v, corner, sizeof(float) * 4);
        std::memcpy(v + 4, corner + 24, sizeof(float) * 2);
        if (v[3] <= 0.0f) return false;
        const float vz = 1.0f / v[3], vx = (v[0] - p.cx) * vz / p.f0, vy = (v[1] - p.cy) * vz / p.f1;
        uint32_t colour;  // the game's A8R8G8B8 - as R8G8B8A8 where the card takes no B8G8R8A8 vertices
        std::memcpy(&colour, corner + 16, sizeof colour);
        if (!bgraVertices_) colour = (colour & 0xFF00FF00u) | (colour & 0xFFu) << 16 | (colour >> 16 & 0xFFu);
        float bits;
        std::memcpy(&bits, &colour, sizeof bits);
        const float out[6] = {vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                              vx * m[2] + vy * m[5] + vz * m[8] + m[11], v[4], v[5], bits};
        casterVerts_.insert(casterVerts_.end(), out, out + 6);
        return true;
    };
    for (uint32_t t = 0; t < triangles; ++t) {
        const uint32_t base = strip ? t : t * 3;
        const size_t before = casterVerts_.size();
        if (!put(base) || !put(base + 1) || !put(base + 2)) casterVerts_.resize(before);
    }
    c.count = uint32_t(casterVerts_.size() / 6) - c.first;
    if (c.count) casters_.push_back(c);
}

// Before the frame's first water: the last frame's casters turned over the water's level and drawn by the game's
// camera at half the picture's size (mirror_vs, mirror_ps), both faces, with a depth of their own; water_ps takes it
// (t5) where the picture itself has nothing to reflect - what is above the screen's top, the far sides of things.
void Renderer11::applyMirror() {
    mirrorDone_ = true;
    if (!cameraValid_ || !target_.texture || mirrorVerts_.empty() || mirrorCasters_.empty()) return;
    const int w = std::max(1, target_.width / 2), h = std::max(1, target_.height / 2);
    if (mirror_.width != w || mirror_.height != h || !mirrorDsv_) {
        dropTarget(mirror_);
        drop(mirrorDsv_);
        drop(mirrorDepth_);
        if (!makeTarget(mirror_, w, h, DXGI_FORMAT_R8G8B8A8_UNORM) || !makeDepth(w, h, mirrorDepth_, mirrorDsv_)) {
            LOG_ERROR("world reflections: no video memory for the mirror (Direct3D 11): none");
            dropTarget(mirror_);
            mirrorFailed_ = true;  // not tried again
            return;
        }
    }
    // The world into the view: the inverse of viewToWorld (its rows the view's axes in the world, then its place).
    const float* m = viewToWorld_;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], hh = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * hh) + c3 * (d * i - e * hh);
    if (std::fabs(det) < 1e-8f) return;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * hh - d * k) / det, (a * k - c3 * hh) / det, (c3 * d - a * g) / det,
                          (d * i - e * hh) / det, (b * hh - a * i) / det, (a * e - b * d) / det};
    Constants c{};
    for (int r = 0; r < 3; ++r) {
        c.reproject[r * 4] = inv[r];
        c.reproject[r * 4 + 1] = inv[3 + r];
        c.reproject[r * 4 + 2] = inv[6 + r];
        c.reproject[r * 4 + 3] = -(inv[r] * m[9] + inv[3 + r] * m[10] + inv[6 + r] * m[11]);
    }
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.uiPixels[0] = float(gameWidth_);
    c.uiPixels[1] = float(gameHeight_);
    c.twin[0] = waterLevel_;
    flushBatch();  // the game's draws gathered so far go into the picture first
    if (!fillBuffer(mirrorVb_, mirrorVbBytes_, D3D11_BIND_VERTEX_BUFFER, mirrorVerts_.data(),
                    mirrorVerts_.size() * sizeof(float), nullptr))
        return;
    targetBound_ = false;  // the next draw binds the target, its state and the game's shaders again
    ID3D11ShaderResourceView* none = nullptr;
    ctx_->PSSetShaderResources(5, 1, &none);  // the last frame's mirror, read by its water, is drawn into now
    ctx_->OMSetRenderTargets(1, &mirror_.rtv, mirrorDsv_);
    const float clearColour[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx_->ClearRenderTargetView(mirror_.rtv, clearColour);
    ctx_->ClearDepthStencilView(mirrorDsv_, D3D11_CLEAR_DEPTH, 1.0f, 0);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    const D3D11_RECT all{0, 0, w, h};
    ctx_->RSSetScissorRects(1, &all);
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO, D3D11_COLOR_WRITE_ENABLE_ALL), nullptr,
                          0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(true, true, d3d7::CMP_LESSEQUAL), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));  // turned over, the winding is too
    ctx_->IASetInputLayout(mirrorLayout_);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = 6 * sizeof(float), offset = 0;
    ctx_->IASetVertexBuffers(0, 1, &mirrorVb_, &stride, &offset);
    ctx_->VSSetShader(mirrorVs_, nullptr, 0);
    ctx_->PSSetShader(mirrorPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    StageSetup st;
    st.addressU = st.addressV = d3d7::TADDRESS_WRAP;
    st.magFilter = d3d7::TFG_LINEAR;
    st.minFilter = d3d7::TFN_LINEAR;
    st.mipFilter = d3d7::TFP_LINEAR;
    ID3D11SamplerState* sampler = samplerState(st, 0);
    ctx_->PSSetSamplers(0, 1, &sampler);
    size_t drawn = 0;
    for (const Caster11& cast : mirrorCasters_) {
        const auto tex = textures_.find(cast.texture);
        if (tex == textures_.end() || !tex->second.srv) continue;  // nothing to show it by
        c.twin[1] = cast.ref;
        c.footprint = cast.alphaTest ? 1.0f : 0.0f;
        setConstants(c);
        ctx_->PSSetShaderResources(0, 1, &tex->second.srv);
        ctx_->Draw(cast.count, cast.first);
        ++drawn;
    }
    ctx_->PSSetShaderResources(0, 1, &none);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    if (!drawn) return;  // nothing drawn: the water as before
    mirrorMade_ = true;
    if (!mirrorLogged_)
        LOG_INFO("world reflections: %zu draws, %zu triangles of %s mirrored at %dx%d (Direct3D 11)", drawn,
                 mirrorVerts_.size() / 18, map_.c_str(), w, h);
    mirrorLogged_ = true;
}

}  // namespace uagfx::gpu
