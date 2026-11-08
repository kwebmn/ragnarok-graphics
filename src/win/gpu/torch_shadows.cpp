#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "core/point_shadows.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr int kF = point_shadows::kFaceTexels;

// Whether a box comes within `reach` of a place
bool boxNear(const float lo[3], const float hi[3], const float at[3], float reach) {
    float d2 = 0.0f;
    for (int k = 0; k < 3; ++k) {
        const float e = at[k] < lo[k] ? lo[k] - at[k] : at[k] > hi[k] ? at[k] - hi[k] : 0.0f;
        d2 += e * e;
    }
    return d2 < reach * reach;
}

}  // namespace

// 3.0.16-beta.16 (the owner: «тени от факелов и ламп» - «начинай»): [effects] torch_shadows. The pipeline: the
// casters' triangles (x y z, u v, the game's colour - as the mirror's) from a light's place into one face of its cube,
// their distance from it (of its reach) as the depth (shaders.cpp torch_shadow_vs/ps); no colour target.
std::vector<int> Renderer::Impl::drawTorchShadows(const std::vector<point_shadows::Candidate>& lights) {
    std::vector<int> picked;
    if (!shadowCollecting() || lights.empty() || sunCasters.empty() || !open) return picked;
    if (!shadowPso) {
        const D3D12_INPUT_ELEMENT_DESC layout[3] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
        p.pRootSignature = root;
        p.VS = {torchShadowVs->GetBufferPointer(), torchShadowVs->GetBufferSize()};
        p.PS = {torchShadowPs->GetBufferPointer(), torchShadowPs->GetBufferSize()};
        p.InputLayout = {layout, 3};
        p.SampleMask = UINT_MAX;
        p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        p.RasterizerState.DepthClipEnable = TRUE;
        p.DepthStencilState.DepthEnable = TRUE;
        p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        p.NumRenderTargets = 0;
        p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        p.SampleDesc.Count = 1;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&shadowPso)))) {
            LOG_ERROR("torch shadows: the pipeline failed: none");
            shadowPso = nullptr;
            shadowFailed = true;
            return picked;
        }
    }
    if (!shadowAtlas) {
        D3D12_CLEAR_VALUE clear{};
        clear.Format = DXGI_FORMAT_D32_FLOAT;
        clear.DepthStencil.Depth = 1.0f;
        shadowAtlas =
            createTexture2D(device, 6 * kF, point_shadows::kMostLights * kF, 1, DXGI_FORMAT_R32_TYPELESS,
                            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear);
        if (shadowAtlas && shadowAtlasSrv == kNoSrv) shadowAtlasSrv = allocSrv();
        if (!shadowAtlas || shadowAtlasSrv == kNoSrv) {
            LOG_ERROR("torch shadows: no video memory for their atlas: none");
            releaseCom(shadowAtlas);
            shadowFailed = true;
            return picked;
        }
        nameResource(shadowAtlas, "torch shadows' atlas");
        shadowAtlasState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = DXGI_FORMAT_R32_FLOAT;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(shadowAtlas, &sv, cpuSrvHandle(shadowAtlasSrv));
        D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = DXGI_FORMAT_D32_FLOAT;
        dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(shadowAtlas, &dv, dsvHandle(kDsvShadow));
    }
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    picked = point_shadows::pick(lights, proj.nearZ);
    if (picked.empty()) return picked;
    // Their places in the world (the casters' are)
    struct Placed {
        float at[3];
        float reach;
    };
    std::vector<Placed> placed;
    const float* m = viewToWorld;
    for (int i : picked) {
        const float* v = lights[size_t(i)].view;
        placed.push_back(Placed{{v[0] * m[0] + v[1] * m[3] + v[2] * m[6] + m[9],
                                 v[0] * m[1] + v[1] * m[4] + v[2] * m[7] + m[10],
                                 v[0] * m[2] + v[1] * m[5] + v[2] * m[8] + m[11]},
                                lights[size_t(i)].reach});
    }
    // The casters within a picked light's reach, their triangles into one upload
    struct Nearby {
        size_t caster;
        uint32_t first;
        uint32_t lights;  // a bit a picked light
    };
    std::vector<Nearby> nearby;
    size_t floats = 0;
    for (size_t k = 0; k < sunCasters.size(); ++k) {
        const SunCaster& c = sunCasters[k];
        uint32_t mask = 0;
        for (size_t s = 0; s < placed.size(); ++s)
            if (boxNear(c.lo, c.hi, placed[s].at, placed[s].reach)) mask |= 1u << s;
        if (!mask) continue;
        nearby.push_back(Nearby{k, uint32_t(floats / 6), mask});
        floats += size_t(c.count) * 6;
    }
    if (nearby.empty()) {
        picked.clear();
        return picked;
    }
    const Upload u = allocUpload(floats * sizeof(float), 16);
    if (!u.cpu) {
        picked.clear();
        return picked;
    }
    for (const Nearby& n : nearby) {
        const SunCaster& c = sunCasters[n.caster];
        std::memcpy(static_cast<uint8_t*>(u.cpu) + size_t(n.first) * 6 * sizeof(float),
                    sunVerts.data() + size_t(c.first) * 6, size_t(c.count) * 6 * sizeof(float));
    }
    StageSetup st;
    st.addressU = st.addressV = d3d7::TADDRESS_WRAP;
    st.magFilter = d3d7::TFG_LINEAR;
    st.minFilter = d3d7::TFN_LINEAR;
    st.mipFilter = d3d7::TFP_LINEAR;
    transition(shadowAtlas, shadowAtlasState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHandle(kDsvShadow);
    list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
    list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(shadowPso);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_VERTEX_BUFFER_VIEW vb{u.gpu, UINT(floats * sizeof(float)), UINT(6 * sizeof(float))};
    list->IASetVertexBuffers(0, 1, &vb);
    size_t draws = 0;
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
            const D3D12_VIEWPORT vp{float(f * kF), float(int(s) * kF), float(kF), float(kF), 0.0f, 1.0f};
            const D3D12_RECT box{LONG(f * kF), LONG(int(s) * kF), LONG((f + 1) * kF), LONG((int(s) + 1) * kF)};
            for (const Nearby& n : nearby) {
                if (!(n.lights & (1u << s))) continue;
                const SunCaster& cast = sunCasters[n.caster];
                float lo[3], hi[3];
                for (int k = 0; k < 3; ++k) {
                    lo[k] = cast.lo[k] - l.at[k];
                    hi[k] = cast.hi[k] - l.at[k];
                }
                if (!point_shadows::boxOnFace(lo, hi, f)) continue;
                if (!makeRoom(st, st, 0, 2)) return {};
                const auto tex = textures.find(cast.texture);
                UINT srv = tex != textures.end() && tex->second.resource ? tex->second.srv : kNoSrv;
                if (srv == kNoSrv && cast.alphaTest) continue;  // its holes not known
                if (srv != kNoSrv)
                    transition(tex->second.resource, tex->second.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                else
                    srv = torchDataSrv;  // read by none (footprint 0)
                // the state again after makeRoom: it may have begun another part of the ring
                list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
                list->SetGraphicsRootSignature(root);
                list->SetPipelineState(shadowPso);
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                list->IASetVertexBuffers(0, 1, &vb);
                list->RSSetViewports(1, &vp);
                list->RSSetScissorRects(1, &box);
                c.twin[1] = cast.ref;
                c.footprint = cast.alphaTest ? 1.0f : 0.0f;
                list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
                list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(srv, srv));
                list->SetGraphicsRootDescriptorTable(kRootSamplers, samplerTable(st, st, 0));
                list->DrawInstanced(cast.count, 1, n.first, 0);
                ++draws;
            }
        }
    }
    transition(shadowAtlas, shadowAtlasState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    if (!shadowLogged)
        LOG_INFO("torch shadows: %zu lights of %zu with shadows, %zu of %zu draws of the world nearby them (%zu "
                 "triangles), %zu draws into their faces ([effects] torch_shadows)",
                 placed.size(), lights.size(), nearby.size(), sunCasters.size(), floats / 18, draws);
    shadowLogged = true;
    return picked;
}

}  // namespace uagfx::gpu
