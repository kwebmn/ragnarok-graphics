#include <windows.h>

#include <d3d11.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "core/ai/kinds.hpp"
#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/game_camera.hpp"
#include "core/point_shadows.hpp"
#include "core/sprite_exe.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

// 3.0.16-beta.35 (the owner: «перенести тени от факелов и отпечаток тел героев на D3D11»): [effects] torch_shadows
// with Direct3D 11 - Direct3D 12's torch_shadows.cpp: the frame's casters (mirror11.cpp noteCaster) and the characters
// standing (turned to each light) from the lights picked into one face each of their cubes (an atlas of six faces a
// light, the leaves' rows below theirs), their distance of the reach as the depth; torch_light_ps reads it (t5).
namespace uagfx::gpu {
namespace {

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

constexpr int kF = point_shadows::kFaceTexels;
constexpr size_t kMostStanding = 128;

bool boxNear(const float lo[3], const float hi[3], const float at[3], float reach) {
    float d2 = 0.0f;
    for (int k = 0; k < 3; ++k) {
        const float e = at[k] < lo[k] ? lo[k] - at[k] : at[k] > hi[k] ? at[k] - hi[k] : 0.0f;
        d2 += e * e;
    }
    return d2 < reach * reach;
}

}  // namespace

uint8_t Renderer11::characterSource(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) const {
    if (s.ui || !s.blend || d.vertexCount != 4 || !d.texture[0]) return 0;
    const auto t = textures_.find(d.texture[0]);
    if (t == textures_.end() || t->second.cells.empty() || t->second.leafy) return 0;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > v.size()) return 0;
    const uint8_t* q = v.data() + d.vertexOffset;
    if (!ai::flatQuad(q, stride, d.vertexCount, d.indexCount)) return 0;
    float u0 = 0.0f, v0 = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float uv[2];
        std::memcpy(uv, q + size_t(i) * stride + 24, sizeof uv);
        u0 += uv[0] * 0.25f;
        v0 += uv[1] * 0.25f;
    }
    const uint8_t source = spriteSourceAt(t->second.cells, t->second.width, t->second.height, u0, v0);
    return source == uint8_t(sprite_exe::Source::Player) || source == uint8_t(sprite_exe::Source::Monster) ||
                   source == uint8_t(sprite_exe::Source::Npc)
               ? source
               : 0;
}

// A character's sprite: its corners in the world, standing (torch_shadows.cpp noteStanding)
void Renderer11::noteStanding(const FrameRecord& rec, const DrawCmd& d) {
    if (standing_.size() >= kMostStanding || !cameraValid_) return;
    if (!characterSource(rec, d, rec.states()[d.state])) return;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    const uint8_t* q = v.data() + d.vertexOffset;
    float world[4][3], screen[4][2], uv[4][2];
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float* m = viewToWorld_;
    for (int i = 0; i < 4; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, q + size_t(i) * stride, sizeof xyzw);
        std::memcpy(uv[i], q + size_t(i) * stride + 24, sizeof uv[i]);
        if (!(xyzw[3] > 0.0f)) return;
        const float vz = 1.0f / xyzw[3], vx = (xyzw[0] - p.cx) * vz / p.f0, vy = (xyzw[1] - p.cy) * vz / p.f1;
        world[i][0] = vx * m[0] + vy * m[3] + vz * m[6] + m[9];
        world[i][1] = vx * m[1] + vy * m[4] + vz * m[7] + m[10];
        world[i][2] = vx * m[2] + vy * m[5] + vz * m[8] + m[11];
        screen[i][0] = xyzw[0];
        screen[i][1] = xyzw[1];
    }
    point_shadows::Standing s;
    if (!point_shadows::standingOf(world, screen, uv, s)) return;
    s.texture = d.texture[0];
    standing_.push_back(s);
}

std::vector<int> Renderer11::drawTorchShadows(const std::vector<point_shadows::Candidate>& lights) {
    std::vector<int> picked;
    shadowedLights_.clear();
    if (!shadowCollecting() || lights.empty() || casters_.empty()) return picked;
    if (!shadowAtlas_) {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = UINT(6 * kF);
        td.Height = UINT(2 * point_shadows::kMostLights * kF);  // the lights' rows, the leaves' below them
        td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = DXGI_FORMAT_D32_FLOAT;
        dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = DXGI_FORMAT_R32_FLOAT;
        sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sv.Texture2D.MipLevels = 1;
        if (FAILED(device_->CreateTexture2D(&td, nullptr, &shadowAtlas_)) ||
            FAILED(device_->CreateDepthStencilView(shadowAtlas_, &dv, &shadowAtlasDsv_)) ||
            FAILED(device_->CreateShaderResourceView(shadowAtlas_, &sv, &shadowAtlasSrv_))) {
            LOG_ERROR("torch shadows: no video memory for their atlas (Direct3D 11): none");
            drop(shadowAtlasSrv_);
            drop(shadowAtlasDsv_);
            drop(shadowAtlas_);
            shadowFailed_ = true;
            return picked;
        }
    }
    const camera::Projection proj = camera::projectionFor(gameWidth_, gameHeight_);
    picked = point_shadows::pick(lights, proj.nearZ);
    if (picked.empty()) return picked;
    struct Placed {
        float at[3];
        float reach;
    };
    std::vector<Placed> placed;
    const float* m = viewToWorld_;
    for (int i : picked) {
        const float* v = lights[size_t(i)].view;
        placed.push_back(Placed{{v[0] * m[0] + v[1] * m[3] + v[2] * m[6] + m[9],
                                 v[0] * m[1] + v[1] * m[4] + v[2] * m[7] + m[10],
                                 v[0] * m[2] + v[1] * m[5] + v[2] * m[8] + m[11]},
                                lights[size_t(i)].reach});
    }
    struct Nearby {
        size_t caster;
        uint32_t first;   // its corners' first in the upload
        uint32_t lights;  // a bit a picked light
        bool leaves;
    };
    std::vector<Nearby> nearby;
    std::vector<float> verts;  // the nearby casters' corners, then the turned characters'
    for (size_t k = 0; k < casters_.size(); ++k) {
        const Caster11& c = casters_[k];
        uint32_t mask = 0;
        for (size_t s = 0; s < placed.size(); ++s)
            if (boxNear(c.lo, c.hi, placed[s].at, placed[s].reach)) mask |= 1u << s;
        if (!mask) continue;
        const auto t = textures_.find(c.texture);
        nearby.push_back(Nearby{k, uint32_t(verts.size() / 6), mask,
                                effects_.torchLeafLight && t != textures_.end() && t->second.leafy});
        verts.insert(verts.end(), casterVerts_.begin() + std::ptrdiff_t(c.first) * 6,
                     casterVerts_.begin() + std::ptrdiff_t(c.first + c.count) * 6);
    }
    // The characters within a light's reach, each turned to face it - two triangles a light, after the casters'
    struct Turned {
        uint32_t first;
        size_t light;
        uint32_t texture;
        float lo[3], hi[3];
    };
    std::vector<Turned> turned;
    for (size_t s = 0; s < placed.size(); ++s)
        for (const point_shadows::Standing& st : standing_) {
            float lo[3], hi[3];
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(st.bottom[k], st.top[k]) - st.halfWidth - point_shadows::kSpritePush;
                hi[k] = std::max(st.bottom[k], st.top[k]) + st.halfWidth + point_shadows::kSpritePush;
            }
            if (!boxNear(lo, hi, placed[s].at, placed[s].reach)) continue;
            float c[4][3];
            point_shadows::facing(st, placed[s].at, point_shadows::kSpritePush, c);
            Turned tq{uint32_t(verts.size() / 6), s, st.texture, {1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}};
            const uint32_t white = 0xFFFFFFFFu;
            float colour;
            std::memcpy(&colour, &white, sizeof colour);
            for (int corner : {0, 1, 2, 2, 1, 3}) {
                const float vtx[6] = {c[corner][0], c[corner][1], c[corner][2], st.uv[corner][0], st.uv[corner][1],
                                      colour};
                verts.insert(verts.end(), vtx, vtx + 6);
                for (int k = 0; k < 3; ++k) {
                    tq.lo[k] = std::min(tq.lo[k], c[corner][k]);
                    tq.hi[k] = std::max(tq.hi[k], c[corner][k]);
                }
            }
            turned.push_back(tq);
        }
    if (nearby.empty() && turned.empty()) {
        picked.clear();
        return picked;
    }
    flushBatch();
    if (!fillBuffer(torchShadowVb_, torchShadowVbBytes_, D3D11_BIND_VERTEX_BUFFER, verts.data(),
                    verts.size() * sizeof(float), nullptr)) {
        picked.clear();
        return picked;
    }
    targetBound_ = false;  // the next draw binds the target, its state and the game's shaders again
    ID3D11ShaderResourceView* none = nullptr;
    ctx_->PSSetShaderResources(5, 1, &none);
    ctx_->OMSetRenderTargets(0, nullptr, shadowAtlasDsv_);
    ctx_->ClearDepthStencilView(shadowAtlasDsv_, D3D11_CLEAR_DEPTH, 1.0f, 0);
    ctx_->OMSetBlendState(blendState(false, d3d7::BLEND_ONE, d3d7::BLEND_ZERO, 0), nullptr, 0xFFFFFFFF);
    ctx_->OMSetDepthStencilState(depthState(true, true, d3d7::CMP_LESS), 0);
    ctx_->RSSetState(rasterState(d3d7::CULL_NONE));
    ctx_->IASetInputLayout(torchShadowLayout_);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = 6 * sizeof(float), offset = 0;
    ctx_->IASetVertexBuffers(0, 1, &torchShadowVb_, &stride, &offset);
    ctx_->VSSetShader(torchShadowVs_, nullptr, 0);
    ctx_->PSSetShader(torchShadowPs_, nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, &constants_);
    ctx_->PSSetConstantBuffers(0, 1, &constants_);
    StageSetup st;
    st.addressU = st.addressV = d3d7::TADDRESS_WRAP;
    st.magFilter = d3d7::TFG_LINEAR;
    st.minFilter = d3d7::TFN_LINEAR;
    st.mipFilter = d3d7::TFP_LINEAR;
    ID3D11SamplerState* sampler = samplerState(st, 0);
    ctx_->PSSetSamplers(0, 1, &sampler);
    size_t draws = 0, characters = 0;
    for (size_t s = 0; s < placed.size(); ++s) {
        const Placed& l = placed[s];
        for (int f = 0; f < 6; ++f) {
            const point_shadows::Face fc = point_shadows::face(f);
            Constants c{};
            for (int k = 0; k < 3; ++k) {
                c.reproject[k] = fc.right[k];
                c.reproject[4 + k] = fc.up[k];
                c.reproject[8 + k] = fc.fwd[k];
                c.projection[k] = l.at[k];
            }
            c.projection[3] = l.reach;
            // one caster's draw into this face (the leaves' into their own row): its texture's holes when it has one
            const auto drawOne = [&](uint32_t texture, bool alphaTest, float ref, uint32_t first, uint32_t count,
                                     bool leaves) {
                const auto tex = textures_.find(texture);
                ID3D11ShaderResourceView* srv = tex != textures_.end() ? tex->second.srv : nullptr;
                if (!srv && alphaTest) return;  // its holes not known
                const int row = leaves ? int(s) + point_shadows::kMostLights : int(s);
                const D3D11_VIEWPORT vp{float(f * kF), float(row * kF), float(kF), float(kF), 0.0f, 1.0f};
                const D3D11_RECT box{LONG(f * kF), LONG(row * kF), LONG((f + 1) * kF), LONG((row + 1) * kF)};
                ctx_->RSSetViewports(1, &vp);
                ctx_->RSSetScissorRects(1, &box);
                c.twin[1] = ref;
                c.footprint = alphaTest && srv ? 1.0f : 0.0f;
                setConstants(c);
                ctx_->PSSetShaderResources(0, 1, &srv);
                ctx_->Draw(count, first);
                ++draws;
            };
            for (const Nearby& n : nearby) {
                if (!(n.lights & (1u << s))) continue;
                const Caster11& cast = casters_[n.caster];
                float lo[3], hi[3];
                for (int k = 0; k < 3; ++k) {
                    lo[k] = cast.lo[k] - l.at[k];
                    hi[k] = cast.hi[k] - l.at[k];
                }
                if (!point_shadows::boxOnFace(lo, hi, f)) continue;
                drawOne(cast.texture, cast.alphaTest, cast.ref, n.first, cast.count, n.leaves);
            }
            for (const Turned& tq : turned) {  // the characters, their pictures' holes let through
                if (tq.light != s) continue;
                float lo[3], hi[3];
                for (int k = 0; k < 3; ++k) {
                    lo[k] = tq.lo[k] - l.at[k];
                    hi[k] = tq.hi[k] - l.at[k];
                }
                if (!point_shadows::boxOnFace(lo, hi, f)) continue;
                drawOne(tq.texture, true, 0.5f, tq.first, 6, false);
                ++characters;
            }
        }
    }
    ctx_->PSSetShaderResources(0, 1, &none);
    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    for (const Placed& l : placed) shadowedLights_.push_back({l.at[0], l.at[1], l.at[2]});
    if (!shadowLogged_)
        LOG_INFO("torch shadows: %zu lights of %zu with shadows, %zu of %zu draws of the world nearby them, %zu "
                 "characters' sprites turned, %zu draws into their faces (Direct3D 11, [effects] torch_shadows)",
                 placed.size(), lights.size(), nearby.size(), casters_.size(), turned.size(), draws);
    shadowLogged_ = true;
    (void)characters;
    return picked;
}

}  // namespace uagfx::gpu
