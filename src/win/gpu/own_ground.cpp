#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "core/own_ground.hpp"
#include "core/trees.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr size_t kTexelsAFrame = size_t(1) << 19;  // pictures put up a frame (2 MB): the upload buffers stay small
constexpr int kAtlasStrip = 256;                    // the lightmaps' rows a piece
constexpr float kMostDepth = 4000.0f;               // how far off the own ground goes (the game's ends at 1500)
// smooth (and the strips between the textures) where a cell is 40 pixels or more on the screen (about 1000 at the
// game's 1080), the game's own cells past it - 3.0.4-beta.15: to 1500 before, much of the pass's 7.7 ms a frame on an
// i7-8700 (R-N6FC5V)
constexpr float kFinePixels = 40.0f;

double msSince(const LARGE_INTEGER& from) {
    LARGE_INTEGER now, f;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&f);
    return double(now.QuadPart - from.QuadPart) * 1000.0 / double(f.QuadPart);
}

}  // namespace

// 3.0.4-beta.10: a texture of the own ground's - made as Renderer::createTexture makes the game's, its id the driver's
uint32_t Renderer::Impl::ownTexture(int width, int height, int levels) {
    Texture t;
    t.width = width;
    t.height = height;
    t.levels = std::max(1, levels);
    t.srv = allocSrv();
    if (t.srv == kNoSrv) return 0;
    t.resource = createTexture2D(device, width, height, t.levels, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_NONE,
                                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (!t.resource) {
        freeSrv.push_back(t.srv);
        return 0;
    }
    device->CreateShaderResourceView(t.resource, nullptr, cpuSrvHandle(t.srv));
    const uint32_t id = nextOwnTexture++;
    char name[48];
    std::snprintf(name, sizeof name, "own ground %u, %dx%d", unsigned(id - 0xF0000000u), width, height);
    nameResource(t.resource, name);
    textures.emplace(id, t);
    return id;
}

// Its textures let go as the game's are (retireDying, once the GPU is past them)
void Renderer::Impl::dropOwnGround() {
    for (uint32_t id : ownState.ids)
        if (id && textures.count(id)) dying.push_back(id);
    for (uint32_t id : ownState.modelIds)  // 3.0.4-beta.14
        if (id && textures.count(id)) dying.push_back(id);
    ownState.modelIds.clear();
    ownState.modelTexture = ownState.modelLevel = 0;
    ownState.modelLikeKnown = false;
    ownState.modelFrames = 0;
    if (ownState.atlas && textures.count(ownState.atlas)) dying.push_back(ownState.atlas);
    ownState.serial = 0;
    ownState.ids.clear();
    ownState.atlas = 0;
    ownState.texture = ownState.level = 0;
    ownState.atlasRow = 0;
    ownState.up = false;
    ownState.frame = false;
    for (int m = 0; m < 2; ++m) {  // 3.0.4-beta.12
        std::vector<uint32_t>().swap(ownState.colours[m]);
        std::vector<uint8_t>().swap(ownState.fresh[m]);
    }
    std::vector<int32_t>().swap(ownState.chunkAt);
    ownState.frames = 0;
    ownState.slowest = 0.0;
    ownState.most = 0;
    ownState.cornersChanged = ownState.chunksMade = 0;
    ownState.made.valid = false;  // its record names the textures let go
    ownState.likeKnown = false;
    ownState.reused = 0;
}

// At the picture's first ground draw of the game's: the map's own ground read (MapWorld), its textures made and their
// pictures put up - a few a frame, the full colours with their mipmaps (the AI textures know them by their 16-bit
// texels, as the game's), the lightmaps in strips. True: drawn this picture in place of the game's.
bool Renderer::Impl::prepareOwnGround() {
    mapWorld.setOwnGround(true);
    mapWorld.poll();
    const OwnGround* g = mapWorld.ownGround();
    OwnState& o = ownState;
    if (!g) {
        if (o.serial) dropOwnGround();
        return false;
    }
    if (o.serial != g->serial) {
        dropOwnGround();
        o.serial = g->serial;
        o.shading.reset(g->width, g->height, g->zoom);
        o.colours[0].assign(g->mesh.vertices.size(), 0);  // 3.0.4-beta.12: the colours made as the chunks show
        o.colours[1].assign(g->distant.vertices.size(), 0);
        o.fresh[0].assign(g->mesh.chunks.size(), 0);
        o.fresh[1].assign(g->distant.chunks.size(), 0);
        o.chunksWide = (g->width + terrain::kChunk - 1) / terrain::kChunk;
        o.chunkAt.assign(size_t(o.chunksWide) * size_t((g->height + terrain::kChunk - 1) / terrain::kChunk), -1);
        for (size_t k = 0; k < g->mesh.chunks.size(); ++k) {
            const terrain::Chunk& c = g->mesh.chunks[k];
            o.chunkAt[size_t(c.cellY / terrain::kChunk) * size_t(o.chunksWide) + size_t(c.cellX / terrain::kChunk)] =
                int32_t(k);
        }
        for (const OwnGround::Picture& p : g->textures)
            o.ids.push_back(p.levels.empty() ? 0 : ownTexture(p.width, p.height, int(p.levels.size())));
        o.atlas = g->atlas.argb.empty() ? 0 : ownTexture(g->atlas.width, g->atlas.height, 1);
        for (const OwnGround::Picture& p : g->modelTextures)  // 3.0.4-beta.14
            o.modelIds.push_back(p.levels.empty() ? 0 : ownTexture(p.width, p.height, int(p.levels.size())));
        o.learning.reset(g->models, g->toLight);
    }
    if (o.up) return true;
    o.uploads.reset();
    size_t texels = 0;
    while (texels < kTexelsAFrame) {
        if (o.texture < g->textures.size()) {
            const OwnGround::Picture& p = g->textures[o.texture];
            const int w = std::max(p.width >> o.level, 1), h = std::max(p.height >> o.level, 1);
            if (!o.ids[o.texture] || o.level >= p.levels.size() || p.levels[o.level].size() != size_t(w) * size_t(h)) {
                ++o.texture;
                o.level = 0;
                continue;
            }
            o.uploads.upload(o.ids[o.texture], uint32_t(o.level), Rect{0, 0, w, h}, p.levels[o.level].data(), {},
                             o.level == 0 ? PixelFormat::A1R5G5B5 : PixelFormat::Unknown);
            texels += size_t(w) * size_t(h);
            ++o.level;
            continue;
        }
        if (o.atlas && o.atlasRow < g->atlas.height) {
            const int rows = std::min(kAtlasStrip, g->atlas.height - o.atlasRow);
            o.uploads.upload(o.atlas, 0, Rect{0, o.atlasRow, g->atlas.width, o.atlasRow + rows},
                             g->atlas.argb.data() + size_t(o.atlasRow) * size_t(g->atlas.width));
            texels += size_t(rows) * size_t(g->atlas.width);
            o.atlasRow += rows;
            continue;
        }
        if (o.modelTexture < g->modelTextures.size()) {  // 3.0.4-beta.14: the models' last
            const OwnGround::Picture& p = g->modelTextures[o.modelTexture];
            const int w = std::max(p.width >> o.modelLevel, 1), h = std::max(p.height >> o.modelLevel, 1);
            if (o.modelTexture >= o.modelIds.size() || !o.modelIds[o.modelTexture] || o.modelLevel >= p.levels.size() ||
                p.levels[o.modelLevel].size() != size_t(w) * size_t(h)) {
                ++o.modelTexture;
                o.modelLevel = 0;
                continue;
            }
            o.uploads.upload(o.modelIds[o.modelTexture], uint32_t(o.modelLevel), Rect{0, 0, w, h},
                             p.levels[o.modelLevel].data(), {},
                             o.modelLevel == 0 ? PixelFormat::A1R5G5B5 : PixelFormat::Unknown);
            texels += size_t(w) * size_t(h);
            ++o.modelLevel;
            continue;
        }
        o.up = true;
        break;
    }
    for (const UploadCmd& u : o.uploads.uploads()) upload(o.uploads, u);
    o.uploads.reset();
    if (!o.up) return false;
    unsigned made = 0;
    for (uint32_t id : o.ids) made += id != 0;
    LOG_INFO("own ground: %s's pictures up - %u of %u textures, lightmaps %dx%d; the game's ground left out from now",
             g->map.c_str(), made, unsigned(o.ids.size()), g->atlas.width, g->atlas.height);
    mapWorld.ownPicturesUp();  // the renderer has them: their memory back
    return true;
}

// A ground draw of the game's in place of which the own ground is drawn: its state, and the colours and fog it gives
// its corners (its walls' left to the cells: their corners lie under other colours)
void Renderer::Impl::noteOwnGround(const FrameRecord& rec, const DrawCmd& d) {
    OwnState& o = ownState;
    if (!o.likeTaken) {
        o.like = rec.states()[d.state];
        o.likeKnown = o.likeTaken = true;
    }
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    if (d.vertexOffset + size_t(d.vertexCount) * sizeof(own_ground::Tl2) > vb.size()) return;
    const own_ground::Tl2* v = reinterpret_cast<const own_ground::Tl2*>(vb.data() + d.vertexOffset);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
    float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
    float at[8][2];
    const uint32_t n = std::min(d.vertexCount, 8u);
    for (uint32_t i = 0; i < d.vertexCount; ++i) {
        if (v[i].rhw <= 0.0f) return;
        const float vz = 1.0f / v[i].rhw, vx = (v[i].x - p.cx) * vz / p.f0, vy = (v[i].y - p.cy) * vz / p.f1;
        const float x = (vx * m[0] + vy * m[3] + vz * m[6] + m[9]) * float(axes.x);
        const float z = (vx * m[2] + vy * m[5] + vz * m[8] + m[11]) * float(axes.z);
        if (i < n) at[i][0] = x, at[i][1] = z;
        lo[0] = std::min(lo[0], x), hi[0] = std::max(hi[0], x);
        lo[1] = std::min(lo[1], z), hi[1] = std::max(hi[1], z);
        o.shading.fog(vz, v[i].specular);
    }
    if (hi[0] - lo[0] < 1.0f || hi[1] - lo[1] < 1.0f) return;  // a wall: upright
    // 3.0.4-beta.15: the cell's first corner (its least x and z) gives its corner's colour; the others fill only
    for (uint32_t i = 0; i < n; ++i) {
        const bool first = at[i][0] - lo[0] < 0.5f && at[i][1] - lo[1] < 0.5f;
        o.shading.note(at[i][0], at[i][1], v[i].diffuse, first);
    }
}

// The own ground once a picture, at the world's first blended draw (or the interface) after the game's ground: the
// chunks in view (core/own_ground.hpp boxInView) - smooth near, the game's cells far off - projected as the game
// projects, in its colours and fog, a draw a texture (the chunks' together: a draw each was up to 938) with the
// state of the game's ground. 3.0.4-beta.12: with the ground seams (effects.ground_seams) the soft way from one
// texture to the next over it -
// the cells' own bands, then the neighbours' strips (core/terrain.hpp), blended by the noise as the game's ground's
// seams are (seamPass: shade() bit 31), a little towards the camera. A record of their own, as the seams' strips: the
// game's geometry goes up again with its next draw.
bool Renderer::Impl::drawOwnGround() {
    OwnState& o = ownState;
    o.drawn = true;
    const OwnGround* g = mapWorld.ownGround();
    if (!g || g->serial != o.serial || !o.likeKnown) return true;
    crumb("own ground");
    own_ground::View view;
    if (!own_ground::viewOf(viewToWorld, view)) return true;
    LARGE_INTEGER start;
    QueryPerformanceCounter(&start);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
    o.shading.fit();
    // the corners the game gave another colour: the chunks round them made again
    bool meanMoved = false;
    o.shading.takeChanged(o.changed, meanMoved);
    o.cornersChanged += o.changed.size();  // 3.0.4-beta.15: for the log
    if (meanMoved) {
        for (std::vector<uint8_t>& f : o.fresh) std::fill(f.begin(), f.end(), uint8_t(0));
    } else {
        for (uint32_t c : o.changed) {
            const int i = int(c % uint32_t(g->width + 1)), j = int(c / uint32_t(g->width + 1));
            for (int cy = j - 1; cy <= j; ++cy)
                for (int cx = i - 1; cx <= i; ++cx) {
                    if (cx < 0 || cy < 0 || cx >= g->width || cy >= g->height) continue;
                    const int32_t k = o.chunkAt[size_t(cy / terrain::kChunk) * size_t(o.chunksWide) +
                                                size_t(cx / terrain::kChunk)];
                    for (int m = 0; m < 2; ++m)
                        if (k >= 0 && size_t(k) < o.fresh[m].size()) o.fresh[m][size_t(k)] = 0;
                }
        }
    }
    const bool blends = effects.groundSeams && mapOutdoors;  // outdoors, as the game's ground's seams (seamsWanted)
    // 3.0.4-beta.15: the record of the last picture again when nothing it was made of changed
    OwnState::Made now;
    std::memcpy(now.view, viewToWorld, sizeof now.view);
    now.width = gameWidth, now.height = gameHeight, now.serial = o.serial, now.blends = blends;
    now.models = effects.ownFarModels, now.seamsView = seamsView, now.matched = o.learning.matched();
    now.fog[0] = o.shading.specularAt(100.0f), now.fog[1] = o.shading.specularAt(1000.0f);
    now.like = o.like;
    now.valid = true;
    const bool same = o.made.valid && !meanMoved && o.changed.empty() &&
                      std::memcmp(o.made.view, now.view, sizeof now.view) == 0 && o.made.width == now.width &&
                      o.made.height == now.height && o.made.serial == now.serial && o.made.blends == now.blends &&
                      o.made.models == now.models && o.made.seamsView == now.seamsView &&
                      o.made.matched == now.matched && o.made.fog[0] == now.fog[0] && o.made.fog[1] == now.fog[1] &&
                      o.made.like == now.like;
    size_t chunks = o.madeChunks, vertices = o.madeVertices, triangles = o.madeTriangles;
    if (same) ++o.reused;
    if (!same) {
        o.made = now;
        chunks = vertices = triangles = 0;
        const bool fogged = o.shading.fogged();
        const uint32_t plain = o.shading.specularAt(0.0f);  // no fog: the game's specular everywhere
        o.rec.reset();
        o.drawLayer.clear();
        DrawSetup st = o.like;
        st.cull = d3d7::CULL_NONE;                     // the axes may turn the cells over
        st.stage[0].mipFilter = d3d7::TFP_LINEAR;      // its pictures have their mipmaps: no shimmer far off
        st.stage[1].mipFilter = d3d7::TFP_NONE;        // the lightmaps' one level
        DrawSetup bs = st;                             // the strips': as the seams' (applySeams)
        bs.blend = true;
        bs.srcBlend = d3d7::BLEND_SRCALPHA;
        bs.dstBlend = d3d7::BLEND_INVSRCALPHA;
        bs.zWrite = false;
        bs.zFunc = seamsView == 2 ? d3d7::CMP_ALWAYS : d3d7::CMP_LESSEQUAL;
        bs.alphaTest = false;
        const uint32_t state = o.rec.addState(st), blendState = o.rec.addState(bs);
        for (std::vector<OwnState::Pending>& layer : o.pending) {
            layer.resize(o.ids.size() + 1);
            for (OwnState::Pending& pd : layer) pd.verts.clear(), pd.idx.clear();
        }
        std::vector<uint8_t> behind;
        const auto emit = [&](uint8_t layer, size_t t) {
            OwnState::Pending& pd = o.pending[layer][t];
            if (!pd.idx.empty()) {
                DrawCmd cmd;
                cmd.state = layer == terrain::kGround ? state : blendState;
                cmd.texture[0] = t < o.ids.size() ? o.ids[t] : 0;
                cmd.texture[1] = o.atlas;
                cmd.primitive = d3d7::PT_TRIANGLELIST;
                cmd.fvf = d3d7::FVF_TL2;
                o.rec.draw(cmd, pd.verts.data(), uint32_t(pd.verts.size()), pd.idx.data(), uint32_t(pd.idx.size()));
                o.drawLayer.push_back(layer);
                vertices += pd.verts.size();
                triangles += pd.idx.size() / 3;
            }
            pd.verts.clear();
            pd.idx.clear();
        };
        const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
        const float fineDepth = std::clamp(g->zoom * p.f1 / kFinePixels, 600.0f, 1500.0f);
        for (size_t k = 0; k < g->mesh.chunks.size(); ++k) {
            const terrain::Chunk& box = g->mesh.chunks[k];
            float lo[3], hi[3];
            for (int a = 0; a < 3; ++a) {
                lo[a] = std::min(box.lo[a] * sign[a], box.hi[a] * sign[a]);
                hi[a] = std::max(box.lo[a] * sign[a], box.hi[a] * sign[a]);
            }
            float nearest = 0.0f;
            if (!own_ground::boxInView(view, p, gameWidth, gameHeight, lo, hi, kMostDepth, nearest)) continue;
            const int which = nearest > fineDepth && k < g->distant.chunks.size() ? 1 : 0;
            const terrain::Mesh& mesh = which ? g->distant : g->mesh;
            const terrain::Chunk& ch = mesh.chunks[k];
            std::vector<uint32_t>& colours = o.colours[which];
            ++chunks;
            if (!o.fresh[which][k]) {  // its colours, the game's round its corners
                ++o.chunksMade;
                for (uint32_t b = ch.firstBatch; b < ch.firstBatch + ch.batchCount; ++b) {
                    const terrain::Batch& bt = mesh.batches[b];
                    for (uint32_t v = bt.firstVertex; v < bt.firstVertex + bt.vertexCount; ++v) {
                        const terrain::Vertex& tv = mesh.vertices[v];
                        const uint32_t c = o.shading.diffuseAt(tv.pos[0], tv.pos[2]);
                        // a neighbour's strip: its alpha how much the place is the neighbour's
                        colours[v] = bt.layer == terrain::kNeighbour ? (c & 0xFFFFFFu) | (tv.color & 0xFF000000u) : c;
                    }
                }
                o.fresh[which][k] = 1;
            }
            for (uint32_t b = ch.firstBatch; b < ch.firstBatch + ch.batchCount; ++b) {
                const terrain::Batch& bt = mesh.batches[b];
                if (bt.layer != terrain::kGround && !blends) continue;
                const size_t tex =
                    bt.texture >= 0 && size_t(bt.texture) < o.ids.size() ? size_t(bt.texture) : o.ids.size();
                OwnState::Pending& pd = o.pending[bt.layer][tex];
                if (pd.verts.size() + bt.vertexCount > 65535) emit(bt.layer, tex);
                const size_t base = pd.verts.size();
                pd.verts.resize(base + bt.vertexCount, own_ground::Tl2{});
                behind.assign(bt.vertexCount, 0);
                // the strips a little towards the camera: on the ground's own points, the bands near them (as the
                // seams')
                const float lift = bt.layer == terrain::kNeighbour ? 0.05f
                                   : bt.layer == terrain::kOwnBand ? 0.12f
                                                                   : 0.0f;
                for (uint32_t i = 0; i < bt.vertexCount; ++i) {
                    const terrain::Vertex& tv = mesh.vertices[bt.firstVertex + i];
                    own_ground::Tl2& t = pd.verts[base + i];
                    if (!own_ground::place(view, p, axes, tv.pos, t)) {
                        behind[i] = 1;
                        continue;
                    }
                    const float zd = 1.0f / t.rhw;
                    if (lift > 0.0f) t.z = own_ground::depthOf(p, std::max(zd - lift, p.nearZ + 0.005f));
                    t.diffuse = colours[bt.firstVertex + i];
                    const uint32_t fog = fogged ? o.shading.specularAt(zd) : plain;
                    t.specular = bt.layer == terrain::kGround ? fog : (fog & 0xFF000000u) | (tv.blend & 0xFFFFFFu);
                    t.u0 = tv.uv[0];
                    t.v0 = tv.uv[1];
                    t.u1 = tv.lm[0];
                    t.v1 = tv.lm[1];
                }
                for (uint32_t i = bt.firstIndex; i + 3 <= bt.firstIndex + bt.indexCount; i += 3) {
                    const uint32_t a = mesh.indices[i], b2 = mesh.indices[i + 1], c = mesh.indices[i + 2];
                    if (behind[a] || behind[b2] || behind[c]) continue;  // across the near plane: left out
                    pd.idx.push_back(uint16_t(base + a));
                    pd.idx.push_back(uint16_t(base + b2));
                    pd.idx.push_back(uint16_t(base + c));
                }
            }
        }
        for (uint8_t layer = 0; layer < 3; ++layer)  // the ground, then the own bands, then the neighbours' strips
            for (size_t t = 0; t < o.pending[layer].size(); ++t) emit(layer, t);
        drawOwnModels(view, p, axes);  // 3.0.4-beta.14: the models past the far plane, into the same record (layer 3)
        o.madeChunks = chunks, o.madeVertices = vertices, o.madeTriangles = triangles;
    }
    const double built = msSince(start);
    if (!o.rec.draws().empty()) {
        flushDraws();  // the game's draws gathered so far are of its own vertices, uploaded before these
        prepareGeometry(o.rec);
        if (vertexBase) {
            ownPass = true;
            for (size_t k = 0; k < o.rec.draws().size(); ++k) {
                seamPass = o.drawLayer[k] == terrain::kOwnBand || o.drawLayer[k] == terrain::kNeighbour;
                seamOwnBand = o.drawLayer[k] == terrain::kOwnBand;
                draw(o.rec, o.rec.draws()[k]);  // layer 3: the models (as the game's own models are drawn)
            }
            seamPass = seamOwnBand = false;
            ownPass = false;
        }
        geometryList = 0;  // the game's record's vertices go up again with its next draw
    }
    ++o.frames;
    o.slowest = std::max(o.slowest, built);
    o.most = std::max(o.most, vertices);
    if (o.frames == 1 || o.frames == 600)  // the first picture, then ten seconds' or so worst
        LOG_INFO("own ground: %s - %zu chunks of %zu in view, %zu draws, %zu vertices, %zu triangles, %.2f ms on the "
                 "CPU (frame %u: at most %zu vertices, %.2f ms; a frame %.1f corners given another colour, %.2f "
                 "chunks' colours made again; %.0f %% of the pictures drawn again as they were); strips between the "
                 "textures %s",
                 g->map.c_str(), chunks, g->mesh.chunks.size(), o.rec.draws().size(), vertices, triangles, built,
                 o.frames, o.most, o.slowest, double(o.cornersChanged) / double(o.frames),
                 double(o.chunksMade) / double(o.frames), 100.0 * double(o.reused) / double(o.frames),
                 blends ? "on" : "off");
    // 3.0.4-beta.14: whether the own models stand where the game's do - of the game's model triangles near, how many
    // lay where one of ours does (all but the moving ones: placed as the game places them)
    if (o.frames == 600 && effects.ownFarModels)
        LOG_INFO("own models: %s - of the game's model triangles near %zu noted, %zu (%.0f %%) where ours lie; %zu "
                 "triangles of ours past the far plane in the last picture",
                 g->map.c_str(), o.learning.seen(), o.learning.matched(),
                 o.learning.seen() ? 100.0 * double(o.learning.matched()) / double(o.learning.seen()) : 0.0,
                 o.modelsDrawn);
    return !lost;
}

// 3.0.4-beta.14: a draw of the game's own models near (one texture coordinate, writing depth, not blended, no
// effect's): its triangles back into the world - the own models' colours learnt from them, and how many lie where ours
// do (the log); its state the far models' (the stages, the alpha test, as the game draws its models)
void Renderer::Impl::noteOwnModels(const FrameRecord& rec, const DrawCmd& d) {
    OwnState& o = ownState;
    // the learning's own ground still the one MapWorld has (it may have read another since: the old one let go)
    const OwnGround* g = mapWorld.ownGround();
    if (!g || g->serial != o.serial) return;
    if (!o.modelLikeKnown) {
        o.modelLike = rec.states()[d.state];
        o.modelLikeKnown = true;
    }
    constexpr uint32_t kMostNoted = 600;  // a picture's: the learning goes on over the pictures
    if (o.modelsNoted >= kMostNoted) return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    if (d.vertexOffset + size_t(d.vertexCount) * sizeof(own_ground::Tl1) > vb.size()) return;
    const own_ground::Tl1* v = reinterpret_cast<const own_ground::Tl1*>(vb.data() + d.vertexOffset);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
    const auto file = [&](const own_ground::Tl1& t, float out[3]) {
        if (t.rhw <= 0.0f) return false;
        const float vz = 1.0f / t.rhw, vx = (t.x - p.cx) * vz / p.f0, vy = (t.y - p.cy) * vz / p.f1;
        out[0] = (vx * m[0] + vy * m[3] + vz * m[6] + m[9]) * float(axes.x);
        out[1] = (vx * m[1] + vy * m[4] + vz * m[7] + m[10]) * float(axes.y);
        out[2] = (vx * m[2] + vy * m[5] + vz * m[8] + m[11]) * float(axes.z);
        return true;
    };
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
    for (uint32_t t = 0; t < triangles && o.modelsNoted < kMostNoted; ++t) {
        uint32_t k[3];
        const uint32_t base = strip ? t : t * 3;
        if (!vertexAt(base, k[0]) || !vertexAt(base + 1, k[1]) || !vertexAt(base + 2, k[2])) break;
        float corners[3][3];
        uint32_t colours[3];
        bool ok = true;
        for (int c = 0; c < 3 && ok; ++c) {
            ok = file(v[k[c]], corners[c]);
            colours[c] = v[k[c]].diffuse;
        }
        if (!ok) continue;
        o.learning.note(corners, colours);
        ++o.modelsNoted;
    }
}

// 3.0.4-beta.14: the parts of the map's models past the game's far plane (own_ground::pastDepth) - the chunks reaching
// past it, each triangle cut at it, in the colours learnt (own_models::Learning), a draw a texture with the game's
// model state (both sides; its alpha test lower: the leaves stay at the smaller mipmaps)
void Renderer::Impl::drawOwnModels(const own_ground::View& view, const camera::Projection& p, const map::Axes& axes) {
    OwnState& o = ownState;
    o.modelsDrawn = 0;
    const OwnGround* g = mapWorld.ownGround();
    if (!g || !effects.ownFarModels || !o.modelLikeKnown || g->models.chunks.empty()) return;
    o.learning.fit();
    DrawSetup ms = o.modelLike;
    ms.cull = d3d7::CULL_NONE;
    ms.stage[0].mipFilter = d3d7::TFP_LINEAR;
    if (ms.alphaTest && ms.alphaRef > 0x60) ms.alphaRef = 0x60;
    const uint32_t state = o.rec.addState(ms);
    o.modelPending.resize(o.modelIds.size() + 1);
    for (OwnState::ModelPending& pd : o.modelPending) pd.verts.clear(), pd.idx.clear();
    const bool fogged = o.shading.fogged();
    const uint32_t plain = o.shading.specularAt(0.0f);
    const auto emit = [&](size_t t) {
        OwnState::ModelPending& pd = o.modelPending[t];
        if (!pd.idx.empty()) {
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = t < o.modelIds.size() ? o.modelIds[t] : 0;
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            o.rec.draw(cmd, pd.verts.data(), uint32_t(pd.verts.size()), pd.idx.data(), uint32_t(pd.idx.size()));
            o.drawLayer.push_back(3);
        }
        pd.verts.clear();
        pd.idx.clear();
    };
    const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
    const terrain::Mesh& mesh = g->models;
    std::vector<float> seen;      // a batch's vertices in the view, each worked out once
    std::vector<int32_t> putAt;   // a batch's vertex in its texture's draw, -1 not yet
    for (const terrain::Chunk& box : mesh.chunks) {
        float lo[3], hi[3];
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(box.lo[a] * sign[a], box.hi[a] * sign[a]);
            hi[a] = std::max(box.lo[a] * sign[a], box.hi[a] * sign[a]);
        }
        float nearest = 0.0f, farthest = 0.0f;
        if (!own_ground::boxInView(view, p, gameWidth, gameHeight, lo, hi, kMostDepth, nearest, &farthest) ||
            farthest < p.farZ)
            continue;  // none of it past the far plane: the game's own
        for (uint32_t b = box.firstBatch; b < box.firstBatch + box.batchCount; ++b) {
            const terrain::Batch& bt = mesh.batches[b];
            const size_t tex = bt.texture >= 0 && size_t(bt.texture) < o.modelIds.size() ? size_t(bt.texture)
                                                                                          : o.modelIds.size();
            OwnState::ModelPending& pd = o.modelPending[tex];
            if (pd.verts.size() + bt.vertexCount + 64 > 65535) emit(tex);
            // 3.0.4-beta.18: leaves - each tree's own shade (tree_tints) baked into its vertices by where they lie: a
            // draw of these holds many trees, the game's own leaves get theirs a draw at a time (leafShading)
            const auto picture = tex < o.modelIds.size() ? textures.find(o.modelIds[tex]) : textures.end();
            const bool tinted = effects.treeTints && picture != textures.end() && picture->second.leafy;
            const trees::Season season = effects.treeSeasons ? mapSeason : trees::Season::Green;
            const auto shaded = [&](uint32_t colour, const float* file) {
                if (!tinted) return colour;
                const trees::Shade sh = trees::shadeAt(file[0] * sign[0], file[2] * sign[2], season);
                uint32_t out = colour & 0xFF000000u;
                for (int n = 0; n < 3; ++n) {
                    const float c = float((colour >> ((2 - n) * 8)) & 0xFF) * sh.tint[n];
                    out |= uint32_t(std::clamp(c, 0.0f, 255.0f) + 0.5f) << ((2 - n) * 8);
                }
                return out;
            };
            seen.resize(size_t(bt.vertexCount) * 3);
            putAt.assign(bt.vertexCount, -1);
            for (uint32_t v = 0; v < bt.vertexCount; ++v) {
                const float* f = mesh.vertices[bt.firstVertex + v].pos;
                const float w[3] = {f[0] * sign[0], f[1] * sign[1], f[2] * sign[2]};
                own_ground::toView(view, w, &seen[size_t(v) * 3]);
            }
            // a corner of ours: its place on the screen, its colour, its fog, its texture's coordinates
            const auto corner = [&](const float at[3], uint32_t colour, float u, float v, own_ground::Tl1& t) {
                own_ground::Tl2 placed;
                if (!own_ground::placeView(p, at, placed)) return false;
                t.x = placed.x, t.y = placed.y, t.z = placed.z, t.rhw = placed.rhw;
                t.diffuse = colour;
                t.specular = fogged ? o.shading.specularAt(at[2]) : plain;
                t.u = u, t.v = v;
                return true;
            };
            for (uint32_t i = bt.firstIndex; i + 3 <= bt.firstIndex + bt.indexCount; i += 3) {
                const uint32_t lv[3] = {mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]};
                const float zd[3] = {seen[size_t(lv[0]) * 3 + 2], seen[size_t(lv[1]) * 3 + 2],
                                     seen[size_t(lv[2]) * 3 + 2]};
                if (zd[0] < p.farZ && zd[1] < p.farZ && zd[2] < p.farZ) continue;  // the game's own
                if (zd[0] >= p.farZ && zd[1] >= p.farZ && zd[2] >= p.farZ) {  // all of it: its corners as they are
                    if (pd.verts.size() + 3 > 65535) {
                        emit(tex);
                        putAt.assign(bt.vertexCount, -1);
                    }
                    for (int k = 0; k < 3; ++k) {
                        if (putAt[lv[k]] < 0) {
                            const terrain::Vertex& mv = mesh.vertices[bt.firstVertex + lv[k]];
                            own_ground::Tl1 t{};
                            corner(&seen[size_t(lv[k]) * 3],
                                   shaded(o.learning.colourOf(bt.firstVertex + lv[k]), mv.pos), mv.uv[0], mv.uv[1], t);
                            putAt[lv[k]] = int32_t(pd.verts.size());
                            pd.verts.push_back(t);
                        }
                        pd.idx.push_back(uint16_t(putAt[lv[k]]));
                    }
                    ++o.modelsDrawn;
                    continue;
                }
                // across the far plane: the part past it (own_ground::pastDepth), its corners made anew
                float share[4][3];
                const int corners = own_ground::pastDepth(zd, p.farZ, share);
                if (corners < 3) continue;
                if (pd.verts.size() + 4 > 65535) {
                    emit(tex);
                    putAt.assign(bt.vertexCount, -1);
                }
                const size_t base = pd.verts.size();
                bool ok = true;
                for (int q = 0; q < corners && ok; ++q) {
                    float at[3] = {0, 0, 0}, u = 0.0f, v = 0.0f, ch[4] = {0, 0, 0, 0};
                    for (int k = 0; k < 3; ++k) {
                        const terrain::Vertex& mv = mesh.vertices[bt.firstVertex + lv[k]];
                        const uint32_t c = shaded(o.learning.colourOf(bt.firstVertex + lv[k]), mv.pos);
                        for (int a = 0; a < 3; ++a) at[a] += share[q][k] * seen[size_t(lv[k]) * 3 + size_t(a)];
                        u += share[q][k] * mv.uv[0];
                        v += share[q][k] * mv.uv[1];
                        for (int n = 0; n < 4; ++n) ch[n] += share[q][k] * float((c >> (n * 8)) & 0xFF);
                    }
                    uint32_t colour = 0;
                    for (int n = 0; n < 4; ++n) colour |= uint32_t(std::clamp(int(ch[n] + 0.5f), 0, 255)) << (n * 8);
                    own_ground::Tl1 t{};
                    ok = corner(at, colour, u, v, t);
                    pd.verts.push_back(t);
                }
                if (!ok) {
                    pd.verts.resize(base);
                    continue;
                }
                for (int q = 1; q + 1 < corners; ++q) {  // a fan round the first corner
                    pd.idx.push_back(uint16_t(base));
                    pd.idx.push_back(uint16_t(base + size_t(q)));
                    pd.idx.push_back(uint16_t(base + size_t(q) + 1));
                    ++o.modelsDrawn;
                }
            }
        }
    }
    for (size_t t = 0; t < o.modelPending.size(); ++t) emit(t);
}

}  // namespace uagfx::gpu
