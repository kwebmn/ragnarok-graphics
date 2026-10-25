#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "core/own_ground.hpp"
#include "core/trees.hpp"
#include "core/water.hpp"
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
constexpr int kRelitAPicture = 8;  // 3.0.4-beta.28: chunks made again a picture when the hills' light moved
constexpr float kMorphFrom = 0.75f;  // 3.0.4-beta.29: of fineDepth - the smooth ground eased onto the plain past it

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
    for (uint32_t id : ownState.waterIds)  // 3.0.4-beta.20
        if (id && textures.count(id)) dying.push_back(id);
    ownState.waterIds.clear();
    ownState.waterPicture = ownState.waterPictureLevel = 0;
    ownState.modelLikeKnown = false;
    ownState.modelFrames = 0;
    ownState.waterTexture = ownState.waterColour = 0;  // 3.0.4-beta.20: the game's water picture of another map
    ownState.lightOn = false;  // 3.0.4-beta.30: the corners' light made again for the next map
    std::vector<float>().swap(ownState.cornerLight);
    ownState.wavePhaseKnown = false;
    ownState.waterLikeKnown = ownState.waterWaitLogged = false;
    ownState.waterFrames = 0;
    ownState.waterMost = 0;
    if (ownState.atlas && textures.count(ownState.atlas)) dying.push_back(ownState.atlas);
    ownState.serial = 0;
    ownState.ids.clear();
    ownState.atlas = 0;
    ownState.texture = ownState.level = 0;
    ownState.atlasRow = 0;
    ownState.up = ownState.groundUp = false;
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
    ownState.minuteFrames = ownState.minuteReused = 0;  // 3.0.4-beta.27
    std::fill(ownState.anew, ownState.anew + 8, 0u);
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
        for (const OwnGround::Picture& p : g->waterPictures)  // 3.0.4-beta.20
            o.waterIds.push_back(p.levels.empty() ? 0 : ownTexture(p.width, p.height, int(p.levels.size())));
        o.learning.reset(g->models, g->toLight);
    }
    if (o.up) return true;
    o.uploads.reset();
    size_t texels = 0;
    // 3.0.4-beta.14: the models' pictures, then (3.0.4-beta.20) the water's - a level of the next one; false: all up
    const auto putUp = [&](const std::vector<OwnGround::Picture>& pictures, const std::vector<uint32_t>& ids,
                           size_t& index, size_t& level) {
        while (index < pictures.size()) {
            const OwnGround::Picture& p = pictures[index];
            const int w = std::max(p.width >> level, 1), h = std::max(p.height >> level, 1);
            if (index >= ids.size() || !ids[index] || level >= p.levels.size() ||
                p.levels[level].size() != size_t(w) * size_t(h)) {
                ++index;
                level = 0;
                continue;
            }
            o.uploads.upload(ids[index], uint32_t(level), Rect{0, 0, w, h}, p.levels[level].data(), {},
                             level == 0 ? PixelFormat::A1R5G5B5 : PixelFormat::Unknown);
            texels += size_t(w) * size_t(h);
            ++level;
            return true;
        }
        return false;
    };
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
        if (putUp(g->modelTextures, o.modelIds, o.modelTexture, o.modelLevel) ||
            putUp(g->waterPictures, o.waterIds, o.waterPicture, o.waterPictureLevel))
            continue;
        o.up = true;
        break;
    }
    for (const UploadCmd& u : o.uploads.uploads()) upload(o.uploads, u);
    o.uploads.reset();
    // 3.0.4-beta.27: the own ground drawn as soon as its own pictures are up - the game's ground showed for most of a
    // second after a map's start, then gave way to it; the models' and the water's go up on while it is drawn
    if (!o.groundUp && (o.up || (o.texture >= g->textures.size() && (!o.atlas || o.atlasRow >= g->atlas.height)))) {
        o.groundUp = true;
        unsigned made = 0;
        for (uint32_t id : o.ids) made += id != 0;
        LOG_INFO("own ground: %s's pictures up - %u of %u textures, lightmaps %dx%d; the game's ground left out from "
                 "now", g->map.c_str(), made, unsigned(o.ids.size()), g->atlas.width, g->atlas.height);
    }
    if (o.up) {
        LOG_INFO("own ground: %s's models' and water's pictures up", g->map.c_str());
        mapWorld.ownPicturesUp();  // the renderer has them all: their memory back
    }
    return o.groundUp;
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
    // 3.0.4-beta.26 (the user chose «Свет своего мира»): the smooth hills' light worked out on the own ground itself
    // (own_ground::reliefLight, as hillLight) from its corners' normals - near and past the game's far plane alike -,
    // baked into its vertices' colours; made again when the sun, the light all round or the strength moved
    const bool relief = hillsWanted() && !g->cornerNormals.empty();
    float sun[3] = {0.0f, -1.0f, 0.0f};
    if (relief) {
        sunInWorld(sun);
        sun[0] *= float(axes.x), sun[1] *= float(axes.y), sun[2] *= float(axes.z);  // the world's way in file places
    }
    const float night = float(std::clamp(effects.nightNow, 0, 100)) / 100.0f;
    const float ambient = mapOutdoors ? 0.55f + 0.3f * night : 0.75f;  // as hillsConstants
    const float strength = float(std::clamp(effects.smoothHillsStrength, 0, 200)) / 100.0f;
    const bool lightMoved = relief != o.lightOn ||
                            (relief && (std::fabs(sun[0] - o.lightSun[0]) > 0.005f ||
                                        std::fabs(sun[1] - o.lightSun[1]) > 0.005f ||
                                        std::fabs(sun[2] - o.lightSun[2]) > 0.005f ||
                                        std::fabs(ambient - o.lightAmbient) > 0.005f || strength != o.lightStrength));
    if (lightMoved) {
        o.lightOn = relief;
        std::copy(sun, sun + 3, o.lightSun);
        o.lightAmbient = ambient, o.lightStrength = strength;
        // 3.0.4-beta.30: the light a corner, once - each point takes it between its cell's corners
        if (relief)
            o.cornerLight = own_ground::cornerLights(g->cornerNormals, sun, ambient, strength);
        else
            std::vector<float>().swap(o.cornerLight);
        // 3.0.4-beta.28: made again a few chunks a picture (2: fresh but in the light before) - the sun of the day's
        // clock moves a little every minute or two, and all of the view at once was a hitch of 10 ms
        for (std::vector<uint8_t>& f : o.fresh)
            for (uint8_t& c : f) c = c ? uint8_t(2) : uint8_t(0);
    }
    const bool blends = effects.groundSeams && mapOutdoors;  // outdoors, as the game's ground's seams (seamsWanted)
    // 3.0.4-beta.15: the record of the last picture again when nothing it was made of changed
    OwnState::Made now;
    std::memcpy(now.view, viewToWorld, sizeof now.view);
    now.width = gameWidth, now.height = gameHeight, now.serial = o.serial, now.blends = blends;
    // 3.0.4-beta.20: the colours learnt anew, not the triangles matched - those grew every picture (R-35N5QN: 0 %)
    // 3.0.4-beta.27: the models once their pictures are up (o.up)
    now.models = effects.ownFarModels && o.up, now.seamsView = seamsView, now.matched = o.learning.changes();
    now.fog[0] = o.shading.specularAt(100.0f), now.fog[1] = o.shading.specularAt(1000.0f);
    now.like = o.like;
    now.valid = true;
    const bool same = o.made.valid && !meanMoved && !lightMoved && !o.lightPending && o.changed.empty() &&
                      std::memcmp(o.made.view, now.view, sizeof now.view) == 0 && o.made.width == now.width &&
                      o.made.height == now.height && o.made.serial == now.serial && o.made.blends == now.blends &&
                      o.made.models == now.models && o.made.seamsView == now.seamsView &&
                      o.made.matched == now.matched && o.made.fog[0] == now.fog[0] && o.made.fog[1] == now.fog[1] &&
                      o.made.like == now.like;
    size_t chunks = o.madeChunks, vertices = o.madeVertices, triangles = o.madeTriangles;
    if (same) ++o.reused, ++o.minuteReused;
    if (!same && o.made.valid) {  // 3.0.4-beta.27: the first reason found
        const int why = std::memcmp(o.made.view, now.view, sizeof now.view) != 0 ? 0
                        : !o.changed.empty()                                  ? 1
                        : meanMoved                                           ? 2
                        : lightMoved                                          ? 3
                        : o.made.fog[0] != now.fog[0] || o.made.fog[1] != now.fog[1] ? 4
                        : !(o.made.like == now.like)                          ? 5
                        : o.made.matched != now.matched                       ? 6
                                                                              : 7;
        ++o.anew[why];
    }
    ++o.minuteFrames;
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
        int relit = 0;  // 3.0.4-beta.28: chunks made again in the moved light this picture
        o.lightPending = false;
        const float from = own_ground::ownFrom(p);  // 3.0.4-beta.23: the triangles across it cut there
        const float morphFrom = fineDepth * kMorphFrom;  // 3.0.4-beta.29: the smooth ground eased from here on
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
            uint8_t& fresh = o.fresh[which][k];
            if (fresh == 2) {  // 3.0.4-beta.28: in the light before it moved - a few a picture
                if (relit < kRelitAPicture)
                    ++relit, fresh = 0;
                else
                    o.lightPending = true;
            }
            if (!fresh) {  // its colours, the game's round its corners
                ++o.chunksMade;
                for (uint32_t b = ch.firstBatch; b < ch.firstBatch + ch.batchCount; ++b) {
                    const terrain::Batch& bt = mesh.batches[b];
                    for (uint32_t v = bt.firstVertex; v < bt.firstVertex + bt.vertexCount; ++v) {
                        const terrain::Vertex& tv = mesh.vertices[v];
                        uint32_t c = o.shading.diffuseAt(tv.pos[0], tv.pos[2]);
                        if (o.lightOn) {  // 3.0.4-beta.26: the smooth hills' light; 3.0.4-beta.30 the corners' between
                            const float light =
                                own_ground::lightAt(o.cornerLight, g->width, g->height, g->zoom, tv.pos[0], tv.pos[2]);
                            uint32_t lit = c & 0xFF000000u;
                            for (int part = 0; part < 3; ++part) {
                                const float channel = float((c >> (part * 8)) & 0xFF) * light;
                                lit |= uint32_t(std::clamp(int(channel + 0.5f), 0, 255)) << (part * 8);
                            }
                            c = lit;
                        }
                        // a neighbour's strip: its alpha how much the place is the neighbour's
                        colours[v] = bt.layer == terrain::kNeighbour ? (c & 0xFFFFFFu) | (tv.color & 0xFF000000u) : c;
                    }
                }
                fresh = 1;
            }
            for (uint32_t b = ch.firstBatch; b < ch.firstBatch + ch.batchCount; ++b) {
                const terrain::Batch& bt = mesh.batches[b];
                if (bt.layer != terrain::kGround && !blends) continue;
                const size_t tex =
                    bt.texture >= 0 && size_t(bt.texture) < o.ids.size() ? size_t(bt.texture) : o.ids.size();
                OwnState::Pending& pd = o.pending[bt.layer][tex];
                // its vertices, and two more for each triangle cut where the depth is squeezed
                if (pd.verts.size() + bt.vertexCount + bt.indexCount / 3 * 2 > 65535) emit(bt.layer, tex);
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
                    float zd = 1.0f / t.rhw;
                    // 3.0.4-beta.29: the smooth ground eased onto the plain one as it nears the change (fineDepth) -
                    // a chunk changes to the plain ground lying on it already: no jump, no crack along their seam
                    if (which == 0 && zd > morphFrom && tv.plain != tv.pos[1]) {
                        const float f = std::min((zd - morphFrom) / (fineDepth - morphFrom), 1.0f);
                        const float m = f * f * (3.0f - 2.0f * f);
                        const float eased[3] = {tv.pos[0], tv.pos[1] + (tv.plain - tv.pos[1]) * m, tv.pos[2]};
                        own_ground::place(view, p, axes, eased, t);
                        zd = 1.0f / t.rhw;
                    }
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
                    const uint32_t tri[3] = {a, b2, c};
                    const float zd[3] = {1.0f / pd.verts[base + a].rhw, 1.0f / pd.verts[base + b2].rhw,
                                         1.0f / pd.verts[base + c].rhw};
                    const bool nearer = zd[0] < from || zd[1] < from || zd[2] < from;
                    if (!nearer || (zd[0] < from && zd[1] < from && zd[2] < from)) {
                        for (uint32_t k : tri) pd.idx.push_back(uint16_t(base + k));
                        continue;
                    }
                    // 3.0.4-beta.23 (R-W7A5XC): across the squeeze - its near side and its far side, each one depth
                    // (own_ground::nearDepth), the two points of the cut shared
                    float sides[2][4][3];
                    const int corners[2] = {own_ground::nearDepth(zd, from, sides[0]),
                                            own_ground::pastDepth(zd, from, sides[1])};
                    float cutAt[2][3];
                    uint16_t cutIndex[2] = {0, 0};
                    int cuts = 0;
                    const auto vertexOf = [&](const float* w) {
                        for (int k = 0; k < 3; ++k)
                            if (w[k] == 1.0f) return uint16_t(base + tri[k]);
                        for (int k = 0; k < cuts; ++k)
                            if (w[0] == cutAt[k][0] && w[1] == cutAt[k][1] && w[2] == cutAt[k][2]) return cutIndex[k];
                        float at[3] = {0, 0, 0}, ch[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}}, uv[4] = {0, 0, 0, 0};
                        for (int k = 0; k < 3; ++k) {
                            const own_ground::Tl2& s = pd.verts[base + tri[k]];
                            at[0] += w[k] * (s.x - p.cx) * zd[k] / p.f0;
                            at[1] += w[k] * (s.y - p.cy) * zd[k] / p.f1;
                            at[2] += w[k] * zd[k];
                            for (int n = 0; n < 4; ++n) {
                                ch[0][n] += w[k] * float((s.diffuse >> (n * 8)) & 0xFF);
                                ch[1][n] += w[k] * float((s.specular >> (n * 8)) & 0xFF);
                            }
                            uv[0] += w[k] * s.u0, uv[1] += w[k] * s.v0, uv[2] += w[k] * s.u1, uv[3] += w[k] * s.v1;
                        }
                        own_ground::Tl2 t{};
                        own_ground::placeView(p, at, t);
                        if (lift > 0.0f) t.z = own_ground::depthOf(p, std::max(at[2] - lift, p.nearZ + 0.005f));
                        for (int n = 0; n < 4; ++n) {
                            t.diffuse |= uint32_t(std::clamp(int(ch[0][n] + 0.5f), 0, 255)) << (n * 8);
                            t.specular |= uint32_t(std::clamp(int(ch[1][n] + 0.5f), 0, 255)) << (n * 8);
                        }
                        t.u0 = uv[0], t.v0 = uv[1], t.u1 = uv[2], t.v1 = uv[3];
                        pd.verts.push_back(t);
                        if (cuts < 2) {
                            std::copy(w, w + 3, cutAt[cuts]);
                            cutIndex[cuts++] = uint16_t(pd.verts.size() - 1);
                        }
                        return uint16_t(pd.verts.size() - 1);
                    };
                    for (int side = 0; side < 2; ++side) {
                        uint16_t piece[4];
                        for (int q = 0; q < corners[side]; ++q) piece[q] = vertexOf(sides[side][q]);
                        for (int q = 1; q + 1 < corners[side]; ++q) {  // a fan round its first corner
                            pd.idx.push_back(piece[0]);
                            pd.idx.push_back(piece[q]);
                            pd.idx.push_back(piece[q + 1]);
                        }
                    }
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
    // 3.0.4-beta.27: a minute's worth - how much was drawn again as it was, and why it was made anew
    if (o.minuteFrames >= 3600) {
        LOG_INFO("own ground: %s - the last %u pictures: %.0f %% drawn again as they were; made anew for the camera "
                 "%u, the ground's colours %u, their mean %u, the hills' light %u, the fog %u, the state %u, the "
                 "models' colours %u, the rest %u", g->map.c_str(), o.minuteFrames,
                 100.0 * double(o.minuteReused) / double(o.minuteFrames), o.anew[0], o.anew[1], o.anew[2], o.anew[3],
                 o.anew[4], o.anew[5], o.anew[6], o.anew[7]);
        o.minuteFrames = o.minuteReused = 0;
        std::fill(o.anew, o.anew + 8, 0u);
    }
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
    if (!g || !effects.ownFarModels || !o.modelLikeKnown || g->models.chunks.empty() || !o.up) return;
    o.learning.fit();
    const float from = own_ground::ownFrom(p);  // 3.0.4-beta.22: where the own ground's depth is squeezed
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
            farthest < from)
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
                if (zd[0] < from && zd[1] < from && zd[2] < from) continue;  // the game's own
                if (zd[0] >= from && zd[1] >= from && zd[2] >= from) {  // all of it: its corners as they are
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
                const int corners = own_ground::pastDepth(zd, from, share);
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

// 3.0.4-beta.20: a quad of the game's own water (on a map of lava, of lava: core/water.hpp) - its picture, the
// picture's state and its colour noted for the own water past the far plane; true when it is one
bool Renderer::Impl::noteOwnWater(const FrameRecord& rec, const DrawCmd& d) {
    OwnState& o = ownState;
    const OwnGround* g = mapWorld.ownGround();
    if (!g || g->serial != o.serial || g->water.chunks.empty()) return false;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + 4 * stride > vb.size()) return false;
    const uint8_t* at = vb.data() + d.vertexOffset;
    const bool strip = d.primitive == d3d7::PT_TRIANGLESTRIP;
    if (!(g->waterType == 4 ? water::isLavaQuad(at, stride, d.vertexCount, d.indexCount, strip)
                            : water::isWaterQuad(at, stride, d.vertexCount, d.indexCount, strip)))
        return false;
    o.waterTexture = d.texture[0];
    o.waterGame = true;
    std::memcpy(&o.waterColour, at + 16, sizeof o.waterColour);  // its first corner's diffuse (lava: the map's light)
    o.waterLike = rec.states()[d.state];
    o.waterLikeKnown = true;
    // 3.0.4-beta.23 (R-W7A5XC): its waves' phase from its corners' heights (core/water.hpp wavePhaseOf) - the far
    // water lifted alike meets it without a line
    if (g->waveHeight != 0.0f && viewToWorldValid) {
        const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
        const float* m = viewToWorld;
        const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
        int gx[4], gy[4];
        float h[4];
        bool seen = true;
        for (int k = 0; k < 4 && seen; ++k) {
            float v[4];
            std::memcpy(v, at + size_t(k) * stride, sizeof v);
            seen = v[3] > 0.0f;
            const float vz = 1.0f / v[3], vx = (v[0] - p.cx) * vz / p.f0, vy = (v[1] - p.cy) * vz / p.f1;
            const float x = (vx * m[0] + vy * m[3] + vz * m[6] + m[9]) * float(axes.x);
            h[k] = (vx * m[1] + vy * m[4] + vz * m[7] + m[10]) * float(axes.y);
            const float z = (vx * m[2] + vy * m[5] + vz * m[8] + m[11]) * float(axes.z);
            gx[k] = int(std::lround(x / g->zoom + float(g->width) / 2.0f));
            gy[k] = int(std::lround(z / g->zoom + float(g->height) / 2.0f));
        }
        if (seen) {
            o.wavePhase = water::wavePhaseOf(gx, gy, h, 4, g->wavePitch, g->waterLevel, g->waveHeight);
            o.wavePhaseKnown = true;
            o.wavePhaseAt = GetTickCount64();
        }
    }
    return true;
}

// 3.0.4-beta.24: the record's first quad of the game's own water (or lava) from draw `from` on, noted as above - the
// far water drawn at the start of the game's sorted list, before the quad itself comes
void Renderer::Impl::noteOwnWaterAhead(const FrameRecord& rec, size_t from) {
    const std::vector<DrawCmd>& draws = rec.draws();
    for (size_t k = from; k < draws.size() && k < from + 20000; ++k) {
        const DrawCmd& d = draws[k];
        const DrawSetup& s = rec.states()[d.state];
        if (s.ui) return;  // the world's draws end
        if (d.fvf == d3d7::FVF_TL1 && d.vertexCount == 4 && s.blend && s.srcBlend == d3d7::BLEND_SRCALPHA &&
            s.dstBlend == d3d7::BLEND_INVSRCALPHA && noteOwnWater(rec, d))
            return;
    }
}

// 3.0.4-beta.20 (the user chose «Своя земля везде»): the map's water past the game's far plane - the game lays its
// water a quad a cell with its ground (0x4e59d0), none past its far plane, and the own ground showed the bed bare
// there (460 of the client's 921 maps have water, 3 lava). The parts of the quads past it (own_ground::pastDepth, as
// the far models'), in the game's own water picture, state and colour, drawn as the game's water is drawn (the modern
// water's shaders by ownWater; its copy of the world taken here - after the characters, as before)
bool Renderer::Impl::drawOwnWater() {
    OwnState& o = ownState;
    o.waterDrawn = true;
    const OwnGround* g = mapWorld.ownGround();
    if (!g || g->serial != o.serial || g->water.chunks.empty()) return true;
    // its picture: the game's own water's this picture (the far water then the same as the near), else the map's own
    // copy of the one the game would show now (a picture every waterAnim ticks of 16 ms, FUN_004e3b80), else the last
    // of the game's; its state the game's water's, else its models' made the water's (texture by diffuse, blended)
    uint32_t texture = o.waterGame ? o.waterTexture : 0;
    if (!texture && !o.waterIds.empty() && o.up) {
        const uint64_t tick = GetTickCount64() / 16 / uint64_t(std::max(g->waterAnim, 1));
        texture = o.waterIds[size_t(tick % o.waterIds.size())];
    }
    if (!texture) texture = o.waterTexture;
    if (!o.waterLikeKnown && o.modelLikeKnown) {
        DrawSetup& w = o.waterLike;
        w = o.modelLike;
        w.blend = true;
        w.srcBlend = d3d7::BLEND_SRCALPHA;
        w.dstBlend = d3d7::BLEND_INVSRCALPHA;
        w.zWrite = w.alphaTest = w.colorKey = false;
        w.stage[0].colorOp = w.stage[0].alphaOp = d3d7::TOP_MODULATE;
        w.stage[0].colorArg1 = w.stage[0].alphaArg1 = d3d7::TA_TEXTURE;
        w.stage[0].colorArg2 = w.stage[0].alphaArg2 = d3d7::TA_DIFFUSE;
        w.stage[1] = StageSetup{};
        o.waterLikeKnown = true;
    }
    if (!texture || !o.waterLikeKnown || !textures.count(texture)) {
        if (!o.waterWaitLogged)
            LOG_INFO("own ground: %s's far %s waits for its picture (%zu of the map's own)", g->map.c_str(),
                     g->waterType == 4 ? "lava" : "water", o.waterIds.size());
        o.waterWaitLogged = true;
        return true;
    }
    own_ground::View view;
    if (!own_ground::viewOf(viewToWorld, view)) return true;
    crumb("own water");
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float from = own_ground::ownFrom(p);  // 3.0.4-beta.22: where the own ground's depth is squeezed
    const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
    const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
    const bool fogged = o.shading.fogged();
    const uint32_t plain = o.shading.specularAt(0.0f);
    // lava's colour the map's light as the game's lava quads have it (not seen yet: white, opaque)
    const uint32_t colour = g->waterType != 4 ? water::kDiffuse : o.waterColour ? o.waterColour : 0xFFFFFFFFu;
    o.waterRec.reset();
    DrawSetup st = o.waterLike;
    st.cull = d3d7::CULL_NONE;  // the axes may turn the quads over
    const uint32_t state = o.waterRec.addState(st);
    o.waterVerts.clear();
    o.waterIdx.clear();
    size_t triangles = 0;
    const auto emit = [&] {
        if (!o.waterIdx.empty()) {
            DrawCmd cmd;
            cmd.state = state;
            cmd.texture[0] = texture;
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL1;
            o.waterRec.draw(cmd, o.waterVerts.data(), uint32_t(o.waterVerts.size()), o.waterIdx.data(),
                            uint32_t(o.waterIdx.size()));
        }
        o.waterVerts.clear();
        o.waterIdx.clear();
    };
    // 3.0.4-beta.23: the waves' phase now - the game's of this picture, else on from the last seen at its pace
    int phase = o.wavePhaseKnown ? o.wavePhase : 0;
    if (o.wavePhaseKnown && !o.waterGame)
        phase = water::phaseAfter(o.wavePhase, g->waveSpeed, int((GetTickCount64() - o.wavePhaseAt) / 16));
    const terrain::Mesh& mesh = g->water;
    for (const terrain::Chunk& box : mesh.chunks) {
        float lo[3], hi[3];
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(box.lo[a] * sign[a], box.hi[a] * sign[a]);
            hi[a] = std::max(box.lo[a] * sign[a], box.hi[a] * sign[a]);
        }
        float nearest = 0.0f, farthest = 0.0f;
        if (!own_ground::boxInView(view, p, gameWidth, gameHeight, lo, hi, kMostDepth, nearest, &farthest) ||
            farthest < from)
            continue;  // none of it past the far plane: the game's own
        const terrain::Batch& bt = mesh.batches[box.firstBatch];
        for (uint32_t i = bt.firstIndex; i + 3 <= bt.firstIndex + bt.indexCount; i += 3) {
            const terrain::Vertex* tv[3];
            float c[3][3];
            for (int k = 0; k < 3; ++k) {
                tv[k] = &mesh.vertices[bt.firstVertex + mesh.indices[i + size_t(k)]];
                const int gx = int(std::lround(tv[k]->pos[0] / g->zoom + float(g->width) / 2.0f));
                const int gy = int(std::lround(tv[k]->pos[2] / g->zoom + float(g->height) / 2.0f));
                const float y = water::waveAt(gx, gy, g->wavePitch, phase, g->waterLevel, g->waveHeight);
                const float w[3] = {tv[k]->pos[0] * sign[0], y * sign[1], tv[k]->pos[2] * sign[2]};
                own_ground::toView(view, w, c[k]);
            }
            const float zd[3] = {c[0][2], c[1][2], c[2][2]};
            if (zd[0] < from && zd[1] < from && zd[2] < from) continue;  // the game's own
            float share[4][3];
            const int corners = own_ground::pastDepth(zd, from, share);
            if (corners < 3) continue;
            if (o.waterVerts.size() + 4 > 65535) emit();
            const size_t base = o.waterVerts.size();
            bool ok = true;
            for (int q = 0; q < corners && ok; ++q) {
                float at[3] = {0, 0, 0}, u = 0.0f, v = 0.0f;
                for (int k = 0; k < 3; ++k) {
                    for (int a = 0; a < 3; ++a) at[a] += share[q][k] * c[k][a];
                    u += share[q][k] * tv[k]->uv[0];
                    v += share[q][k] * tv[k]->uv[1];
                }
                own_ground::Tl2 placed;
                ok = own_ground::placeView(p, at, placed);
                own_ground::Tl1 t{};
                t.x = placed.x, t.y = placed.y, t.z = placed.z, t.rhw = placed.rhw;
                t.diffuse = colour;
                t.specular = fogged ? o.shading.specularAt(at[2]) : plain;
                t.u = u, t.v = v;
                o.waterVerts.push_back(t);
            }
            if (!ok) {
                o.waterVerts.resize(base);
                continue;
            }
            for (int q = 1; q + 1 < corners; ++q) {  // a fan round the first corner
                o.waterIdx.push_back(uint16_t(base));
                o.waterIdx.push_back(uint16_t(base + size_t(q)));
                o.waterIdx.push_back(uint16_t(base + size_t(q) + 1));
                ++triangles;
            }
        }
    }
    emit();
    o.waterTriangles = triangles;
    o.waterMost = std::max(o.waterMost, triangles);
    if (++o.waterFrames == 600)
        LOG_INFO("own ground: %s's far %s - %zu triangles past the far plane in the last picture, %zu at most",
                 g->map.c_str(), g->waterType == 4 ? "lava" : "water", triangles, o.waterMost);
    if (o.waterRec.draws().empty()) return true;
    flushDraws();  // the game's draws gathered so far are of its own vertices, uploaded before these
    prepareGeometry(o.waterRec);
    if (vertexBase) {
        ownPass = true;
        ownWater = g->waterType == 4 ? 2 : 1;
        for (const DrawCmd& cmd : o.waterRec.draws()) draw(o.waterRec, cmd);
        ownWater = 0;
        ownPass = false;
    }
    geometryList = 0;  // the game's record's vertices go up again with its next draw
    return !lost;
}

}  // namespace uagfx::gpu
