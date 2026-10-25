#include <windows.h>

#include <d3d11.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "core/hills.hpp"
#include "core/own_ground.hpp"
#include "core/seams.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

// 2.5.9: the ground's part of the strong set with Direct3D 11, as Direct3D 12's ground.cpp - the frame's ground noted
// back into the world (the grass's cells, the smooth hills of the next frame, this frame's seams), the hills' normals
// put up into their texture, the strips across the cells' seams drawn as the ground is.
namespace uagfx::gpu {
namespace {

template <typename T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

// The game's two-texture vertex (FVF_TL2): its screen place, colours and both texture coordinates.
struct Tl2 {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u0, v0, u1, v1;
};
static_assert(sizeof(Tl2) == 40, "the game's FVF_TL2 vertex");

constexpr size_t kMostGround = 60000;  // triangles a frame (ground.cpp)

}  // namespace

// 2.5.4: a draw of the ground (two texture coordinates, writing depth): its triangles back into the world - 2.5.9
// with their colours, texture coordinates and the draw's state and textures as a group (ground.cpp noteGround).
void Renderer11::noteGround(const FrameRecord& rec, const DrawCmd& d) {
    if (!cameraValid_ || groundTris_.size() >= kMostGround || d.fvf != d3d7::FVF_TL2) return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    if (d.vertexOffset + size_t(d.vertexCount) * sizeof(Tl2) > vb.size()) return;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float* m = viewToWorld_;
    const auto world = [&](uint32_t k, seams::Vertex& out) {
        Tl2 t;
        std::memcpy(&t, vb.data() + d.vertexOffset + size_t(k) * sizeof(Tl2), sizeof t);
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
    const uint32_t group = uint32_t(groundGroups_.size());
    GroundGroup11 g;
    g.state = rec.states()[d.state];
    g.texture[0] = d.texture[0];
    g.texture[1] = d.texture[1];
    g.keyed[0] = d.keyed[0];
    g.keyed[1] = d.keyed[1];
    groundGroups_.push_back(g);
    groundTextures_.push_back(d.texture[0]);
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
    for (uint32_t t = 0; t < triangles && groundTris_.size() < kMostGround; ++t) {
        uint32_t a = 0, b = 0, c = 0;
        const uint32_t base = strip ? t : t * 3;
        if (!vertexAt(base, a) || !vertexAt(base + 1, b) || !vertexAt(base + 2, c)) break;
        seams::Triangle tri;
        tri.group = group;
        if (!world(a, tri.v[0]) || !world(b, tri.v[1]) || !world(c, tri.v[2])) continue;
        groundTris_.push_back(tri);
    }
}

// 2.5.9 (ground.cpp ensureHills): the grid of the last frame's ground (made as the frame began) put up into its
// texture once a frame, at the first ground draw that wants it.
bool Renderer11::ensureHills() {
    if (hillsTried_) return hillsUp_;
    hillsTried_ = true;
    if (hillsFailed_) return false;
    const hills::Grid& g = hillsGrid_;
    if (g.side <= 0.0f || g.width <= 0 || g.height <= 0 || g.packed.size() < size_t(g.width) * size_t(g.height))
        return false;
    if (!hillsData_) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = d.Height = UINT(hills::kMostSide);
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, &hillsData_)) ||
            FAILED(device_->CreateShaderResourceView(hillsData_, nullptr, &hillsSrv_))) {
            LOG_ERROR("smooth hills: no memory for the ground's normals (Direct3D 11): none");
            drop(hillsSrv_);
            drop(hillsData_);
            hillsFailed_ = true;  // not tried again
            return false;
        }
    }
    const D3D11_BOX box{0, 0, 0, UINT(g.width), UINT(g.height), 1};
    ctx_->UpdateSubresource(hillsData_, 0, &box, g.packed.data(), UINT(g.width) * sizeof(float), 0);
    hillsUp_ = true;
    if (!hillsLogged_)
        LOG_INFO("smooth hills: %dx%d corners of %s's ground, %.1f apart (Direct3D 11)", g.width, g.height,
                 map_.c_str(), double(g.side));
    hillsLogged_ = true;
    return true;
}

// The constants of a ground draw lit over the smooth hills (ground.cpp hillsConstants, shaders.cpp hillLight): bit
// 26 of stage 0, the grid's place, the game's projection and camera, the way to the sun, the strength, the light all
// around.
void Renderer11::hillsConstants(Constants& c) const {
    const hills::Grid& g = hillsGrid_;
    c.stage0[1] |= 1u << 26;
    c.sourceRect[0] = g.originX;
    c.sourceRect[1] = g.originZ;
    c.sourceRect[2] = 1.0f / g.side;
    c.sourceRect[3] = float(g.width) + 1024.0f * float(g.height);
    sunInWorld(c.sourceClamp);
    c.sourceClamp[3] = 0.0f;
    c.footprint = float(std::clamp(effects_.smoothHillsStrength, 0, 200)) / 100.0f;
    const float night = float(std::clamp(effects_.nightNow, 0, 100)) / 100.0f;
    c.sharpen = outdoors_ ? 0.55f + 0.3f * night : 0.75f;  // the sun's light against the sky's
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float proj[6] = {p.f0, p.f1, p.cx, p.cy, p.nearZ, p.farZ};
    std::memcpy(c.projection, proj, sizeof proj);
    std::memcpy(c.reproject, viewToWorld_, sizeof c.reproject);
}

// 2.5.9 (ground.cpp applySeams): the strips across the ground's seams (core/seams.hpp) in batches - one draw for each
// state, lightmap and texture shown, the cells' own bands first - drawn by draw() as the game's ground is (its
// textures, the AI's pictures, the materials, the hills), blended over it by the noise (shade(), bit 31). Their
// vertices go into buffers of their own: the game's record stays in vertices_ for its next draws.
void Renderer11::applySeams() {
    seamsDone_ = true;
    // 3.0.5-beta.13: the pieces kept on the card, the camera put on them there; the processor's way below as before
    // where the card's is missing or failed
    if (seamsOnCard() && applySeamsOnCard()) return;
    // 2.5.11: built again only when the ground differs from the last frame's (the camera moved)
    // 2.7.57 (ground.cpp): none round the patches the map's makers laid by hand
    const std::vector<seams::Strip>& strips =
        seamsCache_.build(groundTris_, groundTextures_, 0.5f,
                          [this](float x, float z) { return mapWorld_.laidAt(x, z); }, mapWorld_.laidKey());
    if (strips.empty()) return;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const float* m = viewToWorld_;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    seamRec_.reset();
    struct Batch {
        const GroundGroup11* in;
        const GroundGroup11* shows;
        std::vector<Tl2> verts;
        std::vector<uint16_t> indices;
    };
    std::vector<Batch> batches;
    std::vector<const seams::Strip*> ordered;  // the cells' own bands, then the neighbours' strips over them
    for (const seams::Strip& s : strips)
        if (s.inside == s.shows) ordered.push_back(&s);
    const size_t ownBands = ordered.size();
    for (const seams::Strip& s : strips)
        if (s.inside != s.shows) ordered.push_back(&s);
    size_t firstOther = 0;  // the batches from here on hold the neighbours' strips
    for (size_t at = 0; at < ordered.size(); ++at) {
        const seams::Strip& s = *ordered[at];
        if (at == ownBands) firstOther = batches.size();
        if (s.inside >= groundGroups_.size() || s.shows >= groundGroups_.size()) continue;
        const GroundGroup11& in = groundGroups_[s.inside];
        const GroundGroup11& shows = groundGroups_[s.shows];
        Batch* into = nullptr;
        for (size_t n = at < ownBands ? 0 : firstOther; n < batches.size(); ++n) {
            Batch& bt = batches[n];
            if (bt.in->texture[1] == in.texture[1] && bt.in->keyed[1] == in.keyed[1] &&
                bt.shows->texture[0] == shows.texture[0] && bt.shows->keyed[0] == shows.keyed[0] &&
                bt.in->state == in.state && bt.verts.size() + s.vertices.size() <= 60000) {
                into = &bt;
                break;
            }
        }
        if (!into) {
            batches.push_back(Batch{&in, &shows, {}, {}});
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
        for (uint16_t n : s.indices) into->indices.push_back(uint16_t(base + n));
    }
    size_t drawn = 0;
    for (const Batch& bt : batches) {
        if (bt.verts.empty()) continue;
        DrawSetup st = bt.in->state;
        st.blend = true;
        st.srcBlend = d3d7::BLEND_SRCALPHA;
        st.dstBlend = d3d7::BLEND_INVSRCALPHA;
        st.zWrite = false;
        st.zFunc = d3d7::CMP_LESSEQUAL;
        st.cull = d3d7::CULL_NONE;
        st.alphaTest = false;
        DrawCmd cmd;
        cmd.state = seamRec_.addState(st);
        cmd.texture[0] = bt.shows->texture[0];
        cmd.keyed[0] = bt.shows->keyed[0];
        cmd.texture[1] = bt.in->texture[1];
        cmd.keyed[1] = bt.in->keyed[1];
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL2;
        seamRec_.draw(cmd, bt.verts.data(), uint32_t(bt.verts.size()), bt.indices.data(),
                      uint32_t(bt.indices.size()));
        ++drawn;
    }
    if (!drawn) return;
    flushBatch();  // the game's draws gathered so far are of its own vertices
    const std::vector<uint8_t>& v = seamRec_.vertexBytes();
    const std::vector<uint16_t>& ix = seamRec_.indices();
    if (!fillBuffer(seamVertices_, seamVertexBytes_, D3D11_BIND_VERTEX_BUFFER, v.data(), v.size(),
                    bgraVertices_ ? nullptr : &seamRec_) ||
        !fillBuffer(seamIndices_, seamIndexBytes_, D3D11_BIND_INDEX_BUFFER, ix.data(), ix.size() * sizeof(uint16_t),
                    nullptr))
        return;
    // the strips' buffers stand in for the record's while they are drawn (flushBatch binds vertices_, indices_)
    std::swap(vertices_, seamVertices_);
    std::swap(indices_, seamIndices_);
    seamPass_ = true;
    for (const DrawCmd& cmd : seamRec_.draws()) draw(seamRec_, cmd);
    flushBatch();
    seamPass_ = false;
    std::swap(vertices_, seamVertices_);
    std::swap(indices_, seamIndices_);
    if (!seamsLogged_)
        LOG_INFO("ground seams: %zu kinds of strips in %zu draws over %zu triangles of %s's ground (Direct3D 11)",
                 strips.size(), drawn, groundTris_.size(), map_.c_str());
    seamsLogged_ = true;
}

// 3.0.5-beta.13: room on the card for `slots` slots of a size - a buffer of their vertices (three uint4 each, seam_vs's
// t6) and one of their indices (t7); grown twice at a time, the slots so far copied over on the card.
bool Renderer11::ensureSeamSlots(uint32_t size, uint32_t slots) {
    SeamSlots11& s = seamSlots_[size];
    if (s.vertices && s.slots >= slots) return true;
    uint32_t room = std::max<uint32_t>(s.slots, 1024);
    while (room < slots) room *= 2;
    const UINT vertexBytes = room * seams::kSlotVertices[size] * UINT(sizeof(seams::CardVertex));
    const UINT indexBytes = room * seams::kSlotIndices[size] * UINT(sizeof(uint16_t));
    SeamSlots11 grown;
    grown.slots = room;
    D3D11_BUFFER_DESC d{};
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SHADER_RESOURCE_VIEW_DESC v{};
    v.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    d.ByteWidth = vertexBytes;
    v.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    v.Buffer.NumElements = room * seams::kSlotVertices[size] * 3;
    HRESULT hr = device_->CreateBuffer(&d, nullptr, &grown.vertices);
    if (SUCCEEDED(hr)) hr = device_->CreateShaderResourceView(grown.vertices, &v, &grown.vertexSrv);
    d.ByteWidth = indexBytes;
    v.Format = DXGI_FORMAT_R16_UINT;
    v.Buffer.NumElements = room * seams::kSlotIndices[size];
    if (SUCCEEDED(hr)) hr = device_->CreateBuffer(&d, nullptr, &grown.indices);
    if (SUCCEEDED(hr)) hr = device_->CreateShaderResourceView(grown.indices, &v, &grown.indexSrv);
    if (FAILED(hr)) {
        LOG_ERROR("ground seams: no room on the card for %u pieces (0x%08lX) - made on the processor from now on",
                  room, static_cast<unsigned long>(hr));
        drop(grown.vertexSrv);
        drop(grown.vertices);
        drop(grown.indexSrv);
        drop(grown.indices);
        return false;
    }
    if (s.vertices) {  // the slots so far as they are
        const D3D11_BOX vb{0, 0, 0, s.slots * seams::kSlotVertices[size] * UINT(sizeof(seams::CardVertex)), 1, 1};
        const D3D11_BOX ib{0, 0, 0, s.slots * seams::kSlotIndices[size] * UINT(sizeof(uint16_t)), 1, 1};
        ctx_->CopySubresourceRegion(grown.vertices, 0, 0, 0, 0, s.vertices, 0, &vb);
        ctx_->CopySubresourceRegion(grown.indices, 0, 0, 0, 0, s.indices, 0, &ib);
    }
    drop(s.vertexSrv);
    drop(s.vertices);
    drop(s.indexSrv);
    drop(s.indices);
    s = grown;
    return true;
}

// 3.0.5-beta.13 (the user: «Швы на видеокарте»): the seams' strips from the pieces kept on the card (core/seams.hpp
// Store) - the pieces made this frame written into their slots, then each batch (applySeams': the cell's state and
// lightmap, the texture shown; the cells' own bands' batches first) drawn as instances of its pieces, each size apart
// (a cell's pieces are of one size: the order of those that overlap stays). false: the card's way failed.
bool Renderer11::applySeamsOnCard() {
    if (seamsMap_ != map_) {  // the pieces are of a map's places
        seamStore_.clear();
        seamsMap_ = map_;
        seamsLogIn_ = 300;  // the log's line once the map's ground is all in view
    }
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    own_ground::View view;
    if (!own_ground::viewOf(viewToWorld_, view)) return true;
    float depth[4];  // the view's depth of a world place, as applySeams' vz
    own_ground::depthRow(view, depth);
    seamStore_.build(groundTris_, groundTextures_, 0.5f, depth, p.nearZ,
                     [this](float x, float z) { return mapWorld_.laidAt(x, z); }, mapWorld_.laidKey());
    for (uint32_t size = 0; size < 2; ++size) {
        if (!ensureSeamSlots(size, seamStore_.slotsUsed(size))) {
            seamCardFailed_ = true;
            return false;
        }
        // the slots made, a run of them one after another written at once (a map's first frame: all of them)
        const std::vector<uint32_t>& slots = seamStore_.freshSlots(size);
        const UINT vertexBytes = seams::kSlotVertices[size] * UINT(sizeof(seams::CardVertex));
        const UINT indexBytes = seams::kSlotIndices[size] * UINT(sizeof(uint16_t));
        for (size_t n = 0; n < slots.size();) {
            size_t end = n + 1;
            while (end < slots.size() && slots[end] == slots[end - 1] + 1) ++end;
            const D3D11_BOX vb{slots[n] * vertexBytes, 0, 0, (slots[end - 1] + 1) * vertexBytes, 1, 1};
            const D3D11_BOX ib{slots[n] * indexBytes, 0, 0, (slots[end - 1] + 1) * indexBytes, 1, 1};
            ctx_->UpdateSubresource(seamSlots_[size].vertices, 0, &vb,
                                    seamStore_.freshVertices(size).data() + n * seams::kSlotVertices[size], 0, 0);
            ctx_->UpdateSubresource(seamSlots_[size].indices, 0, &ib,
                                    seamStore_.freshIndices(size).data() + n * seams::kSlotIndices[size], 0, 0);
            n = end;
        }
    }
    const std::vector<seams::Shown>& shown = seamStore_.shown();
    if (shown.empty()) return true;
    // each group's parts of a batch - its state and lightmap (inside), its texture (shows) - as the first group with
    // the same (a few kinds among thousands of draws: only the kinds' first groups looked over, the newest first)
    const size_t groups = groundGroups_.size();
    seamInClass_.assign(groups, UINT32_MAX);
    seamShowsClass_.assign(groups, UINT32_MAX);
    seamInKinds_.clear();
    seamShowsKinds_.clear();
    const auto kindOf = [&](uint32_t gr, std::vector<uint32_t>& classes, std::vector<uint32_t>& kinds, bool in) {
        if (classes[gr] != UINT32_MAX) return classes[gr];
        const GroundGroup11& x = groundGroups_[gr];
        const auto same = [&](uint32_t o) {
            const GroundGroup11& y = groundGroups_[o];
            return in ? y.texture[1] == x.texture[1] && y.keyed[1] == x.keyed[1] && y.state == x.state
                      : y.texture[0] == x.texture[0] && y.keyed[0] == x.keyed[0];
        };
        uint32_t found = UINT32_MAX;
        for (size_t q = kinds.size(); q-- > 0 && found == UINT32_MAX;)
            if (same(kinds[q])) found = kinds[q];
        if (found == UINT32_MAX) kinds.push_back(found = gr);
        return classes[gr] = found;
    };
    for (SeamBatch11& bt : seamBatches_) bt.pieces[0].clear(), bt.pieces[1].clear();
    size_t batches = 0, firstOther = 0, last = SIZE_MAX;
    for (int own = 1; own >= 0; --own) {  // the cells' own bands, then the neighbours' strips over them
        if (!own) firstOther = batches;
        for (uint32_t n = 0; n < uint32_t(shown.size()); ++n) {
            const seams::Shown& s = shown[n];
            if ((s.inside == s.shows) != (own == 1)) continue;
            if (s.inside >= groups || s.shows >= groups) continue;
            const uint32_t in = kindOf(s.inside, seamInClass_, seamInKinds_, true);
            const uint32_t sh = kindOf(s.shows, seamShowsClass_, seamShowsKinds_, false);
            size_t at = last != SIZE_MAX && last >= firstOther && seamBatches_[last].in == in &&
                                seamBatches_[last].shows == sh
                            ? last
                            : SIZE_MAX;
            for (size_t q = own ? 0 : firstOther; q < batches && at == SIZE_MAX; ++q)
                if (seamBatches_[q].in == in && seamBatches_[q].shows == sh) at = q;
            if (at == SIZE_MAX) {
                if (batches == seamBatches_.size()) seamBatches_.emplace_back();
                at = batches++;
                seamBatches_[at].in = in;
                seamBatches_[at].shows = sh;
            }
            seamBatches_[at].pieces[s.size].push_back(n);
            last = at;
        }
        last = SIZE_MAX;
    }
    // the instances, a batch's of each size one after another: the slot's first vertex and index, the indices, the
    // cell's corners' fog
    seamInstanceData_.clear();
    seamDraws_.clear();
    seamRec_.reset();
    static const uint8_t kNone[3 * 40] = {};  // the record's draw: three vertices of nothing (seam_vs reads the slots)
    for (size_t q = 0; q < batches; ++q) {
        const SeamBatch11& bt = seamBatches_[q];
        for (uint32_t size = 0; size < 2; ++size) {
            if (bt.pieces[size].empty()) continue;
            SeamDraw11 sd;
            sd.size = size;
            sd.first = uint32_t(seamInstanceData_.size() / 4);
            for (uint32_t n : bt.pieces[size]) {
                const seams::Shown& s = shown[n];
                seamInstanceData_.push_back(s.slot * seams::kSlotVertices[size]);
                seamInstanceData_.push_back(s.slot * seams::kSlotIndices[size]);
                seamInstanceData_.push_back(s.indices);
                seamInstanceData_.push_back(s.fog);
                sd.corners = std::max(sd.corners, s.indices);
            }
            sd.count = uint32_t(bt.pieces[size].size());
            const GroundGroup11& in = groundGroups_[bt.in];
            const GroundGroup11& shows = groundGroups_[bt.shows];
            DrawSetup st = in.state;  // as applySeams'
            st.blend = true;
            st.srcBlend = d3d7::BLEND_SRCALPHA;
            st.dstBlend = d3d7::BLEND_INVSRCALPHA;
            st.zWrite = false;
            st.zFunc = d3d7::CMP_LESSEQUAL;
            st.cull = d3d7::CULL_NONE;
            st.alphaTest = false;
            DrawCmd cmd;
            cmd.state = seamRec_.addState(st);
            cmd.texture[0] = shows.texture[0];
            cmd.keyed[0] = shows.keyed[0];
            cmd.texture[1] = in.texture[1];
            cmd.keyed[1] = in.keyed[1];
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL2;
            seamRec_.draw(cmd, kNone, 3, nullptr, 0);
            seamDraws_.push_back(sd);
        }
    }
    if (seamDraws_.empty()) return true;
    flushBatch();  // the game's draws gathered so far
    if (!fillBuffer(seamInstances_, seamInstanceBytes_, D3D11_BIND_VERTEX_BUFFER, seamInstanceData_.data(),
                    seamInstanceData_.size() * sizeof(uint32_t), nullptr))
        return true;
    seamPass_ = true;
    for (size_t n = 0; n < seamDraws_.size(); ++n) {
        seamDraw_ = seamDraws_[n];
        draw(seamRec_, seamRec_.draws()[n]);
    }
    flushBatch();
    seamDraw_ = SeamDraw11{};
    seamPass_ = false;
    if (seamsLogIn_ > 0 && --seamsLogIn_ == 0)
        LOG_INFO("ground seams on the card: %zu pieces in view in %zu draws over %zu triangles of %s's ground, %zu "
                 "made so far, %u + %u slots (Direct3D 11)", shown.size(), seamDraws_.size(), groundTris_.size(),
                 map_.c_str(), seamStore_.made(), seamStore_.slotsUsed(seams::kPlain),
                 seamStore_.slotsUsed(seams::kCut));
    return true;
}

// 2.6.18 (ao.cpp noteDarkness): a draw of blindness's darkness kept for applyDarkness.
void Renderer11::noteDarkness(const FrameRecord& rec, const DrawCmd& d, const DrawSetup& s) {
    const std::vector<uint8_t>& v = rec.vertexBytes();
    const std::vector<uint16_t>& ix = rec.indices();
    if (d.vertexOffset + size_t(d.vertexCount) * FrameRecord::stride(d.fvf) > v.size() ||
        size_t(d.firstIndex) + d.indexCount > ix.size())
        return;
    DrawCmd cmd = d;
    cmd.state = darknessRec_.addState(s);
    darknessRec_.draw(cmd, v.data() + d.vertexOffset, d.vertexCount, d.indexCount ? ix.data() + d.firstIndex : nullptr,
                      d.indexCount);
}

// 2.6.18: blindness's darkness over the world and its effects, from the strips' buffers (as applySeams draws them).
void Renderer11::applyDarkness() {
    darknessLaid_ = true;
    if (darknessRec_.draws().empty()) return;
    gradeNow();  // 3.0.5-beta.4: the colours put off meet the world before the darkness, as before
    flushBatch();  // the game's draws gathered so far are of its own vertices
    const std::vector<uint8_t>& v = darknessRec_.vertexBytes();
    const std::vector<uint16_t>& ix = darknessRec_.indices();
    if (fillBuffer(seamVertices_, seamVertexBytes_, D3D11_BIND_VERTEX_BUFFER, v.data(), v.size(),
                   bgraVertices_ ? nullptr : &darknessRec_) &&
        (ix.empty() || fillBuffer(seamIndices_, seamIndexBytes_, D3D11_BIND_INDEX_BUFFER, ix.data(),
                                  ix.size() * sizeof(uint16_t), nullptr))) {
        std::swap(vertices_, seamVertices_);
        std::swap(indices_, seamIndices_);
        darknessPass_ = true;
        for (const DrawCmd& cmd : darknessRec_.draws()) draw(darknessRec_, cmd);
        flushBatch();
        darknessPass_ = false;
        std::swap(vertices_, seamVertices_);
        std::swap(indices_, seamIndices_);
    }
    darknessRec_.reset();
}

}  // namespace uagfx::gpu
