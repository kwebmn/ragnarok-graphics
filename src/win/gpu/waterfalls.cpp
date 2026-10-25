#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

// 0.65: the waterfalls the maps' makers placed and uaRO.exe never shows (effect 821, known up to 629: bra_fild01's
// cliffs) - with the modern water on, the map's file read and the camera known: each a sheet of water falling from its
// place, param[0] units down, along param[1] degrees, drawn over the world and hidden behind what stands nearer.
namespace uagfx::gpu {
namespace {

struct SheetVertex {  // as shadow_vs takes it: a place, uv, (how much, view depth), unused
    float pos[4];
    float uv[2];
    float az[2];
    float unused[2];
};

constexpr float kWidth = 34.0f;  // world units across - the map's sheets stand some twenty to thirty apart
constexpr int kMostSheets = 96;

map::Point viewOf(const float m[12], const map::Point& w) {  // as torches.cpp's toView
    const float dx = w.x - m[9], dy = w.y - m[10], dz = w.z - m[11];
    return map::Point{dx * m[0] + dy * m[1] + dz * m[2], dx * m[3] + dy * m[4] + dz * m[5],
                      dx * m[6] + dy * m[7] + dz * m[8]};
}

}  // namespace

ID3D12PipelineState* Renderer::Impl::waterfallPipeline(UINT n) {
    const UINT code = n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0;
    if (waterfallPipelines[code] || !waterfallPs || !shadowVs) return waterfallPipelines[code];
    const D3D12_INPUT_ELEMENT_DESC layout[4] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p =
        fullScreenPipeline(root, shadowVs, waterfallPs, DXGI_FORMAT_B8G8R8A8_UNORM, 1u << code);
    p.InputLayout = {layout, 4};
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&waterfallPipelines[code])))) {
        LOG_ERROR("waterfalls: the pipeline onto %u samples failed: none", 1u << code);
        waterfallPipelines[code] = nullptr;
        releaseCom(waterfallPs);  // not tried again
    }
    return waterfallPipelines[code];
}

bool Renderer::Impl::applyWaterfalls() {
    waterfallsDone = true;
    const rsw::World* layout = viewToWorldValid && mapWorld.axes().known() ? mapWorld.world() : nullptr;
    if (!layout) return true;
    crumb("waterfalls");  // 3.0.5-beta.8: a span of their own
    const map::Axes axes = mapWorld.axes();
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float k = float(upscale ? worldScale : scale);
    const float jx = upscale ? jitter[0] : 0.0f, jy = upscale ? jitter[1] : 0.0f;
    const int w = sceneWidth, h = sceneHeight;
    std::vector<SheetVertex> vertices;
    for (const rsw::Effect& e : layout->effects) {
        if (e.id != 821 || vertices.size() >= size_t(kMostSheets) * 6) continue;
        const float drop = std::clamp(e.param[0], 10.0f, 200.0f);
        const float a = e.param[1] * 3.14159265f / 180.0f;
        const float ax = std::cos(a) * kWidth * 0.5f, az = std::sin(a) * kWidth * 0.5f;
        // Its four corners in the file's coordinates (y down), into the game's world and the view.
        const map::Point corner[4] = {{e.pos[0] - ax, e.pos[1], e.pos[2] - az},
                                      {e.pos[0] + ax, e.pos[1], e.pos[2] + az},
                                      {e.pos[0] - ax, e.pos[1] + drop, e.pos[2] - az},
                                      {e.pos[0] + ax, e.pos[1] + drop, e.pos[2] + az}};
        SheetVertex q[4];
        bool seen = true;
        for (int c = 0; c < 4 && seen; ++c) {
            const map::Point v = viewOf(viewToWorld, map::toWorld(axes, corner[c]));
            if (v.z <= proj.nearZ) {
                seen = false;
                break;
            }
            const float gx = v.x / v.z * proj.f0 + proj.cx, gy = v.y / v.z * proj.f1 + proj.cy;
            q[c].pos[0] = (gx * k + 0.5f + jx) * 2.0f / float(w) - 1.0f;
            q[c].pos[1] = 1.0f - (gy * k + 0.5f + jy) * 2.0f / float(h);
            q[c].pos[2] = 0.0f;
            q[c].pos[3] = 1.0f;
            q[c].uv[0] = (c & 1) ? 1.0f : 0.0f;
            q[c].uv[1] = (c & 2) ? 1.0f : 0.0f;
            q[c].az[0] = 1.0f;
            q[c].az[1] = v.z;
            q[c].unused[0] = q[c].unused[1] = 0.0f;
        }
        if (!seen) continue;
        bool onScreen = false;  // any corner near the screen
        for (const SheetVertex& s : q)
            onScreen = onScreen || (std::fabs(s.pos[0]) < 1.5f && std::fabs(s.pos[1]) < 1.5f);
        if (!onScreen) continue;
        static const int kOrder[6] = {0, 1, 2, 2, 1, 3};
        for (int i : kOrder) vertices.push_back(q[i]);
    }
    if (vertices.empty()) return true;
    ID3D12PipelineState* pso = waterfallPipeline(upscale ? 1 : samples);
    if (!pso || !ensureScene("waterfalls")) return true;
    if (!copySceneDepth()) return false;
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    if (!makeRoom(linearClamp, linearClamp, 0, 5)) return false;
    const Upload up = allocUpload(vertices.size() * sizeof(SheetVertex), 16);
    if (!up.cpu) return true;
    std::memcpy(up.cpu, vertices.data(), vertices.size() * sizeof(SheetVertex));
    ID3D12Resource* dst = upscale ? world : target;
    D3D12_RESOURCE_STATES& dstState = upscale ? worldState : targetState;
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(upscale ? kRtvWorld : kRtvTarget);
    transition(dst, dstState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    placeSamples(false);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    const D3D12_RECT whole{0, 0, w, h};
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &whole);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(pso);
    Constants c{};
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    c.scale = k;
    c.targetSize[0] = float(w);
    c.targetSize[1] = float(h);
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.sharpen = waterTime();
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplerTable(linearClamp, linearClamp, 0));
    list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(sceneColorSrv, sceneColorSrv));
    list->SetGraphicsRootDescriptorTable(kRootScene, sceneTable(sceneColorSrv, sceneDepthSrv, sceneColorSrv));
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_VERTEX_BUFFER_VIEW vb{up.gpu, UINT(vertices.size() * sizeof(SheetVertex)), UINT(sizeof(SheetVertex))};
    list->IASetVertexBuffers(0, 1, &vb);
    list->DrawInstanced(UINT(vertices.size()), 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    if (!waterfallsLogged) LOG_INFO("waterfalls: the map file's drawn (%u in view)", unsigned(vertices.size() / 6));
    waterfallsLogged = true;
    return true;
}

}  // namespace uagfx::gpu
