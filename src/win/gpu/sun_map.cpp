#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/daylight.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr int kSunMap = 2048;          // the map's side, texels
constexpr float kSunHalf = 75.0f;      // half the box across, world units: about the screen's ground
constexpr float kSunDepth = 400.0f;    // the box along the light
constexpr size_t kMostSunVerts = 600000;

}  // namespace

// 1.12: an opaque draw of the world (not the interface, not blended: the map, its models, the crowns): its
// triangles in the world with their texture coordinates, to be drawn into the sun's map.
void Renderer::Impl::noteCaster(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    if (sunVerts.size() / 5 >= kMostSunVerts || (d.primitive != d3d7::PT_TRIANGLELIST &&
                                                 d.primitive != d3d7::PT_TRIANGLESTRIP))
        return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) return;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const std::vector<uint16_t>& ix = rec.indices();
    const uint32_t n = d.indexCount ? d.indexCount : d.vertexCount;
    const bool strip = d.primitive == d3d7::PT_TRIANGLESTRIP;
    const uint32_t triangles = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
    SunCaster c;
    c.first = uint32_t(sunVerts.size() / 5);
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
        float v[8];
        std::memcpy(v, vb.data() + d.vertexOffset + size_t(at) * stride, sizeof(float) * 4);
        std::memcpy(v + 4, vb.data() + d.vertexOffset + size_t(at) * stride + 24, sizeof(float) * 2);
        if (v[3] <= 0.0f) return false;
        const float vz = 1.0f / v[3], vx = (v[0] - p.cx) * vz / p.f0, vy = (v[1] - p.cy) * vz / p.f1;
        const float out[5] = {vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                              vx * m[2] + vy * m[5] + vz * m[8] + m[11], v[4], v[5]};
        sunVerts.insert(sunVerts.end(), out, out + 5);
        return true;
    };
    for (uint32_t t = 0; t < triangles; ++t) {
        const uint32_t base = strip ? t : t * 3;
        const size_t before = sunVerts.size();
        if (!put(base) || !put(base + 1) || !put(base + 2)) sunVerts.resize(before);
    }
    c.count = uint32_t(sunVerts.size() / 5) - c.first;
    if (c.count) sunCasters.push_back(c);
}

// The sun's map's pipeline: the world's triangles into a depth map, the leaves' holes let through.
ID3D12PipelineState* Renderer::Impl::sunMapPipeline() {
    if (sunMapPso || !sunMapVs || !sunMapPs) return sunMapPso;
    const D3D12_INPUT_ELEMENT_DESC layout[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {sunMapVs->GetBufferPointer(), sunMapVs->GetBufferSize()};
    p.PS = {sunMapPs->GetBufferPointer(), sunMapPs->GetBufferSize()};
    p.InputLayout = {layout, 2};
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.RasterizerState.DepthBias = 600;
    p.RasterizerState.SlopeScaledDepthBias = 2.0f;
    p.DepthStencilState.DepthEnable = TRUE;
    p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 0;
    p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    p.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&sunMapPso)))) {
        LOG_ERROR("real sun shadows: the map's pipeline failed: none");
        sunMapPso = nullptr;
        releaseCom(sunMapPs);
    }
    return sunMapPso;
}

// 1.12: the real sun's shadows - the frame's opaque world drawn from the sun into its map (the box about the player),
// then each pixel of the world darkened where something nearer the sun hides it (sun_apply_ps), before the
// characters. The screen's own sun shadows (0.77) give way to them.
bool Renderer::Impl::applyRealShadows() {
    sunMapDone = true;
    crumb("real sun shadows");
    if (sunCasters.empty() || !cameraValid || cameraTargetZ <= 0.0f) return true;
    if (!ensureScene("real sun shadows") || !sunMapPipeline()) return true;
    ID3D12PipelineState* apply = mulPipeline(sunApplyPs, sunApplyPipelines, upscale ? 1 : samples, "real sun shadows");
    if (!apply) return true;
    if (!sunMap) {
        D3D12_CLEAR_VALUE deep{};
        deep.Format = DXGI_FORMAT_D32_FLOAT;
        deep.DepthStencil.Depth = 1.0f;
        sunMap = createTexture2D(device, kSunMap, kSunMap, 1, DXGI_FORMAT_R32_TYPELESS,
                                 D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &deep);
        sunMapSrv = sunMap ? allocSrv() : kNoSrv;
        if (!sunMap || sunMapSrv == kNoSrv) {
            LOG_ERROR("real sun shadows: no video memory for the map: none");
            releaseCom(sunMap);
            releaseCom(sunApplyPs);
            return true;
        }
        sunMapState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = DXGI_FORMAT_D32_FLOAT;
        dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(sunMap, &dv, dsvHandle(kDsvSun));
        D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = DXGI_FORMAT_R32_FLOAT;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(sunMap, &sv, cpuSrvHandle(sunMapSrv));
    }
    // The sun's box: about the player, its axes the light's way (fwd) and two across it.
    const float* m = viewToWorld;
    const float tz = cameraTargetZ;
    const float mid[3] = {tz * m[6] + m[9], tz * m[7] + m[10], tz * m[8] + m[11]};
    float sun[2];
    sunOnScreen(sun);
    const daylight::Light day = daylight::at(effects.dayCycle ? effects.dayNow : -1);
    const float height = std::clamp(50.0f / day.length * 0.55f, 15.0f, 70.0f) * 0.0174533f;
    float gdir[3];
    for (int q = 0; q < 3; ++q) gdir[q] = sun[0] * m[q] + sun[1] * m[3 + q];
    const float gl = std::max(std::sqrt(gdir[0] * gdir[0] + gdir[2] * gdir[2]), 1e-4f);
    const float fwd[3] = {gdir[0] / gl * std::cos(height), std::sin(height), gdir[2] / gl * std::cos(height)};  // down
    const float upw[3] = {0.0f, -1.0f, 0.0f};
    float right[3] = {fwd[1] * upw[2] - fwd[2] * upw[1], fwd[2] * upw[0] - fwd[0] * upw[2],
                      fwd[0] * upw[1] - fwd[1] * upw[0]};
    const float rl = std::max(std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]), 1e-4f);
    for (float& r : right) r /= rl;
    const float up[3] = {right[1] * fwd[2] - right[2] * fwd[1], right[2] * fwd[0] - right[0] * fwd[2],
                         right[0] * fwd[1] - right[1] * fwd[0]};
    // the world into the box: rows (x, y, z) with their offsets
    float light[12];
    const auto row = [&](const float axis[3], float scaleBy, float add, float* out) {
        out[0] = axis[0] * scaleBy;
        out[1] = axis[1] * scaleBy;
        out[2] = axis[2] * scaleBy;
        out[3] = -(axis[0] * mid[0] + axis[1] * mid[1] + axis[2] * mid[2]) * scaleBy + add;
    };
    row(right, 1.0f / kSunHalf, 0.0f, light);
    row(up, 1.0f / kSunHalf, 0.0f, light + 4);
    row(fwd, 1.0f / kSunDepth, 0.5f, light + 8);
    // the map: the casters' vertices up once, each draw its texture (its holes)
    const Upload u = allocUpload(sunVerts.size() * sizeof(float), 16);
    if (!u.cpu) return true;
    std::memcpy(u.cpu, sunVerts.data(), sunVerts.size() * sizeof(float));
    StageSetup st;
    st.addressU = st.addressV = d3d7::TADDRESS_WRAP;
    st.magFilter = d3d7::TFG_POINT;
    st.minFilter = d3d7::TFN_POINT;
    st.mipFilter = d3d7::TFP_NONE;
    transition(sunMap, sunMapState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    bool cleared = false;
    for (const SunCaster& c : sunCasters) {
        if (!makeRoom(st, st, 0, 2)) return false;
        const auto tex = textures.find(c.texture);
        const UINT srv = tex != textures.end() && tex->second.resource ? tex->second.srv : kNoSrv;
        if (srv != kNoSrv && tex->second.resource)
            transition(tex->second.resource, tex->second.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHandle(kDsvSun);
        list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        if (!cleared) {
            list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            cleared = true;
        }
        const D3D12_VIEWPORT vp{0.0f, 0.0f, float(kSunMap), float(kSunMap), 0.0f, 1.0f};
        const D3D12_RECT all{0, 0, kSunMap, kSunMap};
        list->RSSetViewports(1, &vp);
        list->RSSetScissorRects(1, &all);
        list->SetGraphicsRootSignature(root);
        list->SetPipelineState(sunMapPso);
        Constants k{};
        std::copy(light, light + 12, k.reproject);
        k.footprint = c.alphaTest && srv != kNoSrv ? 1.0f : 0.0f;
        k.twin[0] = c.ref;
        list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &k, 0);
        const UINT use = srv != kNoSrv ? srv : 0;
        list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(use, use));
        list->SetGraphicsRootDescriptorTable(kRootSamplers, samplerTable(st, st, 0));
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const D3D12_VERTEX_BUFFER_VIEW vb{u.gpu, UINT(sunVerts.size() * sizeof(float)), UINT(5 * sizeof(float))};
        list->IASetVertexBuffers(0, 1, &vb);
        list->DrawInstanced(c.count, 1, c.first, 0);
    }
    boundPipeline = nullptr;
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    transition(sunMap, sunMapState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    // applied: the view into the box (the box's rows times the view's own into the world)
    if (!copySceneDepth()) return false;
    Constants c{};
    for (int r = 0; r < 3; ++r)
        for (int col = 0; col < 3; ++col)
            c.reproject[r * 4 + col] = light[r * 4] * m[col * 3] + light[r * 4 + 1] * m[col * 3 + 1] +
                                       light[r * 4 + 2] * m[col * 3 + 2];
    for (int r = 0; r < 3; ++r)
        c.reproject[r * 4 + 3] = light[r * 4] * m[9] + light[r * 4 + 1] * m[10] + light[r * 4 + 2] * m[11] +
                                 light[r * 4 + 3];
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    std::copy(c.reproject, c.reproject + 12, sunViewToBox);  // 1.13: for the fog
    sunMapMade = true;
    const int w = sceneWidth, h = sceneHeight;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.twin[1] = float(kSunMap);
    c.twin[2] = 0.0015f;  // the bias along the light
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    c.sharpen = 0.9f * float(std::clamp(effects.sunShadowsStrength, 0, 100)) / 100.0f * (1.0f - night);
    if (!sunMapLogged)
        LOG_INFO("real sun shadows: %zu draws, %zu triangles of %s into a %dx%d map", sunCasters.size(),
                 sunVerts.size() / 15, mapName.c_str(), kSunMap, kSunMap);
    sunMapLogged = true;
    return upscale ? effectPass(apply, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, true, sunMapSrv)
                   : effectPass(apply, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, true,
                                sunMapSrv);
}

// 1.13: volumetric fog lying low over the fields, lit by the sun where its map says the light gets through (rays
// through the crowns and past the houses), blended over the world once it is drawn - the characters in it too.
bool Renderer::Impl::applyFog() {
    fogLightDone = true;
    crumb("volumetric fog");
    if (!ensureScene("volumetric fog")) return true;
    ID3D12PipelineState* pso = alphaPipeline(fogLightPs, fogLightPipelines, upscale ? 1 : samples, "volumetric fog");
    if (!pso) return true;
    if (!copySceneDepth()) return false;
    if (sunMapMade) transition(sunMap, sunMapState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const float* m = viewToWorld;
    Constants c{};
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    const int w = sceneWidth, h = sceneHeight;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    const float worldX[4] = {m[0], m[3], m[6], m[9]}, worldY[4] = {m[1], m[4], m[7], m[10]},
                worldZ[4] = {m[2], m[5], m[8], m[11]};
    std::copy(worldX, worldX + 4, c.sourceClamp);
    std::copy(worldY, worldY + 4, c.sourceRect);
    std::copy(worldZ, worldZ + 4, c.fogColor);
    const float tz = cameraTargetZ;
    c.twin[0] = tz * m[7] + m[10];  // the ground's height at the player
    c.twin[1] = 5.0f;              // the fog's depth over it
    c.twin[2] = 0.02f * float(std::clamp(effects.volumetricFogDensity, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    c.uiPixels[2] = 0.9f * (1.0f - night);
    c.uiPixels[3] = float(kSunMap);
    c.footprint = sunMapMade ? 1.0f : 0.0f;
    if (sunMapMade) std::copy(sunViewToBox, sunViewToBox + 12, c.reproject);
    c.sharpen = waterTime();
    if (!fogLightLogged)
        LOG_INFO("volumetric fog: over %s%s", mapName.c_str(), sunMapMade ? ", lit by the sun's map" : "");
    fogLightLogged = true;
    const UINT map = sunMapMade ? sunMapSrv : sceneColorSrv;
    return upscale ? effectPass(pso, world, worldState, rtvHandle(kRtvWorld), w, h, c, sceneColorSrv, true, map)
                   : effectPass(pso, target, targetState, rtvHandle(kRtvTarget), w, h, c, sceneColorSrv, true, map);
}

}  // namespace uagfx::gpu
