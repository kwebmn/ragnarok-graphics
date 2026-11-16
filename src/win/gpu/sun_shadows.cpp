#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/game_camera.hpp"
#include "core/sun_shadows.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr UINT kSunStride = 5 * sizeof(float);  // x y z, u v
constexpr int kSunStrip = 256;                  // the sunless lightmaps' rows a picture

// a buffer in the card's own memory, to be copied into
ID3D12Resource* cardBuffer(ID3D12Device* device, UINT64 bytes) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* made = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                               nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&made))))
        return nullptr;
    return made;
}

}  // namespace

// 3.0.20-beta.50 (the owner's «делай 1-9», 2 - a trial of real shadows in the 3D view): the casters on the card - the
// map's own ground (its plain cells: OwnGround::distant, two triangles a cell and the walls) and its own models
// (OwnGround::models, every one the map places - not those the game happened to draw), in the map file's places. One
// buffer of vertices (x y z, u v), one of indices: the ground's in one run, then the models' a texture a run (a
// picture's holes cast none). Put up once a map, whole.
bool Renderer::Impl::ensureSunCasters(const OwnGround& g) {
    if (sunCastersOf == g.serial) return sunVertices && sunIndices && !sunRuns.empty();
    sunCastersOf = g.serial;
    discard(sunVertices);
    discard(sunIndices);
    sunVertices = sunIndices = nullptr;
    sunRuns.clear();
    const terrain::Mesh& ground = g.distant;
    const terrain::Mesh& models = g.models;
    const size_t vertices = ground.vertices.size() + models.vertices.size();
    size_t indices = 0;
    for (const terrain::Batch& b : ground.batches) indices += b.layer == terrain::kGround ? b.indexCount : 0;
    for (const terrain::Batch& b : models.batches) indices += b.indexCount;
    if (!vertices || !indices) return false;
    sunVertexBytes = UINT64(vertices) * kSunStride;
    sunIndexBytes = UINT64(indices) * sizeof(uint32_t);
    sunVertices = cardBuffer(device, sunVertexBytes);
    sunIndices = cardBuffer(device, sunIndexBytes);
    const Upload uv = sunVertices && sunIndices ? allocUpload(sunVertexBytes, 16) : Upload{nullptr, 0, 0};
    if (!uv.cpu) {
        LOG_ERROR("sun shadows: no memory on the card for %s's casters (%zu vertices, %zu indices) - none on this map",
                  g.map.c_str(), vertices, indices);
        releaseCom(sunVertices);
        releaseCom(sunIndices);
        return false;
    }
    float* v = reinterpret_cast<float*>(uv.cpu);
    for (const terrain::Mesh* mesh : {&ground, &models})
        for (const terrain::Vertex& t : mesh->vertices) {
            const float one[5] = {t.pos[0], t.pos[1], t.pos[2], t.uv[0], t.uv[1]};
            std::memcpy(v, one, sizeof one);
            v += 5;
        }
    flushDraws();  // the draws gathered so far before the copies
    list->CopyBufferRegion(sunVertices, 0, slots[slot].upload, uv.offset, sunVertexBytes);
    const Upload ui = allocUpload(sunIndexBytes, 16);
    if (!ui.cpu) {
        releaseCom(sunVertices);
        releaseCom(sunIndices);
        return false;
    }
    uint32_t* into = reinterpret_cast<uint32_t*>(ui.cpu);
    uint32_t at = 0;
    SunRun run;  // the ground's: all of it, no picture
    for (const terrain::Batch& b : ground.batches) {
        if (b.layer != terrain::kGround) continue;
        for (uint32_t i = 0; i < b.indexCount; ++i) into[at + i] = b.firstVertex + ground.indices[b.firstIndex + i];
        at += b.indexCount;
    }
    run.count = at;
    if (run.count) sunRuns.push_back(run);
    const uint32_t base = uint32_t(ground.vertices.size());  // the models' vertices after the ground's
    int most = -1;
    for (const terrain::Batch& b : models.batches) most = std::max(most, b.texture);
    for (int texture = -1; texture <= most; ++texture) {  // (-1: a face of no picture - whole)
        SunRun r;
        r.texture = texture < 0 ? -2 : texture;
        r.first = at;
        for (const terrain::Batch& b : models.batches) {
            if (b.texture != texture) continue;
            for (uint32_t i = 0; i < b.indexCount; ++i)
                into[at + i] = base + b.firstVertex + models.indices[b.firstIndex + i];
            at += b.indexCount;
        }
        r.count = at - r.first;
        if (r.count) sunRuns.push_back(r);
    }
    list->CopyBufferRegion(sunIndices, 0, slots[slot].upload, ui.offset, sunIndexBytes);
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
    transition(sunVertices, state, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    state = D3D12_RESOURCE_STATE_COPY_DEST;
    transition(sunIndices, state, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    nameResource(sunVertices, "sun shadows' vertices");
    nameResource(sunIndices, "sun shadows' indices");
    LOG_INFO("sun shadows: %s's casters on the card - its ground's %zu vertices and its models' %zu, %u triangles in "
             "%zu runs, %.1f MB",
             g.map.c_str(), ground.vertices.size(), models.vertices.size(), at / 3, sunRuns.size(),
             double(sunVertexBytes + sunIndexBytes) / 1048576.0);
    return true;
}

// Whether the trial's shadows are drawn this picture: wanted, the own ground's pictures up, its light without the
// painted shadows up (made and put up here, a strip a picture) and the casters on the card. The own ground is drawn
// with that light when so - and the shadows must follow (sunShadowsDue).
bool Renderer::Impl::sunShadowsReady(const OwnGround& g) {
    OwnState& o = ownState;
    // (its models' pictures up too - o.up: their holes cast none)
    if (!sunShadowsWanted() || !o.up || !o.atlas || g.serial != o.serial) return false;
    if (g.sunless.tops.empty() || g.atlas.width <= 0 || g.atlas.height <= 0 ||
        g.atlas.argb.size() < size_t(g.atlas.width) * size_t(g.atlas.height)) {
        if (sunShadowsLogged != g.map + "!") {
            sunShadowsLogged = g.map + "!";
            LOG_INFO("sun shadows: %s's lightmaps are not kept (a 32-bit renderer lets them go) - none on this map",
                     g.map.c_str());
        }
        return false;
    }
    if (!o.atlasSunless) {
        o.atlasSunless = ownTexture(g.atlas.width, g.atlas.height, 1);
        o.atlasSunlessRow = 0;
        if (!o.atlasSunless) {
            LOG_ERROR("sun shadows: no memory on the card for the ground's light without the painted shadows - off");
            sunShadowsFailed = true;
            return false;
        }
    }
    if (o.atlasSunlessRow < g.atlas.height) {
        const int rows = std::min(kSunStrip, g.atlas.height - o.atlasSunlessRow);
        std::vector<uint32_t> strip(size_t(rows) * size_t(g.atlas.width));
        sun_shadows::sunlessRows(g.atlas, g.sunless, o.atlasSunlessRow, rows, strip.data());
        FrameRecord rec;
        rec.upload(o.atlasSunless, 0, Rect{0, o.atlasSunlessRow, g.atlas.width, o.atlasSunlessRow + rows},
                   strip.data());
        for (const UploadCmd& u : rec.uploads()) upload(rec, u);
        o.atlasSunlessRow += rows;
        if (o.atlasSunlessRow < g.atlas.height) return false;
    }
    return ensureSunCasters(g);
}

// The map of depths from the sun's side, then the shadows over the picture (sun_shadow_ps, multiplied in).
bool Renderer::Impl::applySunShadows() {
    sunShadowsDone = true;
    crumb("sun shadows");
    const OwnGround* g = mapWorld.ownGround();
    if (!g || !sunVertices || !sunIndices || sunRuns.empty() || !ensureScene("sun shadows")) return true;
    ID3D12PipelineState* over = mulPipeline(sunShadowPs, sunShadowPipelines, upscale ? 1 : samples, "sun shadows");
    if (!over) return true;
    if (!sunCasterPso) {
        const D3D12_INPUT_ELEMENT_DESC layout[2] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
        p.pRootSignature = root;
        p.VS = {sunCasterVs->GetBufferPointer(), sunCasterVs->GetBufferSize()};
        p.PS = {sunCasterPs->GetBufferPointer(), sunCasterPs->GetBufferSize()};
        p.InputLayout = {layout, 2};
        p.SampleMask = UINT_MAX;
        p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        p.RasterizerState.DepthClipEnable = FALSE;  // a caster nearer the sun than the box casts all the same
        p.RasterizerState.SlopeScaledDepthBias = 2.0f;
        p.DepthStencilState.DepthEnable = TRUE;
        p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        p.NumRenderTargets = 0;
        p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        p.SampleDesc.Count = 1;
        if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                       reinterpret_cast<void**>(&sunCasterPso)))) {
            LOG_ERROR("sun shadows: the casters' pipeline failed - off");
            sunCasterPso = nullptr;
            sunShadowsFailed = true;
            return true;
        }
    }
    const int side = sun_shadows::kTexels;
    if (!sunMap) {
        D3D12_CLEAR_VALUE clear{};
        clear.Format = DXGI_FORMAT_D32_FLOAT;
        clear.DepthStencil.Depth = 1.0f;
        sunMap = createTexture2D(device, side, side, 1, DXGI_FORMAT_R32_TYPELESS,
                                 D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear);
        if (sunMap && sunMapSrv == kNoSrv) sunMapSrv = allocSrv();
        if (!sunMap || sunMapSrv == kNoSrv) {
            LOG_ERROR("sun shadows: no video memory for their map (%dx%d) - off", side, side);
            releaseCom(sunMap);
            sunShadowsFailed = true;
            return true;
        }
        nameResource(sunMap, "sun shadows' map");
        sunMapState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = DXGI_FORMAT_R32_FLOAT;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(sunMap, &sv, cpuSrvHandle(sunMapSrv));
        D3D12_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = DXGI_FORMAT_D32_FLOAT;
        dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(sunMap, &dv, dsvHandle(kDsvSun));
    }
    // the box round the eye, in the map file's places (the world's by the axes' signs)
    const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
    const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
    float eye[3], ahead[3], focus[3];
    for (int k = 0; k < 3; ++k) {
        eye[k] = viewToWorld[9 + k] * sign[k];
        ahead[k] = viewToWorld[6 + k] * sign[k];
    }
    sun_shadows::focusOf(eye, ahead, sun_shadows::kHalf, focus);
    const sun_shadows::Box box =
        sun_shadows::boxAt(focus, g->toLight, sun_shadows::kHalf, sun_shadows::kDepth, side);
    Constants c{};
    for (int k = 0; k < 3; ++k) {
        c.sourceRect[k] = box.right[k];
        c.sourceClamp[k] = box.up[k];
        c.fogColor[k] = box.toSun[k];
        c.twin[k] = box.middle[k];
    }
    c.sourceRect[3] = box.half;
    c.sourceClamp[3] = box.depth;
    // the map of depths
    StageSetup st;
    st.addressU = st.addressV = d3d7::TADDRESS_WRAP;
    st.magFilter = d3d7::TFG_LINEAR;
    st.minFilter = d3d7::TFN_LINEAR;
    st.mipFilter = d3d7::TFP_LINEAR;
    flushDraws();
    transition(sunMap, sunMapState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHandle(kDsvSun);
    list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
    list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    const D3D12_VIEWPORT vp{0.0f, 0.0f, float(side), float(side), 0.0f, 1.0f};
    const D3D12_RECT all{0, 0, side, side};
    const D3D12_VERTEX_BUFFER_VIEW vb{sunVertices->GetGPUVirtualAddress(), UINT(sunVertexBytes), kSunStride};
    const D3D12_INDEX_BUFFER_VIEW ib{sunIndices->GetGPUVirtualAddress(), UINT(sunIndexBytes), DXGI_FORMAT_R32_UINT};
    const auto lit = textures.find(ownState.atlas);  // a picture for a run without one (read by none)
    size_t draws = 0, holed = 0;
    for (const SunRun& run : sunRuns) {
        UINT srv = kNoSrv;
        bool holes = false;
        if (run.texture >= 0 && size_t(run.texture) < ownState.modelIds.size()) {
            const auto tex = textures.find(ownState.modelIds[size_t(run.texture)]);
            if (tex != textures.end() && tex->second.resource) {
                transition(tex->second.resource, tex->second.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                srv = tex->second.srv;
                holes = true;
            }
        }
        if (srv == kNoSrv) {
            if (lit == textures.end() || !lit->second.resource) continue;
            transition(lit->second.resource, lit->second.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            srv = lit->second.srv;
        }
        if (!makeRoom(st, st, 0, 2)) return false;
        // the state again after makeRoom: it may have begun another part of the ring
        list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        list->SetGraphicsRootSignature(root);
        list->SetPipelineState(sunCasterPso);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        list->IASetVertexBuffers(0, 1, &vb);
        list->IASetIndexBuffer(&ib);
        list->RSSetViewports(1, &vp);
        list->RSSetScissorRects(1, &all);
        c.footprint = holes ? 1.0f : 0.0f;
        list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
        list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(srv, srv));
        list->SetGraphicsRootDescriptorTable(kRootSamplers, samplerTable(st, st, 0));
        list->DrawIndexedInstanced(run.count, 1, run.first, 0, 0);
        ++draws;
        holed += holes ? 1 : 0;
    }
    transition(sunMap, sunMapState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    pass = kPassNone;
    // the shadows over the picture
    if (!copySceneDepth()) return false;
    const int w = sceneWidth, h = sceneHeight;
    const camera::Projection proj = camera::projectionFor(gameWidth, gameHeight);
    const float pv[6] = {proj.f0, proj.f1, proj.cx, proj.cy, proj.nearZ, proj.farZ};
    std::copy(pv, pv + 6, c.projection);
    std::copy(viewToWorld, viewToWorld + 12, c.reproject);
    c.scale = float(upscale ? worldScale : scale);
    if (upscale) {
        c.jitter[0] = jitter[0];
        c.jitter[1] = jitter[1];
    }
    c.footprint = 0.0f;
    c.uiPixels[0] = float(w);
    c.uiPixels[1] = float(h);
    c.uiPixels[3] = float(side);
    c.fogColor[3] = 1.0f;       // all of the map's own shade
    c.twin[3] = g->shade;       // as dark as its makers painted theirs
    for (int k = 0; k < 3; ++k) c.hole[k] = sign[k];
    if (sunShadowsLogged != g->map) {
        sunShadowsLogged = g->map;
        LOG_INFO("sun shadows: on %s - a map of %dx%d over %.0f units round the eye (%.2f a texel), %zu draws of the "
                 "casters (%zu with their pictures' holes), the way to the sun %.2f %.2f %.2f, the shade %.2f; the "
                 "ground's painted shadows out of its light ([view3d] shadows, a trial)",
                 g->map.c_str(), side, side, double(2.0f * box.half), double(2.0f * box.half / float(side)), draws,
                 holed, double(box.toSun[0]), double(box.toSun[1]), double(box.toSun[2]), double(g->shade));
    }
    return worldPass(over, w, h, c, sunMapSrv, true, sunMapSrv);
}

}  // namespace uagfx::gpu
