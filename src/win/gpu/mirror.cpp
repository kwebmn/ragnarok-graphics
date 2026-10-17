#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

// 1.16: the mirror's pipeline - the casters' triangles (x y z, u v, the game's colour) turned over the water and
// drawn with their textures, both faces (turned over, their winding is too), with a depth of their own.
ID3D12PipelineState* Renderer::Impl::mirrorPipeline() {
    if (mirrorPso || !mirrorVs || !mirrorPs) return mirrorPso;
    const D3D12_INPUT_ELEMENT_DESC layout[3] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {mirrorVs->GetBufferPointer(), mirrorVs->GetBufferSize()};
    p.PS = {mirrorPs->GetBufferPointer(), mirrorPs->GetBufferSize()};
    p.InputLayout = {layout, 3};
    p.SampleMask = UINT_MAX;
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.DepthStencilState.DepthEnable = TRUE;
    p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    p.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&mirrorPso)))) {
        LOG_ERROR("world reflections: the pipeline failed: none");
        mirrorPso = nullptr;
        mirrorFailed = true;
    }
    return mirrorPso;
}

// 1.16, the world's reflections in the water: before the frame's first water the last frame's opaque world (its
// houses, rocks, trees, cliffs - the sun map's casters) turned over the water's level and drawn by the game's camera
// at half the scene's size; the water (water_ps) takes it where the picture itself has nothing to reflect - what is
// above the screen's top, the far sides of things.
bool Renderer::Impl::applyMirror() {
    mirrorDone = true;
    crumb("world reflections");
    if (!mirrorPipeline() || !viewToWorldValid || sceneWidth <= 0 || sceneHeight <= 0) return true;
    const int w = std::max(1, sceneWidth / 2), h = std::max(1, sceneHeight / 2);
    if (!mirror || mirrorWidth != w || mirrorHeight != h) {
        if (mirror) slots[slot].garbage.push_back(mirror);
        if (mirrorDepth) slots[slot].garbage.push_back(mirrorDepth);
        mirror = mirrorDepth = nullptr;
        D3D12_CLEAR_VALUE clearColour{};
        clearColour.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        D3D12_CLEAR_VALUE clearDepth{};
        clearDepth.Format = DXGI_FORMAT_D32_FLOAT;
        clearDepth.DepthStencil.Depth = 1.0f;
        mirror = createTexture2D(device, w, h, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                 D3D12_RESOURCE_STATE_RENDER_TARGET, &clearColour);
        mirrorDepth = createTexture2D(device, w, h, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                                      D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearDepth);
        if (mirrorSrv == kNoSrv && mirror) mirrorSrv = allocSrv();
        if (!mirror || !mirrorDepth || mirrorSrv == kNoSrv) {
            LOG_ERROR("world reflections: no video memory for the mirror: none");
            releaseCom(mirror);
            releaseCom(mirrorDepth);
            mirrorFailed = true;
            return true;
        }
        mirrorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        mirrorDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        mirrorWidth = w;
        mirrorHeight = h;
        device->CreateShaderResourceView(mirror, nullptr, cpuSrvHandle(mirrorSrv));
        device->CreateRenderTargetView(mirror, nullptr, rtvHandle(kRtvMirror));
        D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = DXGI_FORMAT_D32_FLOAT;
        dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(mirrorDepth, &dv, dsvHandle(kDsvMirror));
    }
    // The world into the view: the inverse of viewToWorld (its rows the view's axes in the world, then its place).
    const float* m = viewToWorld;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], hh = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * hh) + c3 * (d * i - e * hh);
    if (std::fabs(det) < 1e-8f) return true;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * hh - d * k) / det, (a * k - c3 * hh) / det, (c3 * d - a * g) / det,
                          (d * i - e * hh) / det, (b * hh - a * i) / det, (a * e - b * d) / det};
    float toView[12];
    for (int r = 0; r < 3; ++r) {
        toView[r * 4] = inv[r];
        toView[r * 4 + 1] = inv[3 + r];
        toView[r * 4 + 2] = inv[6 + r];
        toView[r * 4 + 3] = -(inv[r] * m[9] + inv[3 + r] * m[10] + inv[6 + r] * m[11]);
    }
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const Upload u = allocUpload(mirrorVerts.size() * sizeof(float), 16);
    if (!u.cpu) return true;
    std::memcpy(u.cpu, mirrorVerts.data(), mirrorVerts.size() * sizeof(float));
    StageSetup st;
    st.addressU = st.addressV = d3d7::TADDRESS_WRAP;
    st.magFilter = d3d7::TFG_LINEAR;
    st.minFilter = d3d7::TFN_LINEAR;
    st.mipFilter = d3d7::TFP_LINEAR;
    transition(mirror, mirrorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    transition(mirrorDepth, mirrorDepthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    bool cleared = false;
    for (const SunCaster& cast : mirrorCasters) {
        if (!makeRoom(st, st, 0, 2)) return false;
        const auto tex = textures.find(cast.texture);
        const UINT srv = tex != textures.end() && tex->second.resource ? tex->second.srv : kNoSrv;
        if (srv == kNoSrv) continue;  // nothing to show it by
        transition(tex->second.resource, tex->second.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(kRtvMirror), dsv = dsvHandle(kDsvMirror);
        list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        if (!cleared) {
            const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            list->ClearRenderTargetView(rtv, none, 0, nullptr);
            list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            cleared = true;
        }
        const D3D12_VIEWPORT vp{0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
        const D3D12_RECT all{0, 0, w, h};
        list->RSSetViewports(1, &vp);
        list->RSSetScissorRects(1, &all);
        list->SetGraphicsRootSignature(root);
        list->SetPipelineState(mirrorPso);
        Constants c{};
        std::copy(toView, toView + 12, c.reproject);
        const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
        std::copy(pv, pv + 6, c.projection);
        c.uiPixels[0] = float(gameWidth);
        c.uiPixels[1] = float(gameHeight);
        c.twin[0] = waterLevel;
        c.twin[1] = cast.ref;
        c.footprint = cast.alphaTest ? 1.0f : 0.0f;
        list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
        list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(srv, srv));
        list->SetGraphicsRootDescriptorTable(kRootSamplers, samplerTable(st, st, 0));
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const D3D12_VERTEX_BUFFER_VIEW vb{u.gpu, UINT(mirrorVerts.size() * sizeof(float)), UINT(6 * sizeof(float))};
        list->IASetVertexBuffers(0, 1, &vb);
        list->DrawInstanced(cast.count, 1, cast.first, 0);
    }
    boundPipeline = nullptr;
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    if (!cleared) return true;  // nothing drawn: the water as before
    transition(mirror, mirrorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mirrorMade = true;
    if (!mirrorLogged)
        LOG_INFO("world reflections: %zu draws, %zu triangles of %s mirrored at %dx%d", mirrorCasters.size(),
                 mirrorVerts.size() / 18, mapName.c_str(), w, h);
    mirrorLogged = true;
    return true;
}

}  // namespace uagfx::gpu
