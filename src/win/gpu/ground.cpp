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
namespace {

// The game's two-texture vertex (FVF_TL2): its screen place, colours and both texture coordinates.
struct Tl2 {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u0, v0, u1, v1;
};
static_assert(sizeof(Tl2) == 40, "the game's FVF_TL2 vertex");

constexpr size_t kMostGround = 60000;  // triangles a frame

}  // namespace

// 1.9: a draw of the ground (two texture coordinates, writing depth): its triangles back into the world, for the
// seams (applySeams), with its state and textures as a group.
void Renderer::Impl::noteGround(const FrameRecord& rec, const DrawCmd& d) {
    if (groundTris.size() >= kMostGround) return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    if (d.vertexOffset + size_t(d.vertexCount) * sizeof(Tl2) > vb.size()) return;
    const Tl2* v = reinterpret_cast<const Tl2*>(vb.data() + d.vertexOffset);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const auto world = [&](const Tl2& t, seams::Vertex& out) {
        if (t.rhw <= 0.0f) return false;
        const float vz = 1.0f / t.rhw, vx = (t.x - p.cx) * vz / p.f0, vy = (t.y - p.cy) * vz / p.f1;
        out.pos[0] = vx * m[0] + vy * m[3] + vz * m[6] + m[9];
        out.pos[1] = vx * m[1] + vy * m[4] + vz * m[7] + m[10];
        out.pos[2] = vx * m[2] + vy * m[5] + vz * m[8] + m[11];
        out.diffuse = t.diffuse;
        out.specular = t.specular;
        out.uv0[0] = t.u0;
        out.uv0[1] = t.v0;
        out.uv1[0] = t.u1;
        out.uv1[1] = t.v1;
        return true;
    };
    const uint32_t group = uint32_t(groundGroups.size());
    GroundGroup g;
    g.state = rec.states()[d.state];
    g.texture[0] = d.texture[0];
    g.texture[1] = d.texture[1];
    g.keyed[0] = d.keyed[0];
    g.keyed[1] = d.keyed[1];
    groundGroups.push_back(g);
    groundTextures.push_back(d.texture[0]);
    const std::vector<uint16_t>& ix = rec.indices();
    const uint32_t n = d.indexCount ? d.indexCount : d.vertexCount;
    const auto vertexAt = [&](uint32_t k, uint32_t& out) {
        if (!d.indexCount) {
            out = k;
            return k < d.vertexCount;
        }
        if (d.firstIndex + k >= ix.size()) return false;
        out = ix[d.firstIndex + k];
        return out < d.vertexCount;
    };
    const bool strip = d.primitive == d3d7::PT_TRIANGLESTRIP;
    const uint32_t triangles = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
    for (uint32_t t = 0; t < triangles && groundTris.size() < kMostGround; ++t) {
        uint32_t a = 0, b = 0, c = 0;
        const uint32_t base = strip ? t : t * 3;
        if (!vertexAt(base, a) || !vertexAt(base + 1, b) || !vertexAt(base + 2, c)) break;
        seams::Triangle tri;
        tri.group = group;
        if (!world(v[a], tri.v[0]) || !world(v[b], tri.v[1]) || !world(v[c], tri.v[2])) continue;
        groundTris.push_back(tri);
    }
}

// 1.9: the strips across the ground's seams (core/seams.hpp), drawn as the game's ground is - its pipeline, its
// textures, the AI's pictures, the materials - blended over it by the noise (shade(), bit 31), before the soft
// shadows and the grass. A record of their own: the game's geometry goes up again with its next draw.
bool Renderer::Impl::applySeams() {
    seamsDone = true;
    crumb("ground seams");
    // 2.5.11: built again only when the ground differs from the last frame's (the camera moved)
    // 2.7.57: none round the patches the map's makers laid by hand (its .gnd, MapWorld::laidAt)
    const std::vector<seams::Strip>& strips =
        seamsCache.build(groundTris, groundTextures, 0.5f,
                         [this](float x, float z) { return mapWorld.laidAt(x, z); }, mapWorld.laidKey());
    if (strips.empty()) return true;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return true;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    seamRec.reset();
    // 1.9.5: the strips in batches - one draw for each state, lightmap and texture shown (1.9.0-1.9.4 drew each kind
    // of strip apart: 600 and more draws a frame took moc_fild01 to 35 frames a second)
    struct Batch {
        const GroundGroup* in;
        const GroundGroup* shows;
        std::vector<Tl2> verts;
        std::vector<uint16_t> indices;
        bool own;  // 2.7.8: the cells' own bands (the debug view's colour)
    };
    std::vector<Batch> batches;
    // 1.25: the cells' own bands first (their texture over their rim, whole at the edge), then the neighbours' strips
    // over them - the other way the rim showed through
    std::vector<const seams::Strip*> ordered;
    for (const seams::Strip& s : strips)
        if (s.inside == s.shows) ordered.push_back(&s);
    const size_t ownBands = ordered.size();
    for (const seams::Strip& s : strips)
        if (s.inside != s.shows) ordered.push_back(&s);
    size_t firstOther = 0;  // the batches from here on hold the neighbours' strips
    for (size_t at = 0; at < ordered.size(); ++at) {
        const seams::Strip& s = *ordered[at];
        if (at == ownBands) firstOther = batches.size();
        if (s.inside >= groundGroups.size() || s.shows >= groundGroups.size()) continue;
        const GroundGroup& in = groundGroups[s.inside];
        const GroundGroup& shows = groundGroups[s.shows];
        Batch* into = nullptr;
        for (size_t k = at < ownBands ? 0 : firstOther; k < batches.size(); ++k) {
            Batch& bt = batches[k];
            if (bt.in->texture[1] == in.texture[1] && bt.in->keyed[1] == in.keyed[1] &&
                bt.shows->texture[0] == shows.texture[0] && bt.shows->keyed[0] == shows.keyed[0] &&
                bt.in->state == in.state && bt.verts.size() + s.vertices.size() <= 60000) {
                into = &bt;
                break;
            }
        }
        if (!into) {
            batches.push_back(Batch{&in, &shows, {}, {}, at < ownBands});
            into = &batches.back();
        }
        const uint16_t base = uint16_t(into->verts.size());
        bool behind = false;
        std::vector<Tl2> verts;
        for (const seams::Vertex& v : s.vertices) {
            const float x = v.pos[0] - m[9], y = v.pos[1] - m[10], z = v.pos[2] - m[11];
            const float vx = x * inv[0] + y * inv[3] + z * inv[6], vy = x * inv[1] + y * inv[4] + z * inv[7];
            const float vz = x * inv[2] + y * inv[5] + z * inv[8];
            if (vz <= p.nearZ) behind = true;
            Tl2 t{};
            const float zv = std::max(vz, p.nearZ + 0.01f);
            t.x = vx / zv * p.f0 + p.cx;
            t.y = vy / zv * p.f1 + p.cy;
            // the game's depth, a little towards the camera: the strip lies on its cell's surface, not under it
            // (2.1.2: 0.15 - where the strip's grid crosses the cell's diagonal it runs a hair under the ground)
            const float zd = std::max(zv - 0.15f, p.nearZ + 0.005f);
            t.z = p.farZ * (1.0f - p.nearZ / zd) / (p.farZ - p.nearZ);
            t.rhw = 1.0f / zv;
            t.diffuse = v.diffuse;
            t.specular = v.specular;
            t.u0 = v.uv0[0];
            t.v0 = v.uv0[1];
            t.u1 = v.uv1[0];
            t.v1 = v.uv1[1];
            verts.push_back(t);
        }
        if (behind || verts.empty()) continue;
        into->verts.insert(into->verts.end(), verts.begin(), verts.end());
        for (uint16_t k : s.indices) into->indices.push_back(uint16_t(base + k));
    }
    size_t drawn = 0;
    std::vector<bool> owns;  // 2.7.8: of each draw
    for (const Batch& bt : batches) {
        if (bt.verts.empty()) continue;
        DrawSetup st = bt.in->state;
        st.blend = true;
        st.srcBlend = d3d7::BLEND_SRCALPHA;
        st.dstBlend = d3d7::BLEND_INVSRCALPHA;
        st.zWrite = false;
        st.zFunc = seamsView == 2 ? d3d7::CMP_ALWAYS : d3d7::CMP_LESSEQUAL;  // 2.7.8: the debug view over everything
        st.cull = d3d7::CULL_NONE;
        st.alphaTest = false;
        DrawCmd cmd;
        cmd.state = seamRec.addState(st);
        cmd.texture[0] = bt.shows->texture[0];
        cmd.keyed[0] = bt.shows->keyed[0];
        cmd.texture[1] = bt.in->texture[1];
        cmd.keyed[1] = bt.in->keyed[1];
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL2;
        seamRec.draw(cmd, bt.verts.data(), uint32_t(bt.verts.size()), bt.indices.data(), uint32_t(bt.indices.size()));
        owns.push_back(bt.own);
        ++drawn;
    }
    if (!drawn) return true;
    flushDraws();  // 2.5: the game's draws gathered so far are of its own vertices, uploaded before the strips'
    prepareGeometry(seamRec);
    if (vertexBase) {
        seamPass = true;
        for (size_t k = 0; k < seamRec.draws().size(); ++k) {
            seamOwnBand = k < owns.size() && owns[k];
            draw(seamRec, seamRec.draws()[k]);
        }
        seamPass = false;
    }
    geometryList = 0;  // the game's record's vertices go up again with its next draw
    if (!seamsLogged)
        LOG_INFO("ground seams: %zu kinds of strips in %zu draws over %zu triangles of %s's ground", strips.size(),
                 drawn, groundTris.size(), mapName.c_str());
    seamsLogged = true;
    return !lost;
}

// 1.15, smooth hills: the grid of the last frame's ground (core/hills.hpp, made as the frame began) put up into its
// texture once a frame, at the first ground draw that wants it (in the draw's list, before it records).
bool Renderer::Impl::ensureHills() {
    if (hillsTried) return hillsUp;
    hillsTried = true;
    if (hillsFailed) return false;
    const hills::Grid& g = hillsGrid;
    if (g.side <= 0.0f || g.width <= 0 || g.height <= 0 || g.packed.size() < size_t(g.width) * size_t(g.height))
        return false;
    if (!hillsData) {
        hillsData = createTexture2D(device, hills::kMostSide, hills::kMostSide, 1, DXGI_FORMAT_R32_FLOAT,
                                    D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        hillsDataSrv = hillsData ? allocSrv() : kNoSrv;
        if (!hillsData || hillsDataSrv == kNoSrv) {
            LOG_ERROR("smooth hills: no memory for the ground's normals: none");
            releaseCom(hillsData);
            hillsFailed = true;  // not tried again
            return false;
        }
        hillsDataState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(hillsData, nullptr, cpuSrvHandle(hillsDataSrv));
    }
    const UINT pitch = UINT(hills::kMostSide) * sizeof(float);  // 1024: a multiple of 256
    const Upload u = allocUpload(size_t(pitch) * size_t(g.height), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!u.cpu) return false;
    for (int row = 0; row < g.height; ++row)
        std::memcpy(static_cast<uint8_t*>(u.cpu) + size_t(row) * pitch, g.packed.data() + size_t(row) * size_t(g.width),
                    size_t(g.width) * sizeof(float));
    D3D12_TEXTURE_COPY_LOCATION src{}, to{};
    src.pResource = slots[slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
    src.PlacedFootprint.Footprint.Width = UINT(g.width);
    src.PlacedFootprint.Footprint.Height = UINT(g.height);
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = pitch;
    to.pResource = hillsData;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    transition(hillsData, hillsDataState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    transition(hillsData, hillsDataState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    hillsUp = true;
    if (!hillsLogged)
        LOG_INFO("smooth hills: %dx%d corners of %s's ground, %.1f apart", g.width, g.height, mapName.c_str(),
                 double(g.side));
    hillsLogged = true;
    return true;
}

// 2.2.1: the grass's ground (core/ground_cells.hpp, made as the frame began from the last frame's) into its texture
// once a frame, in the grass's pass's list before it records.
bool Renderer::Impl::ensureGroundCells() {
    if (groundCellsTried) return groundCellsUp;
    groundCellsTried = true;
    if (groundCellsFailed) return false;
    const ground_cells::Grid& g = groundCells;
    if (g.side <= 0.0f || g.width <= 0 || g.height <= 0 || g.span.size() < size_t(g.width) * size_t(g.height) * 2)
        return false;
    if (!groundCellsData) {
        groundCellsData = createTexture2D(device, ground_cells::kMostSide, ground_cells::kMostSide, 1,
                                          DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_FLAG_NONE,
                                          D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
        groundCellsSrv = groundCellsData ? allocSrv() : kNoSrv;
        if (!groundCellsData || groundCellsSrv == kNoSrv) {
            LOG_ERROR("grass: no memory for the ground's heights: on every green pixel as before");
            releaseCom(groundCellsData);
            groundCellsFailed = true;  // not tried again
            return false;
        }
        groundCellsState = D3D12_RESOURCE_STATE_COPY_DEST;
        device->CreateShaderResourceView(groundCellsData, nullptr, cpuSrvHandle(groundCellsSrv));
    }
    const UINT pitch = UINT(ground_cells::kMostSide) * 2 * sizeof(float);  // 2048: a multiple of 256
    const Upload u = allocUpload(size_t(pitch) * size_t(g.height), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!u.cpu) return false;
    for (int row = 0; row < g.height; ++row)
        std::memcpy(static_cast<uint8_t*>(u.cpu) + size_t(row) * pitch,
                    g.span.data() + size_t(row) * size_t(g.width) * 2, size_t(g.width) * 2 * sizeof(float));
    D3D12_TEXTURE_COPY_LOCATION src{}, to{};
    src.pResource = slots[slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32_FLOAT;
    src.PlacedFootprint.Footprint.Width = UINT(g.width);
    src.PlacedFootprint.Footprint.Height = UINT(g.height);
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = pitch;
    to.pResource = groundCellsData;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    transition(groundCellsData, groundCellsState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyTextureRegion(&to, 0, 0, 0, &src, nullptr);
    transition(groundCellsData, groundCellsState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    groundCellsUp = true;
    if (!groundCellsLogged)
        LOG_INFO("grass: %dx%d cells of %s's ground, %.1f a side", g.width, g.height, mapName.c_str(),
                 double(g.side));
    groundCellsLogged = true;
    return true;
}

// The constants of a ground draw lit over the smooth hills (shaders.cpp hillLight): bit 26 of stage 0, the grid's
// place, the game's projection and camera, the way to the sun in the world, the strength, the light all around.
void Renderer::Impl::hillsConstants(Constants& c) const {
    const hills::Grid& g = hillsGrid;
    c.stage0[1] |= 1u << 26;
    c.sourceRect[0] = g.originX;
    c.sourceRect[1] = g.originZ;
    c.sourceRect[2] = 1.0f / g.side;
    c.sourceRect[3] = float(g.width) + 1024.0f * float(g.height);
    sunInWorld(c.sourceClamp);
    c.sourceClamp[3] = 0.0f;
    c.footprint = float(std::clamp(effects.smoothHillsStrength, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    c.sharpen = mapOutdoors ? 0.55f + 0.3f * night : 0.75f;  // the sun's light against the sky's
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
    std::memcpy(c.projection, proj, sizeof proj);
    std::memcpy(c.reproject, viewToWorld, sizeof c.reproject);
}

}  // namespace uagfx::gpu
