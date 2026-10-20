#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "core/actor_shadows.hpp"
#include "core/daylight.hpp"
#include "core/ai/kinds.hpp"
#include "core/effect_exe.hpp"
#include "core/game_camera.hpp"
#include "core/sprite_exe.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

// A corner of a shadow in the mask: its place (clip space), its texture's coordinates, the layer's alpha and the
// feet's view depth, (0.39.1) where the feet stand (target pixels) (shadow_vs).
struct MaskVertex {
    float pos[4];
    float uv[2];
    float alphaZ[2];
    float feet[2];
};

constexpr float kLength = 0.45f;  // of a layer's height above the feet, the shadow's length

}  // namespace

void Renderer::Impl::noteSpriteCells(Texture& t, const SpriteCell* slots, uint32_t count) {
    spriteCellsWritten(t.spriteCells, slots, count);
}

void Renderer::Impl::releaseShadowMask() {
    Slot& s = slots[slot];  // what the open list recorded may still read them
    for (ID3D12Resource*& r : shadowMask) {
        if (r) s.garbage.push_back(r);
        r = nullptr;
    }
    shadowMaskW = shadowMaskH = 0;
}

// The two half-sized R8 masks and the pipelines; false when they cannot be made (logged once a size).
bool Renderer::Impl::ensureShadowMask(int w, int h) {
    if (!shadowVs || !shadowPs || !shadowBlurPs || !shadowApplyPs) return false;
    if (!shadowQuadPipeline) {
        const D3D12_INPUT_ELEMENT_DESC layout[4] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p = fullScreenPipeline(root, shadowVs, shadowPs, DXGI_FORMAT_R8_UNORM, 1);
        p.InputLayout = {layout, 4};
        D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
        b.BlendEnable = TRUE;  // where several layers' shadows fall, the darkest of them: no layer darkens another
        b.SrcBlend = D3D12_BLEND_ONE;
        b.DestBlend = D3D12_BLEND_ONE;
        b.BlendOp = D3D12_BLEND_OP_MAX;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;
        b.DestBlendAlpha = D3D12_BLEND_ONE;
        b.BlendOpAlpha = D3D12_BLEND_OP_MAX;
        b.LogicOp = D3D12_LOGIC_OP_NOOP;
        b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&shadowQuadPipeline))))
            shadowQuadPipeline = nullptr;
        p = fullScreenPipeline(root, presentVs, shadowBlurPs, DXGI_FORMAT_R8_UNORM, 1);
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&shadowBlurPipeline))))
            shadowBlurPipeline = nullptr;
        if (!shadowQuadPipeline || !shadowBlurPipeline) {
            LOG_ERROR("characters' shadows: their pipelines failed: they stay the game's");
            releaseCom(shadowPs);  // not tried again
            return false;
        }
    }
    const int mw = std::max(1, (w + 1) / 2), mh = std::max(1, (h + 1) / 2);
    if (shadowMask[0] && mw == shadowMaskW && mh == shadowMaskH) return true;
    if (shadowMaskFailed && mw == shadowMaskW && mh == shadowMaskH) return false;
    releaseShadowMask();
    shadowMaskW = mw;
    shadowMaskH = mh;
    shadowMaskFailed = true;
    D3D12_CLEAR_VALUE none{};
    none.Format = DXGI_FORMAT_R8_UNORM;
    for (int i = 0; i < 2; ++i) {
        shadowMask[i] = createTexture2D(device, mw, mh, 1, DXGI_FORMAT_R8_UNORM,
                                        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        &none);
        shadowMaskState[i] = D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (!shadowMask[i]) {
            LOG_ERROR("characters' shadows: no video memory for them at %dx%d: they stay the game's", mw, mh);
            releaseShadowMask();
            shadowMaskW = mw;  // remembered as failed
            shadowMaskH = mh;
            return false;
        }
        device->CreateRenderTargetView(shadowMask[i], nullptr, rtvHandle(kRtvShadow + UINT(i)));
        device->CreateShaderResourceView(shadowMask[i], nullptr, cpuSrvHandle(shadowMaskSrv[i]));
    }
    shadowMaskFailed = false;
    return true;
}

// Multiplied into the target (its samples) or the world under DLSS (one sample).
ID3D12PipelineState* Renderer::Impl::shadowApplyPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (shadowApplyPipelines[code]) return shadowApplyPipelines[code];
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, presentVs, shadowApplyPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;  // the picture times the light the shadows leave
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
                                                   reinterpret_cast<void**>(&shadowApplyPipelines[code])))) {
        LOG_ERROR("characters' shadows: the pipeline onto %u samples failed", 1u << code);
        shadowApplyPipelines[code] = nullptr;
    }
    return shadowApplyPipelines[code];
}

// The frame's characters (from this draw on: the game's round shadows, known by the driver, and the sprite layers
// that stand on them) cast their shadows into the mask, which is softened and multiplied into the picture - before
// the characters are drawn. The round shadows go into it too, and are not drawn on their own.
// The way the sun's shadows fall on the screen (a unit vector, game pixels' axes).
// 0.74: by the time of day when its cycle is on (core/daylight.hpp).
// 2.0.1: the sun fixed in the world - its shadows' way along the ground (sunGround, the daylight's way taken in the
// world's x and z) turned onto the screen by the camera; until 2.0 the screen's way itself, and the real shadows
// turned with the camera.
void Renderer::Impl::sunOnScreen(float out[2]) const {
    const daylight::Light l = daylight::at(effects.dayCycle ? effects.dayNow : -1);
    out[0] = l.shadow[0];
    out[1] = l.shadow[1];
    if (!viewToWorldValid) return;
    float g[3];
    sunGround(g);
    const float* m = viewToWorld;
    const float sx = g[0] * m[0] + g[1] * m[1] + g[2] * m[2], sy = g[0] * m[3] + g[1] * m[4] + g[2] * m[5];
    const float len = std::sqrt(sx * sx + sy * sy);
    if (len < 1e-4f) return;
    out[0] = sx / len;
    out[1] = sy / len;
}

bool Renderer::Impl::mapSun(float toSun[3]) const {
    const rsw::World* w = mapWorld.world();
    const map::Axes axes = mapWorld.axes();
    if (!w || !w->lightKnown || !axes.known()) return false;
    // as the client turns the file's light into its way (longitude about up, latitude from straight above), the
    // file's room up along -y: the way to the light
    const float lon = float(w->lightLongitude) * 0.0174533f, lat = float(w->lightLatitude) * 0.0174533f;
    toSun[0] = float(axes.x) * -std::cos(lon) * std::sin(lat);
    toSun[1] = -std::cos(lat);
    toSun[2] = float(axes.z) * -std::sin(lon) * std::sin(lat);
    return true;
}

float Renderer::Impl::sunHeight() const {
    float toSun[3];
    if (!effects.dayCycle && mapSun(toSun))
        return std::clamp(std::asin(std::clamp(-toSun[1], -1.0f, 1.0f)), 15.0f * 0.0174533f, 70.0f * 0.0174533f);
    const daylight::Light day = daylight::at(effects.dayCycle ? effects.dayNow : -1);
    return std::clamp(50.0f / day.length * 0.55f, 15.0f, 70.0f) * 0.0174533f;
}

// 2.1.3: the shadows fall the map's way (its painted shadows' and ours agree), the day cycle only lifting and lowering
// the sun; a map without its light, the day's way as before.
void Renderer::Impl::sunGround(float out[3]) const {
    float toSun[3];
    if (mapSun(toSun)) {
        const float len = std::sqrt(toSun[0] * toSun[0] + toSun[2] * toSun[2]);
        if (len > 1e-4f) {
            out[0] = -toSun[0] / len;
            out[1] = 0.0f;
            out[2] = -toSun[2] / len;
            return;
        }
    }
    const daylight::Light l = daylight::at(effects.dayCycle ? effects.dayNow : -1);
    const float len = std::max(std::sqrt(l.shadow[0] * l.shadow[0] + l.shadow[1] * l.shadow[1]), 1e-4f);
    out[0] = l.shadow[0] / len;
    out[1] = 0.0f;
    out[2] = l.shadow[1] / len;
}

// 0.94: a round shadow under a character or a monster: its middle, in the world, for the footprints and the ripples.
void Renderer::Impl::noteFoot(const FrameRecord& rec, const DrawCmd& d) {
    if (!viewToWorldValid || d.vertexCount != 4 || frameFeet.size() >= 256) return;
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > rec.vertexBytes().size()) return;
    const uint8_t* q = rec.vertexBytes().data() + d.vertexOffset;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, rhw = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, q + size_t(i) * stride, sizeof xyzw);
        x0 = std::min(x0, xyzw[0]);
        x1 = std::max(x1, xyzw[0]);
        y0 = std::min(y0, xyzw[1]);
        y1 = std::max(y1, xyzw[1]);
        rhw += xyzw[3] * 0.25f;
    }
    if (rhw <= 0.0f) return;
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float* fm = viewToWorld;
    const float vz = 1.0f / rhw, vx = ((x0 + x1) * 0.5f - proj.cx) * vz / proj.f0,
                vy = ((y0 + y1) * 0.5f - proj.cy) * vz / proj.f1;
    frameFeet.push_back(map::Point{vx * fm[0] + vy * fm[3] + vz * fm[6] + fm[9],
                                   vx * fm[1] + vy * fm[4] + vz * fm[7] + fm[10],
                                   vx * fm[2] + vy * fm[5] + vz * fm[8] + fm[11]});
}

bool Renderer::Impl::applyActorShadows(const FrameRecord& rec, size_t from) {
    actorShadowsDone = true;  // tried once a frame, whatever comes of it
    EffectTime timing(*this, kTimeActorShadows);  // 0.39.2
    if (!ensureScene("characters' shadows")) return true;
    const int w = sceneWidth, h = sceneHeight;
    ID3D12PipelineState* apply = shadowApplyPipeline(upscale ? 1 : samples);
    if (!ensureShadowMask(w, h) || !apply) return true;
    float sun[2];
    sunOnScreen(sun);
    const float shadowLength = daylight::at(effects.dayCycle ? effects.dayNow : -1).length;  // 0.74
    // What each sprite cache cell holds as the record goes on (its uploads write cells before their draws).
    std::unordered_map<uint32_t, std::vector<SpriteCell>> cells;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    std::vector<actor_shadows::Blob> blobs;
    std::vector<actor_shadows::Layer> layers;
    std::vector<actor_shadows::Layer> rounds;  // the round shadows themselves, as layers of their own
    bool started = false;
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    for (const Command& cmd : rec.commands()) {
        if (cmd.type == Cmd::Draw && cmd.index == from) started = true;
        if (!started) continue;
        if (cmd.type == Cmd::Upload) {
            const UploadCmd& u = rec.uploads()[cmd.index];
            if (!u.slotCount) continue;
            auto it = cells.find(u.texture);
            if (it == cells.end()) {
                const auto t = textures.find(u.texture);
                it = cells.emplace(u.texture, t != textures.end() ? t->second.spriteCells : std::vector<SpriteCell>())
                         .first;
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
        float rhw = 0.0f, u0 = 0.0f, v0 = 0.0f, w[4] = {0, 0, 0, 0};
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
            w[i] = xyzw[3];
            rhw += xyzw[3] * 0.25f;
            u0 += uv[0] * 0.25f;
            v0 += uv[1] * 0.25f;
        }
        if (rhw <= 0.0f || w[0] <= 0.0f || w[1] <= 0.0f || w[2] <= 0.0f || w[3] <= 0.0f) continue;
        {  // 0.39.1: the depth of its top and of its bottom - the game tilts a sprite's depth with its height
            int order[4] = {0, 1, 2, 3};
            std::sort(order, order + 4, [&](int a, int b) { return l.corner[a][1] < l.corner[b][1]; });
            l.topZ = 0.5f * (1.0f / w[order[0]] + 1.0f / w[order[1]]);
            l.bottomZ = 0.5f * (1.0f / w[order[2]] + 1.0f / w[order[3]]);
        }
        const float viewZ = 1.0f / rhw;
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
                                                viewZ, d.effectSeed});
            rounds.push_back(l);
            continue;
        }
        if (!ai::flatQuad(q, stride, d.vertexCount, d.indexCount)) continue;
        const auto t = textures.find(d.texture[0]);
        if (t == textures.end()) continue;
        const auto c = cells.find(d.texture[0]);
        const uint8_t source = spriteSourceAt(c != cells.end() ? c->second : t->second.spriteCells, t->second.width,
                                              t->second.height, u0, v0);
        if (source == uint8_t(sprite_exe::Source::Player) || source == uint8_t(sprite_exe::Source::Monster) ||
            source == uint8_t(sprite_exe::Source::Npc))
            layers.push_back(l);
    }
    // 0.94: the feet for the footprints are noted as the round shadows are drawn (noteFoot)
    if (blobs.empty()) return true;  // no character in view: the game's round shadows, if any, stay
    // 2.6.14: each character's shadows - the sun's under the open sky by day; under the ground, and as the night
    // darkens the world, the map's two nearest torches or lamps', longer the farther off, fainter as their light
    // fades, trembling with a flame (core/actor_shadows.hpp castsFor)
    const rsw::World* layout = viewToWorldValid && mapWorld.axes().known() ? mapWorld.world() : nullptr;
    std::vector<actor_shadows::Light> lights;
    if (layout) lights = actor_shadows::lightsOf(*layout, mapWorld.lamps(), mapWorld.axes());
    const float flicker = torch::flickerAmount(effects.torchFlicker);
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    std::vector<actor_shadows::ShadowQuad> quads = actor_shadows::cast(
        blobs, layers,
        actor_shadows::castsFor(blobs, lights, proj, viewToWorld, layout != nullptr, sun, shadowLength, mapOutdoors,
                                night, kLength, double(waterTime()), flicker,
                                actor_shadows::Walls{mapWorld.ground(), mapWorld.axes()}));  // 2.6.18
    // the round ones, under the feet - 2.6.15: as much of each as effects.actor_shadows_spot leaves
    const std::vector<float> spots = actor_shadows::spotShares(quads, blobs.size(), effects.actorShadowsSpot);
    for (size_t i = 0; i < rounds.size(); ++i) {
        const actor_shadows::Layer& r = rounds[i];
        const float share = i < spots.size() ? spots[i] : 1.0f;
        if (share < 0.01f) continue;
        actor_shadows::ShadowQuad q{};
        std::memcpy(q.corner, r.corner, sizeof q.corner);
        std::memcpy(q.uv, r.uv, sizeof q.uv);
        q.alpha = std::min(1.0f, r.alpha * 1.5f) * share;
        q.viewZ = blobs[i].viewZ;
        q.feetX = blobs[i].x;
        q.feetY = blobs[i].y;
        q.texture = r.texture;
        quads.push_back(q);
    }
    std::sort(quads.begin(), quads.end(),
              [](const actor_shadows::ShadowQuad& a, const actor_shadows::ShadowQuad& b) {
                  return a.texture < b.texture;
              });
    // The mask: cleared, then each texture's quads (two triangles each) at the mask's size.
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    size_t groups = 0;  // textures: a table of two each
    for (size_t i = 0; i < quads.size(); ++i) groups += i == 0 || quads[i].texture != quads[i - 1].texture;
    if (!copySceneDepth()) return false;
    if (!makeRoom(linearClamp, linearClamp, 0, UINT(5 + 2 * std::min<size_t>(groups, 64)))) return false;
    const Upload up = allocUpload(quads.size() * 6 * sizeof(MaskVertex), 16);
    if (!up.cpu) return true;  // no upload memory this frame: the game's shadows
    const float k = float(upscale ? worldScale : scale);
    const float jx = upscale ? jitter[0] : 0.0f, jy = upscale ? jitter[1] : 0.0f;
    MaskVertex* out = reinterpret_cast<MaskVertex*>(up.cpu);
    const float opacity = 0.75f;
    for (size_t i = 0; i < quads.size(); ++i) {
        static const int kOrder[6] = {0, 1, 2, 2, 1, 3};
        for (int j = 0; j < 6; ++j) {
            const actor_shadows::ShadowQuad& s = quads[i];
            const int c = kOrder[j];
            MaskVertex& mv = out[i * 6 + size_t(j)];
            mv.pos[0] = (s.corner[c][0] * k + 0.5f + jx) * 2.0f / float(w) - 1.0f;
            mv.pos[1] = 1.0f - (s.corner[c][1] * k + 0.5f + jy) * 2.0f / float(h);
            mv.pos[2] = 0.0f;
            mv.pos[3] = 1.0f;
            mv.uv[0] = s.uv[c][0];
            mv.uv[1] = s.uv[c][1];
            mv.alphaZ[0] = s.alpha * opacity;
            mv.alphaZ[1] = s.viewZ;
            mv.feet[0] = s.feetX * k + 0.5f + jx;
            mv.feet[1] = s.feetY * k + 0.5f + jy;
        }
    }
    transition(shadowMask[0], shadowMaskState[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(kRtvShadow);
    const float zero[4] = {0, 0, 0, 0};
    list->ClearRenderTargetView(rtv, zero, 0, nullptr);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(shadowMaskW), float(shadowMaskH), 0.0f, 1.0f};
    const D3D12_RECT whole{0, 0, shadowMaskW, shadowMaskH};
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &whole);
    placeSamples(false);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(shadowQuadPipeline);
    Constants c{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[0] = float(w) / float(shadowMaskW);  // the mask's pixels to the world's
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.targetSize[0] = float(shadowMaskW);
    c.targetSize[1] = float(shadowMaskH);
    c.scale = k;
    if (viewToWorldValid) std::copy(viewToWorld, viewToWorld + 12, c.reproject);  // 2.6.3: the ground's level
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplerTable(linearClamp, linearClamp, 0));
    list->SetGraphicsRootDescriptorTable(kRootScene, sceneTable(sceneColorSrv, sceneDepthSrv, sceneColorSrv));
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_VERTEX_BUFFER_VIEW vb{up.gpu, UINT(quads.size() * 6 * sizeof(MaskVertex)), UINT(sizeof(MaskVertex))};
    list->IASetVertexBuffers(0, 1, &vb);
    for (size_t i = 0, drawn = 0; i < quads.size() && drawn < 64;) {  // 64 textures: as much room as was made
        size_t j = i;
        while (j < quads.size() && quads[j].texture == quads[i].texture) ++j;
        const auto t = textures.find(quads[i].texture);
        if (t != textures.end()) {
            ++drawn;
            transition(t->second.resource, t->second.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(t->second.srv, t->second.srv));
            list->DrawInstanced(UINT((j - i) * 6), 1, UINT(i * 6), 0);
        }
        i = j;
    }
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    // Softened, then multiplied in.
    c = Constants{};
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[2] = float(shadowMaskW);
    c.uiPixels[3] = float(shadowMaskH);
    for (int i = 0; i < 2; ++i) {  // across, then down
        transition(shadowMask[i], shadowMaskState[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        c.sourceRect[0] = i == 0 ? 1.0f : 0.0f;
        c.sourceRect[1] = i == 0 ? 0.0f : 1.0f;
        const D3D12_CPU_DESCRIPTOR_HANDLE to = rtvHandle(kRtvShadow + UINT(1 - i));
        if (!effectPass(shadowBlurPipeline, shadowMask[1 - i], shadowMaskState[1 - i], to, shadowMaskW, shadowMaskH, c,
                        shadowMaskSrv[i], false))
            return false;
    }
    transition(shadowMask[0], shadowMaskState[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    c.sharpen = float(std::clamp(effects.actorShadowsStrength, 0, 100)) / 100.0f;
    const UINT mask = shadowMaskSrv[0];
    const bool done = worldPass(apply, w, h, c, mask, false);
    if (!done) return false;
    actorShadowsDrawn = true;
    if (!actorShadowsLogged) {
        const size_t owned = size_t(std::count_if(quads.begin(), quads.end(),
                                                  [](const actor_shadows::ShadowQuad& q) { return q.owned; }));
        LOG_INFO("characters' shadows: %u characters, %u layers of their sprites (%u cast, %u known as their "
                 "characters')%s",
                 unsigned(blobs.size()), unsigned(layers.size()), unsigned(quads.size() - rounds.size()),
                 unsigned(owned), layout ? ", lit by the map's torches and lamps" : "");
    }
    actorShadowsLogged = true;
    return true;
}

}  // namespace uagfx::gpu
