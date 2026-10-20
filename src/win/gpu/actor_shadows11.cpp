#include <windows.h>

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>

#include "core/actor_shadows.hpp"
#include "core/daylight.hpp"
#include "core/ai/kinds.hpp"
#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/game_camera.hpp"
#include "core/sprite_exe.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/gpu/shaders.hpp"
#include "win/log.hpp"

// 0.60.1: the characters' shadows with Direct3D 11 - as Direct3D 12's actor_shadows.cpp: the game's round shadows
// and the sprite layers standing on them cast their shadows into a half-sized mask, softened and multiplied into the
// picture before the characters are drawn. 2.6.14: the light as Direct3D 12's - the sun's, the map's torches and
// lamps' (core/actor_shadows.hpp castsFor).
namespace uagfx::gpu {
namespace {

struct MaskVertex {  // as actor_shadows.cpp's (shadow_vs)
    float pos[4];
    float uv[2];
    float alphaZ[2];
    float feet[2];
};

constexpr float kLength = 0.45f;

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

bool Renderer11::ensureShadowMask(int w, int h) {
    if (!maxBlend_ || !mulBlend_) {
        D3D11_BLEND_DESC b{};
        D3D11_RENDER_TARGET_BLEND_DESC& r = b.RenderTarget[0];
        r.BlendEnable = TRUE;  // where several layers' shadows fall, the darkest of them
        r.SrcBlend = r.DestBlend = r.SrcBlendAlpha = r.DestBlendAlpha = D3D11_BLEND_ONE;
        r.BlendOp = r.BlendOpAlpha = D3D11_BLEND_OP_MAX;
        r.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;
        if (FAILED(device_->CreateBlendState(&b, &maxBlend_))) return false;
        r.SrcBlend = D3D11_BLEND_ZERO;  // the picture times the light the shadows leave
        r.DestBlend = D3D11_BLEND_SRC_COLOR;
        r.BlendOp = D3D11_BLEND_OP_ADD;
        r.SrcBlendAlpha = D3D11_BLEND_ZERO;
        r.DestBlendAlpha = D3D11_BLEND_ONE;
        r.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        r.RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device_->CreateBlendState(&b, &mulBlend_))) return false;
    }
    const int mw = std::max(1, (w + 1) / 2), mh = std::max(1, (h + 1) / 2);
    if (shadowMask_[0].texture && shadowMask_[0].width == mw && shadowMask_[0].height == mh) return true;
    for (Target& m : shadowMask_) {
        dropTarget(m);
        D3D11_TEXTURE2D_DESC d{};
        d.Width = UINT(mw);
        d.Height = UINT(mh);
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &m.texture)) ||
            FAILED(device_->CreateRenderTargetView(m.texture, nullptr, &m.rtv)) ||
            FAILED(device_->CreateShaderResourceView(m.texture, nullptr, &m.srv))) {
            LOG_ERROR("characters' shadows: no video memory for them at %dx%d (Direct3D 11)", mw, mh);
            for (Target& t : shadowMask_) dropTarget(t);
            return false;
        }
        m.width = mw;
        m.height = mh;
    }
    return true;
}

void Renderer11::applyActorShadows(const FrameRecord& rec, size_t from) {
    EffectSpan span(*this, kTimeShadows11);  // 0.89
    shadowsDone_ = true;  // tried once a frame
    const int w = target_.width, h = target_.height;
    // 2.5.21: the depth's views as depthViews makes them - of its own format (the 16-bit world's D16 too)
    if (!depth_ || !ensureShadowMask(w, h) || !depthViews()) return;
    // What each sprite cache cell holds as the record goes on (its uploads write cells before their draws).
    std::unordered_map<uint32_t, std::vector<SpriteCell>> cells;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    std::vector<actor_shadows::Blob> blobs;
    std::vector<actor_shadows::Layer> layers, rounds;
    bool started = false;
    for (const Command& cmd : rec.commands()) {
        if (cmd.type == Cmd::Draw && cmd.index == from) started = true;
        if (!started) continue;
        if (cmd.type == Cmd::Upload) {
            const UploadCmd& u = rec.uploads()[cmd.index];
            if (!u.slotCount) continue;
            auto it = cells.find(u.texture);
            if (it == cells.end()) {
                const auto t = textures_.find(u.texture);
                it = cells.emplace(u.texture, t != textures_.end() ? t->second.cells : std::vector<SpriteCell>()).first;
            }
            spriteCellsWritten(it->second, rec.slots().data() + u.firstSlot, u.slotCount);
            continue;
        }
        if (cmd.type != Cmd::Draw) continue;
        const DrawCmd& d = rec.draws()[cmd.index];
        if (rec.states()[d.state].ui || d.vertexCount != 4 || !d.texture[0]) continue;
        const size_t stride = FrameRecord::stride(d.fvf);
        if (d.vertexOffset + 4 * stride > v.size()) continue;
        const uint8_t* q = v.data() + d.vertexOffset;
        actor_shadows::Layer l{};
        float rhw = 0.0f, u0 = 0.0f, v0 = 0.0f, wv[4] = {0, 0, 0, 0};
        uint32_t diffuse = 0;
        for (int i = 0; i < 4; ++i) {
            float xyzw[4], uv[2];
            std::memcpy(xyzw, q + size_t(i) * stride, sizeof xyzw);
            std::memcpy(uv, q + size_t(i) * stride + 24, sizeof uv);
            if (i == 0) std::memcpy(&diffuse, q + 16, sizeof diffuse);
            l.corner[i][0] = xyzw[0];
            l.corner[i][1] = xyzw[1];
            l.uv[i][0] = uv[0];
            l.uv[i][1] = uv[1];
            wv[i] = xyzw[3];
            rhw += xyzw[3] * 0.25f;
            u0 += uv[0] * 0.25f;
            v0 += uv[1] * 0.25f;
        }
        if (rhw <= 0.0f || wv[0] <= 0.0f || wv[1] <= 0.0f || wv[2] <= 0.0f || wv[3] <= 0.0f) continue;
        {
            int order[4] = {0, 1, 2, 3};
            std::sort(order, order + 4, [&](int a, int b) { return l.corner[a][1] < l.corner[b][1]; });
            l.topZ = 0.5f * (1.0f / wv[order[0]] + 1.0f / wv[order[1]]);
            l.bottomZ = 0.5f * (1.0f / wv[order[2]] + 1.0f / wv[order[3]]);
        }
        l.alpha = float(diffuse >> 24) / 255.0f;
        l.texture = d.texture[0];
        l.owner = d.effect == uint8_t(effect_exe::Kind::None) ? d.effectSeed : 0;  // 2.6.3: its character's seed
        if (d.effect == uint8_t(effect_exe::Kind::Shadow)) {  // the game's round one
            float x0 = l.corner[0][0], x1 = x0, y0 = l.corner[0][1], y1 = y0;
            for (int i = 1; i < 4; ++i) {
                x0 = std::min(x0, l.corner[i][0]);
                x1 = std::max(x1, l.corner[i][0]);
                y0 = std::min(y0, l.corner[i][1]);
                y1 = std::max(y1, l.corner[i][1]);
            }
            blobs.push_back(actor_shadows::Blob{(x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (x1 - x0) * 0.5f, (y1 - y0) * 0.5f,
                                                1.0f / rhw, d.effectSeed});
            rounds.push_back(l);
            continue;
        }
        if (!ai::flatQuad(q, stride, d.vertexCount, d.indexCount)) continue;
        const auto t = textures_.find(d.texture[0]);
        if (t == textures_.end()) continue;
        const auto c = cells.find(d.texture[0]);
        const uint8_t source =
            spriteSourceAt(c != cells.end() ? c->second : t->second.cells, t->second.width, t->second.height, u0, v0);
        if (source == uint8_t(sprite_exe::Source::Player) || source == uint8_t(sprite_exe::Source::Monster) ||
            source == uint8_t(sprite_exe::Source::Npc))
            layers.push_back(l);
    }
    if (blobs.empty()) return;  // no character in view: the game's round shadows stay
    // 0.74: the sun's way and the shadows' length by the time of day (core/daylight.hpp)
    const daylight::Light sun = daylight::at(effects_.dayCycle ? effects_.dayNow : -1);
    // 2.6.14: and the map's torches and lamps under the ground and at night, as Direct3D 12's
    const rsw::World* layout = cameraValid_ && mapWorld_.axes().known() ? mapWorld_.world() : nullptr;
    std::vector<actor_shadows::Light> lights;
    if (layout) lights = actor_shadows::lightsOf(*layout, mapWorld_.lamps(), mapWorld_.axes());
    const float flicker = 0.35f * float(std::clamp(effects_.torchFlicker, 0, 100)) / 100.0f;
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    std::vector<actor_shadows::ShadowQuad> quads = actor_shadows::cast(
        blobs, layers,
        actor_shadows::castsFor(blobs, lights, camera::projectionFor(gameWidth_, gameHeight_), viewToWorld_,
                                layout != nullptr, sun.shadow, sun.length, outdoors_, night, kLength,
                                double(waterTime()), flicker));
    // the round ones, under the feet - 2.6.15: as much of each as effects.actor_shadows_spot leaves
    const std::vector<float> spots = actor_shadows::spotShares(quads, blobs.size(), effects_.actorShadowsSpot);
    for (size_t i = 0; i < rounds.size(); ++i) {
        const actor_shadows::Layer& r = rounds[i];
        const float share = i < spots.size() ? spots[i] : 1.0f;
        if (share < 0.01f) continue;
        actor_shadows::ShadowQuad sq{};
        std::memcpy(sq.corner, r.corner, sizeof sq.corner);
        std::memcpy(sq.uv, r.uv, sizeof sq.uv);
        sq.alpha = std::min(1.0f, r.alpha * 1.5f) * share;
        sq.viewZ = blobs[i].viewZ;
        sq.feetX = blobs[i].x;
        sq.feetY = blobs[i].y;
        sq.texture = r.texture;
        quads.push_back(sq);
    }
    std::sort(quads.begin(), quads.end(), [](const actor_shadows::ShadowQuad& a, const actor_shadows::ShadowQuad& b) {
        return a.texture < b.texture;
    });
    // The quads' vertices into a dynamic buffer.
    const size_t bytes = quads.size() * 6 * sizeof(MaskVertex);
    if (bytes > shadowVbBytes_) {
        drop(shadowVb_);
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = UINT(std::max<size_t>(bytes, 64u << 10));
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateBuffer(&bd, nullptr, &shadowVb_))) {
            shadowVb_ = nullptr;
            shadowVbBytes_ = 0;
            return;
        }
        shadowVbBytes_ = bd.ByteWidth;
    }
    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(ctx_->Map(shadowVb_, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
    const float k = float(scale_);
    const float opacity = 0.75f;
    auto* out = static_cast<MaskVertex*>(map.pData);
    for (size_t i = 0; i < quads.size(); ++i) {
        static const int kOrder[6] = {0, 1, 2, 2, 1, 3};
        for (int j = 0; j < 6; ++j) {
            const actor_shadows::ShadowQuad& s = quads[i];
            const int c = kOrder[j];
            MaskVertex& mv = out[i * 6 + size_t(j)];
            mv.pos[0] = (s.corner[c][0] * k + 0.5f) * 2.0f / float(w) - 1.0f;
            mv.pos[1] = 1.0f - (s.corner[c][1] * k + 0.5f) * 2.0f / float(h);
            mv.pos[2] = 0.0f;
            mv.pos[3] = 1.0f;
            mv.uv[0] = s.uv[c][0];
            mv.uv[1] = s.uv[c][1];
            mv.alphaZ[0] = s.alpha * opacity;
            mv.alphaZ[1] = s.viewZ;
            mv.feet[0] = s.feetX * k + 0.5f;
            mv.feet[1] = s.feetY * k + 0.5f;
        }
    }
    ctx_->Unmap(shadowVb_, 0);
    // The mask: cleared, the quads into it, the ground's depth read (the depth buffer unbound for it).
    targetBound_ = false;
    ID3D11ShaderResourceView* none[4] = {nullptr, nullptr, nullptr, nullptr};
    ctx_->PSSetShaderResources(0, 4, none);
    const float zero[4] = {0, 0, 0, 0};
    ctx_->ClearRenderTargetView(shadowMask_[0].rtv, zero);
    ctx_->OMSetRenderTargets(1, &shadowMask_[0].rtv, nullptr);
    const int mw = shadowMask_[0].width, mh = shadowMask_[0].height;
    D3D11_VIEWPORT vp{0.0f, 0.0f, float(mw), float(mh), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    D3D11_RECT whole{0, 0, mw, mh};
    ctx_->RSSetScissorRects(1, &whole);
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    ID3D11SamplerState* sampler = samplerState(linearClamp, 0);
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    Constants c{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[0] = float(w) / float(mw);  // the mask's pixels to the world's
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.targetSize[0] = float(mw);
    c.targetSize[1] = float(mh);
    c.scale = k;
    if (cameraValid_) std::copy(viewToWorld_, viewToWorld_ + 12, c.reproject);  // 2.6.3: the ground's level
    setConstants(c);
    ctx_->OMSetBlendState(maxBlend_, nullptr, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(false, false, d3d7::CMP_ALWAYS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(shadowLayout_);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = sizeof(MaskVertex), offset = 0;
    ctx_->IASetVertexBuffers(0, 1, &shadowVb_, &stride, &offset);
    ctx_->VSSetShader(shadowVs_, nullptr, 0);
    ctx_->PSSetShader(shadowPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetSamplers(0, 1, &sampler);
    ctx_->PSSetShaderResources(3, 1, &depthSrv_);  // sceneDepth : register(t3)
    for (size_t i = 0; i < quads.size();) {
        size_t j = i;
        while (j < quads.size() && quads[j].texture == quads[i].texture) ++j;
        const auto t = textures_.find(quads[i].texture);
        if (t != textures_.end() && t->second.srv) {
            ctx_->PSSetShaderResources(0, 1, &t->second.srv);
            ctx_->Draw(UINT((j - i) * 6), UINT(i * 6));
        }
        i = j;
    }
    ctx_->PSSetShaderResources(0, 4, none);
    // Softened across, then down; then multiplied into the picture.
    c = Constants{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(mw);
    c.uiPixels[3] = float(mh);
    ctx_->IASetInputLayout(nullptr);
    ctx_->VSSetShader(presentVs_, nullptr, 0);
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO, D3D11_COLOR_WRITE_ENABLE_ALL), nullptr,
                          0xFFFFFFFF);
    for (int i = 0; i < 2; ++i) {
        c.sourceRect[0] = i == 0 ? 1.0f : 0.0f;
        c.sourceRect[1] = i == 0 ? 0.0f : 1.0f;
        setConstants(c);
        ctx_->OMSetRenderTargets(1, &shadowMask_[1 - i].rtv, nullptr);
        ctx_->PSSetShader(shadowBlurPs_, nullptr, 0);
        ctx_->PSSetShaderResources(0, 1, &shadowMask_[i].srv);
        ctx_->Draw(3, 0);
        ctx_->PSSetShaderResources(0, 1, none);
    }
    c.sharpen = float(std::clamp(effects_.actorShadowsStrength, 0, 100)) / 100.0f;
    setConstants(c);
    ctx_->OMSetRenderTargets(1, &target_.rtv, nullptr);
    vp = D3D11_VIEWPORT{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    ctx_->RSSetViewports(1, &vp);
    whole = D3D11_RECT{0, 0, w, h};
    ctx_->RSSetScissorRects(1, &whole);
    ctx_->OMSetBlendState(mulBlend_, nullptr, 0xFFFFFFFF);
    ctx_->PSSetShader(shadowApplyPs_, nullptr, 0);
    ctx_->PSSetShaderResources(0, 1, &shadowMask_[0].srv);
    ctx_->Draw(3, 0);
    ctx_->PSSetShaderResources(0, 4, none);
    if (!shadowsLogged_) {
        const size_t owned = size_t(std::count_if(quads.begin(), quads.end(),
                                                  [](const actor_shadows::ShadowQuad& q) { return q.owned; }));
        LOG_INFO("characters' shadows: %u characters, %u layers of their sprites (%u cast, %u known as their "
                 "characters'; Direct3D 11, the sun's light)",
                 unsigned(blobs.size()), unsigned(layers.size()), unsigned(quads.size() - rounds.size()),
                 unsigned(owned));
    }
    shadowsLogged_ = true;
}

}  // namespace uagfx::gpu
