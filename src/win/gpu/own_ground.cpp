#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "core/own_ground.hpp"
#include "core/own_ground_card.hpp"
#include "core/quad_batch.hpp"
#include "core/trees.hpp"
#include "core/water.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr size_t kTexelsAFrame = size_t(1) << 19;  // pictures put up a frame (2 MB): the upload buffers stay small
constexpr int kAtlasStrip = 256;                    // the lightmaps' rows a piece
// smooth (and the strips between the textures) where a cell is 40 pixels or more on the screen (about 1000 at the
// game's 1080), the game's own cells past it - 3.0.4-beta.15: to 1500 before, much of the pass's 7.7 ms a frame on an
// i7-8700 (R-N6FC5V); 3.0.19-beta.20: core own_ground::fineDepthOf (the 3D view from 150, R-K2JPUU)
constexpr int kRelitAPicture = 8;  // 3.0.4-beta.28: chunks made again a picture when the hills' light moved
constexpr unsigned kWallsChecked = 32;  // 3.0.7-beta.3: the game's walls seen before the log tells how many are alike
// 3.0.20-beta.23: the own ground's vertices put up on the card a picture (2 MB of them, their blends)
constexpr uint32_t kGroundPiece = 65536;

// a ground's mesh by its number (ChunkJob::which): the smooth one, the plain one, (3.0.20-beta.30) the outskirts
const terrain::Mesh& meshOf(const OwnGround& g, int which) {
    return which == 2 ? g.outskirts : which ? g.distant : g.mesh;
}
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
    if (ownState.atlasSunless && textures.count(ownState.atlasSunless)) dying.push_back(ownState.atlasSunless);
    ownState.atlasSunless = 0;  // 3.0.20-beta.50
    ownState.atlasSunlessRow = 0;
    ownState.serial = 0;
    ownState.ids.clear();
    ownState.atlas = 0;
    ownState.texture = ownState.level = 0;
    ownState.atlasRow = 0;
    ownState.up = ownState.groundUp = false;
    ownState.refused = false;  // 3.0.18-beta.204
    ownState.frame = false;
    for (int m = 0; m < 3; ++m) {  // 3.0.4-beta.12
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
    ownState.sinceMade = 0;
    ownState.minuteFrames = ownState.minuteReused = 0;  // 3.0.4-beta.27
    std::fill(ownState.anew, ownState.anew + 8, 0u);
    if (groundCard) discard(groundCard);  // 3.0.20-beta.23: the card may still read it - let go with its list
    groundCard = nullptr;
    groundCardSerial = groundCardUp = 0;
    for (std::vector<uint8_t>& f : groundCardFresh) std::vector<uint8_t>().swap(f);
}

// At the picture's first ground draw of the game's: the map's own ground read (MapWorld), its textures made and their
// pictures put up - a few a frame, the full colours with their mipmaps (the AI textures know them by their 16-bit
// texels, as the game's), the lightmaps in strips. True: drawn this picture in place of the game's.
bool Renderer::Impl::prepareOwnGround() {
    mapWorld.setRoomWalls(effects.roomWalls3d);  // 3.0.20-beta.71: from the next map read
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
        o.wallCheck.reset(g->width, g->height, g->zoom);  // 3.0.7-beta.3
        o.wallApartDraw.clear();
        o.wallsTold = false;
        o.colours[0].assign(g->mesh.vertices.size(), 0);  // 3.0.4-beta.12: the colours made as the chunks show
        o.colours[1].assign(g->distant.vertices.size(), 0);
        o.colours[2].assign(g->outskirts.vertices.size(), 0);  // 3.0.20-beta.30
        o.fresh[0].assign(g->mesh.chunks.size(), 0);
        o.fresh[1].assign(g->distant.chunks.size(), 0);
        o.fresh[2].assign(g->outskirts.chunks.size(), 0);
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
        // 3.0.18-beta.204 (R-7XY449: the ground white): a texture of it or its lightmaps not made on the card (its
        // memory, its views) would be drawn white (a texture not there is) - the game's own ground on this map
        unsigned unmade = 0;
        for (size_t k = 0; k < g->textures.size(); ++k) unmade += !g->textures[k].levels.empty() && !o.ids[k];
        unmade += !g->atlas.argb.empty() && !o.atlas;
        o.refused = unmade > 0;
        if (o.refused)
            LOG_INFO("own ground: %s - %u of its pictures not made on the card (its memory?) - the game's own ground "
                     "on this map, not a white one", g->map.c_str(), unmade);
        for (const OwnGround::Picture& p : g->modelTextures)  // 3.0.4-beta.14
            o.modelIds.push_back(p.levels.empty() ? 0 : ownTexture(p.width, p.height, int(p.levels.size())));
        for (const OwnGround::Picture& p : g->waterPictures)  // 3.0.4-beta.20
            o.waterIds.push_back(p.levels.empty() ? 0 : ownTexture(p.width, p.height, int(p.levels.size())));
        o.learning.reset(g->models, g->toLight);
        o.modelsTurn = own_models::NotingTurn{};  // 3.0.4-beta.34: the map's own, from its first
    }
    if (o.refused) return false;  // 3.0.18-beta.204
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
    // 3.0.20-beta.38 (the owner's «6,7,8,9», 9 - the frame's work on more threads): noted now, worked out with the
    // frame's others at once on its threads (flushOwnNotes: when the own ground is drawn, at the frame's end)
    if (o.notesRec && o.notesRec != &rec) flushOwnNotes(nullptr);
    o.notesRec = &rec;
    o.notes.push_back(&d);
}

namespace {

// A ground cell's face (its `count` vertices from `v`): its corners in the file's places and the colours it gives them
// (the cell's first corner - its least x and z - its own, the others fill only), its vertices' fog; a wall (upright)
// apart, for the renderer's thread (its check logs). On any of the frame's threads (3.0.20-beta.38)
void ownCellOf(const own_ground::Tl2* v, uint32_t count, const DrawCmd& d, const camera::Projection& p, const float* m,
               const map::Axes& axes, bool walls, Renderer::Impl::OwnNotePart& out) {
    float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
    float at[8][2];
    const uint32_t n = std::min(count, 8u);
    for (uint32_t i = 0; i < count; ++i) {
        if (v[i].rhw <= 0.0f) return;
        const float vz = 1.0f / v[i].rhw, vx = (v[i].x - p.cx) * vz / p.f0, vy = (v[i].y - p.cy) * vz / p.f1;
        const float x = (vx * m[0] + vy * m[3] + vz * m[6] + m[9]) * float(axes.x);
        const float z = (vx * m[2] + vy * m[5] + vz * m[8] + m[11]) * float(axes.z);
        if (i < n) at[i][0] = x, at[i][1] = z;
        lo[0] = std::min(lo[0], x), hi[0] = std::max(hi[0], x);
        lo[1] = std::min(lo[1], z), hi[1] = std::max(hi[1], z);
        out.fog.add(vz, v[i].specular);
    }
    if (hi[0] - lo[0] < 1.0f || hi[1] - lo[1] < 1.0f) {  // a wall: upright
        if (!walls) return;
        Renderer::Impl::OwnNotePart::Wall w;
        w.d = &d;
        std::copy(lo, lo + 2, w.lo);
        std::copy(hi, hi + 2, w.hi);
        w.count = count;
        w.cornersCount = int(std::min(count, 4u));
        for (int i = 0; i < w.cornersCount; ++i) w.corners[i] = v[i].diffuse;
        out.walls.push_back(w);
        return;
    }
    for (uint32_t i = 0; i < n; ++i) {
        const bool first = at[i][0] - lo[0] < 0.5f && at[i][1] - lo[1] < 0.5f;
        out.corners.push_back(Renderer::Impl::OwnNotePart::Corner{at[i][0], at[i][1], v[i].diffuse, first});
    }
}

}  // namespace

// 3.0.20-beta.38: the frame's ground draws noted so far - their cells worked out in parts on the frame's threads, then
// in their order into the shading (the corners' colours, the fog) and the walls' check, as one by one before.
// outer: the part whose span it is in (its time taken out of it)
void Renderer::Impl::flushOwnNotes(LONGLONG* outer) {
    OwnState& o = ownState;
    if (o.notes.empty() || !o.notesRec) {
        o.notes.clear();
        return;
    }
    LONGLONG none = 0;
    CpuTimeWithin t(cpuPartTicks[kCpuOwnGroundNotes], outer ? *outer : none);
    const FrameRecord& rec = *o.notesRec;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const map::Axes axes = mapWorld.axes().known() ? mapWorld.axes() : map::Axes{1, 1, 1};
    const OwnGround* g = o.wallsTold ? nullptr : mapWorld.ownGround();
    const bool walls = g && g->serial == o.serial;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t n = o.notes.size(), chunks = std::min<size_t>(n, 64);
    if (o.noteParts.size() < chunks) o.noteParts.resize(chunks);
    parallelChunks(chunks, [&](size_t k) {
        OwnNotePart& part = o.noteParts[k];
        part.corners.clear();
        part.walls.clear();
        part.fog = own_ground::FogSums{};
        for (size_t i = n * k / chunks; i < n * (k + 1) / chunks; ++i) {
            const DrawCmd& d = *o.notes[i];
            if (d.vertexOffset + size_t(d.vertexCount) * sizeof(own_ground::Tl2) > vb.size()) continue;
            const own_ground::Tl2* all = reinterpret_cast<const own_ground::Tl2*>(vb.data() + d.vertexOffset);
            // 3.0.19-beta.50 (core/quad_batch): the driver's gathered cells - a cell each 4 vertices
            if (quadbatch::isBatch(d.vertexCount, d.indexCount)) {
                for (uint32_t q = 0; q + 4 <= d.vertexCount; q += 4)
                    ownCellOf(all + q, 4, d, p, viewToWorld, axes, walls, part);
            } else {
                ownCellOf(all, d.vertexCount, d, p, viewToWorld, axes, walls, part);
            }
        }
    });
    for (size_t k = 0; k < chunks; ++k) {
        const OwnNotePart& part = o.noteParts[k];
        o.shading.fogOf(part.fog);
        for (const OwnNotePart::Wall& w : part.walls) noteOwnWall(rec, w);
        for (const OwnNotePart::Corner& c : part.corners) o.shading.note(c.x, c.z, c.diffuse, c.first);
    }
    o.notes.clear();
}

// 3.0.7-beta.3 (R-YXZM7Y «посмотри насчет освещения стен»): a wall of the game's ground - its colour against the light
// the own ground gives it (terrain::wallLight); in the log once kWallsChecked walls of the map's are seen
void Renderer::Impl::noteOwnWall(const FrameRecord& rec, const OwnNotePart::Wall& w) {
    OwnState& o = ownState;
    const OwnGround* g = o.wallsTold ? nullptr : mapWorld.ownGround();
    if (!g || g->serial != o.serial) return;
    // 3.0.18-beta.5 (the owner: «засветление стен»): all its corners, and the first apart's stages
    const bool apartBefore = !o.wallCheck.firstApart().empty();
    // 3.0.20-beta.80: ours in the factors the game drew its walls with (camera::groundFactors)
    float factors[3];
    camera::groundFactors(factors);
    o.wallCheck.note(w.lo, w.hi, w.corners[0], g->wallLight, w.corners, w.cornersCount, factors);
    if (!apartBefore && !o.wallCheck.firstApart().empty()) {
        const DrawSetup& s = rec.states()[w.d->state];
        char text[160];
        std::snprintf(text, sizeof text, "; its draw: stage 0 op %u (%u, %u), stage 1 op %u (%u, %u), "
                      "%u vertices, blend %d %d/%d", s.stage[0].colorOp, s.stage[0].colorArg1,
                      s.stage[0].colorArg2, s.stage[1].colorOp, s.stage[1].colorArg1, s.stage[1].colorArg2,
                      w.count, int(s.blend), int(s.srcBlend), int(s.dstBlend));
        o.wallApartDraw = text;
    }
    if (o.wallCheck.seen() >= kWallsChecked) {
        o.wallsTold = true;
        const std::string& apart = o.wallCheck.firstApart();
        LOG_INFO("own ground: %s's walls - %u of the game's seen, %u lit as ours (a channel 2 apart at "
                 "most)%s%s%s",
                 g->map.c_str(), o.wallCheck.seen(), o.wallCheck.alike(),
                 apart.empty() ? "" : "; first apart: ", apart.c_str(), o.wallApartDraw.c_str());
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
namespace {

// 3.0.19-beta.46: Windows' own pool runs `body` for 0 .. count-1, this thread taking its share too; a few helpers -
// a quarter of the processor's threads, three at most - the renderer's other work and the game's own thread left room
struct ChunkRun {
    PTP_WORK work = nullptr;
    const std::function<void(size_t)>* body = nullptr;
    size_t count = 0;
    volatile LONG next = 0;
    int helpers = -1;
};

void runChunks(ChunkRun& r) {
    for (;;) {
        const LONG n = InterlockedIncrement(&r.next) - 1;
        if (n < 0 || size_t(n) >= r.count) return;
        (*r.body)(size_t(n));
    }
}

VOID CALLBACK chunkWork(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WORK) { runChunks(*static_cast<ChunkRun*>(context)); }

}  // namespace

void parallelChunks(size_t count, const std::function<void(size_t)>& body) {
    static ChunkRun r;  // the renderer's thread alone asks
    if (r.helpers < 0) {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        r.helpers = std::clamp(int(si.dwNumberOfProcessors) / 4, 0, 3);
        r.work = r.helpers > 0 ? CreateThreadpoolWork(&chunkWork, &r, nullptr) : nullptr;
        LOG_INFO("own ground: its chunks made on %d thread(s) a picture (%lu of the processor's)",
                 r.work ? r.helpers + 1 : 1, static_cast<unsigned long>(si.dwNumberOfProcessors));
    }
    r.body = &body;
    r.count = count;
    r.next = 0;
    const bool helped = r.work && count >= 16;  // a few chunks: this thread alone
    if (helped)
        for (int h = 0; h < r.helpers; ++h) SubmitThreadpoolWork(r.work);
    runChunks(r);
    if (helped) WaitForThreadpoolWorkCallbacks(r.work, FALSE);
}

// a chunk's colours, the game's round its corners (3.0.20-beta.23: apart - the card's way sends them as they are)
void Renderer::Impl::ownColours(const OwnMaking& m, const OwnState::ChunkJob& job) {
    OwnState& o = ownState;
    const OwnGround* g = m.g;
    const terrain::Mesh& mesh = meshOf(*g, job.which);
    const terrain::Chunk& ch = mesh.chunks[job.k];
    std::vector<uint32_t>& colours = o.colours[job.which];  // this chunk's vertices alone written
    for (uint32_t b = ch.firstBatch; b < ch.firstBatch + ch.batchCount; ++b) {
        const terrain::Batch& bt = mesh.batches[b];
        for (uint32_t v = bt.firstVertex; v < bt.firstVertex + bt.vertexCount; ++v) {
            const terrain::Vertex& tv = mesh.vertices[v];
            if (job.which == 2) {  // 3.0.20-beta.30: the outskirts - the edge's colour times their own slope's light
                // 3.0.20-beta.42 (the audit): the colour of the ground's corner it lies nearest to where the land
                // begins, the map's mean a few cells on (Shading::landColour; the vertex's blend names the corner) -
                // not the colour at the map's rectangle: a filler cell's grey there darkened the land to the horizon
                const uint32_t edge = o.shading.landColour(tv.blend & 0xFFFFFFu, float(tv.blend >> 24) / 16.0f);
                const float slope = float(tv.color & 0xFFu) / 127.5f;
                uint32_t c = edge & 0xFF000000u;
                for (int part = 0; part < 3; ++part) {
                    const float channel = float((edge >> (part * 8)) & 0xFF) * slope;
                    c |= uint32_t(std::clamp(int(channel + 0.5f), 0, 255)) << (part * 8);
                }
                colours[v] = c;
                continue;
            }
            // 3.0.7-beta.3 (R-YXZM7Y): a wall in the game's light on it (terrain::wallLight), not the ground's
            // beside it; 3.0.20-beta.80 (the owner: «засветление стен»): and in the factors the game draws its ground
            // with this moment (camera::groundFactors - the night, a dungeon's darkness), as its tops are in the
            // game's own corners' colours
            const bool wall = terrain::isWall(tv);
            uint32_t c =
                wall ? own_ground::byFactors(tv.color, o.wallFactors) : o.shading.diffuseAt(tv.pos[0], tv.pos[2]);
            if (o.lightOn && !wall) {  // 3.0.4-beta.26: the smooth hills' light; 3.0.4-beta.30 the corners' between
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
}

void Renderer::Impl::ownChunk(const OwnMaking& m, const OwnState::ChunkJob& job, OwnState::ChunkOut& out) {
    OwnState& o = ownState;
    const OwnGround* g = m.g;
    const camera::Projection& p = m.p;
    const terrain::Mesh& mesh = meshOf(*g, job.which);
    const terrain::Chunk& ch = mesh.chunks[job.k];
    const std::vector<uint32_t>& colours = o.colours[job.which];
    out.used = 0;
    if (job.recolour) ownColours(m, job);
    thread_local std::vector<uint8_t> behind;
    for (uint32_t b = ch.firstBatch; b < ch.firstBatch + ch.batchCount; ++b) {
        const terrain::Batch& bt = mesh.batches[b];
        if (bt.layer != terrain::kGround && !m.blends) continue;
        if (m.leftOut && job.which != 2 && bt.texture >= 0 && size_t(bt.texture) < m.leftOut->size() &&
            (*m.leftOut)[size_t(bt.texture)])
            continue;  // 3.0.20-beta.48: the makers' filler - the land's own there
        if (out.used == out.pieces.size()) out.pieces.emplace_back();
        OwnState::Piece& pd = out.pieces[out.used++];
        pd.layer = bt.layer;
        pd.tex = bt.texture >= 0 && size_t(bt.texture) < m.textures ? size_t(bt.texture) : m.textures;
        pd.vertexCount = bt.vertexCount;
        pd.indexCount = bt.indexCount;
        pd.verts.assign(bt.vertexCount, own_ground::Tl2{});
        pd.idx.clear();
        const size_t base = 0;  // its own, from 0: pending's place added as it goes in
        behind.assign(bt.vertexCount, 0);
        // the strips a little towards the camera: on the ground's own points, the bands near them (as the seams')
        const float lift = bt.layer < 3 ? own_ground::card::kLift[bt.layer] : 0.0f;
        for (uint32_t i = 0; i < bt.vertexCount; ++i) {  // 3.0.20-beta.23: in core, as ground_vs puts them
            own_ground::Tl2& t = pd.verts[base + i];
            if (!own_ground::card::putVertex(m.putting, mesh.vertices[bt.firstVertex + i], job.which == 0, bt.layer,
                                             t)) {
                behind[i] = 1;
                continue;
            }
            t.diffuse = colours[bt.firstVertex + i];
        }
        for (uint32_t i = bt.firstIndex; i + 3 <= bt.firstIndex + bt.indexCount; i += 3) {
            const uint32_t a = mesh.indices[i], b2 = mesh.indices[i + 1], c = mesh.indices[i + 2];
            if (behind[a] || behind[b2] || behind[c]) continue;  // across the near plane: left out
            const uint32_t tri[3] = {a, b2, c};
            const float zd[3] = {1.0f / pd.verts[base + a].rhw, 1.0f / pd.verts[base + b2].rhw,
                                 1.0f / pd.verts[base + c].rhw};
            const bool nearer = zd[0] < m.from || zd[1] < m.from || zd[2] < m.from;
            if (!nearer || (zd[0] < m.from && zd[1] < m.from && zd[2] < m.from)) {
                for (uint32_t k = 0; k < 3; ++k) pd.idx.push_back(uint16_t(base + tri[k]));
                continue;
            }
            // 3.0.4-beta.23 (R-W7A5XC): across the squeeze - its near side and its far side, each one depth
            // (own_ground::nearDepth), the two points of the cut shared
            float sides[2][4][3];
            const int corners[2] = {own_ground::nearDepth(zd, m.from, sides[0]),
                                    own_ground::pastDepth(zd, m.from, sides[1])};
            float cutAt[2][3];
            uint16_t cutIndex[2] = {0, 0};
            int cuts = 0;
            const auto vertexOf = [&](const float* w) {
                for (int k = 0; k < 3; ++k)
                    if (w[k] == 1.0f) return uint16_t(base + tri[k]);
                for (int k = 0; k < cuts; ++k)
                    if (w[0] == cutAt[k][0] && w[1] == cutAt[k][1] && w[2] == cutAt[k][2]) return cutIndex[k];
                float at[3] = {0, 0, 0}, chn[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}}, uv[4] = {0, 0, 0, 0};
                for (int k = 0; k < 3; ++k) {
                    const own_ground::Tl2& sv = pd.verts[base + tri[k]];
                    at[0] += w[k] * (sv.x - p.cx) * zd[k] / p.f0;
                    at[1] += w[k] * (sv.y - p.cy) * zd[k] / p.f1;
                    at[2] += w[k] * zd[k];
                    for (int n = 0; n < 4; ++n) {
                        chn[0][n] += w[k] * float((sv.diffuse >> (n * 8)) & 0xFF);
                        chn[1][n] += w[k] * float((sv.specular >> (n * 8)) & 0xFF);
                    }
                    uv[0] += w[k] * sv.u0, uv[1] += w[k] * sv.v0, uv[2] += w[k] * sv.u1, uv[3] += w[k] * sv.v1;
                }
                own_ground::Tl2 t{};
                own_ground::placeView(p, at, t);
                if (lift > 0.0f) t.z = own_ground::depthOf(p, std::max(at[2] - lift, p.nearZ + 0.005f));
                for (int n = 0; n < 4; ++n) {
                    t.diffuse |= uint32_t(std::clamp(int(chn[0][n] + 0.5f), 0, 255)) << (n * 8);
                    t.specular |= uint32_t(std::clamp(int(chn[1][n] + 0.5f), 0, 255)) << (n * 8);
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
    flushOwnNotes(&cpuPartTicks[kCpuOwnGround]);  // 3.0.20-beta.38: the game's ground noted so far, worked out
    o.shading.fit();
    // the corners the game gave another colour: the chunks round them made again
    bool meanMoved = false;
    o.shading.takeChanged(o.changed, meanMoved);
    o.cornersChanged += o.changed.size();  // 3.0.4-beta.15: for the log
    // 3.0.18-beta.189 (R-VHMGXR «словил глюк с берсрком»: the ground and the walls washed pink-white, the models not):
    // the game's corners' mean when it jumps - the light the game gives its ground taken from it - at most 20 a map
    if (const uint32_t mean = o.shading.meanColour(); o.meanTold == 0) {
        o.meanTold = mean;
    } else if (own_ground::channelsApart(mean, o.meanTold) > 24 && o.meanJumpsTold < 20) {
        ++o.meanJumpsTold;
        LOG_INFO("own ground: %s - the game's corners' mean #%08X, #%08X before (%zu corners given another colour now)",
                 g->map.c_str(), mean, o.meanTold, o.changed.size());
        o.meanTold = mean;
    }
    if (meanMoved) {
        for (std::vector<uint8_t>& f : o.fresh) std::fill(f.begin(), f.end(), uint8_t(0));
    } else {
        // 3.0.20-beta.42: a corner of the ground's edge the land past it takes its colour from - the land's chunks
        // made again, a few a picture (as in a moved light)
        bool edgeChanged = false;
        for (uint32_t c : o.changed) edgeChanged = edgeChanged || (c < g->edgeCorners.size() && g->edgeCorners[c]);
        if (edgeChanged)
            for (uint8_t& f : o.fresh[2]) f = f ? uint8_t(2) : uint8_t(0);
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
    // 3.0.20-beta.80 (the owner: «засветление стен» - «иногда случается, иногда нет»): the factors the game draws its
    // ground with moved (the night coming, a dungeon's own) - the walls made again in them, a few chunks a picture as
    // in a moved light
    {
        float factors[3];
        camera::groundFactors(factors);
        bool moved = false;
        for (int k = 0; k < 3; ++k) moved = moved || std::fabs(factors[k] - o.wallFactors[k]) > 0.004f;
        if (moved) {
            std::copy(factors, factors + 3, o.wallFactors);
            for (std::vector<uint8_t>& f : o.fresh)
                for (uint8_t& c : f) c = c ? uint8_t(2) : uint8_t(0);
        }
    }
    const bool blends = effects.groundSeams && mapOutdoors;  // outdoors, as the game's ground's seams (seamsWanted)
    // 3.0.4-beta.15: the record of the last picture again when nothing it was made of changed
    // 3.0.20-beta.23 (the owner's «берем», item 1): the own ground from the card once all of it is up there - a piece
    // more put up each picture until then, the processor's way meanwhile
    const bool onCard = ensureGroundCard(*g);
    OwnState::Made now;
    std::memcpy(now.view, viewToWorld, sizeof now.view);
    now.width = gameWidth, now.height = gameHeight, now.serial = o.serial, now.blends = blends;
    // 3.0.4-beta.20: the colours learnt anew, not the triangles matched - those grew every picture (R-35N5QN: 0 %)
    // 3.0.4-beta.27: the models once their pictures are up (o.up)
    now.models = effects.ownFarModels && o.up, now.seamsView = seamsView, now.matched = o.learning.changes();
    now.fog[0] = o.shading.specularAt(100.0f), now.fog[1] = o.shading.specularAt(1000.0f);
    now.like = o.like;
    // 3.0.20-beta.4: the game's still models drawn by us, the frame's; 3.0.20-beta.11: none in the record while the
    // card draws them (drawGameModelsCard after it) - the record kept over their changes
    now.gameModels = cardModelsReady() ? 0 : gameFrameKey();
    now.onCard = onCard;
    // 3.0.20-beta.50 (the trial of real shadows): its light without the painted shadows while ours are drawn
    const bool sunless = sunShadowsReady(*g);
    now.sunless = sunless;
    sunlessDrawn = sunless;
    now.valid = true;
    // 3.0.4-beta.32 (R-Q3W727): the models' colours learnt anew at most every kModelsColoursEvery pictures
    const bool same = o.made.valid && !meanMoved && !lightMoved && !o.lightPending && o.changed.empty() &&
                      std::memcmp(o.made.view, now.view, sizeof now.view) == 0 && o.made.width == now.width &&
                      o.made.height == now.height && o.made.serial == now.serial && o.made.blends == now.blends &&
                      o.made.models == now.models && o.made.seamsView == now.seamsView &&
                      !own_ground::modelsColoursDue(o.made.matched, now.matched, o.sinceMade) &&
                      o.made.fog[0] == now.fog[0] && o.made.fog[1] == now.fog[1] && o.made.like == now.like &&
                      o.made.gameModels == now.gameModels && o.made.onCard == now.onCard &&
                      o.made.sunless == now.sunless;
    size_t chunks = o.madeChunks, vertices = o.madeVertices, triangles = o.madeTriangles;
    if (same) ++o.reused, ++o.minuteReused, o.sinceMade = std::min(o.sinceMade + 1, 1 << 20);
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
        o.sinceMade = 0;
        chunks = vertices = triangles = 0;
        o.rec.reset();
        o.drawLayer.clear();
        o.drawCard.clear();  // 3.0.20-beta.23
        o.drawBits.clear();
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
        // 3.0.9-beta.3 (R-6JBVTG: magenta strips along Izlude's bridge and wall): a texture with the game's
        // see-through magenta (keyed away when read, OwnGround::Picture::keyed) - its clear texels cut, as the game's
        // colour key drops them
        DrawSetup ks = st;
        ks.colorKey = true;
        ks.alphaTest = true;
        ks.alphaFunc = d3d7::CMP_GREATEREQUAL;
        ks.alphaRef = 0x80;
        const uint32_t state = o.rec.addState(st), blendState = o.rec.addState(bs), keyedState = o.rec.addState(ks);
        // 3.0.20-beta.20: a layer's texture's draw - its state, its pictures (the record's room made by place below)
        // 3.0.20-beta.71 (core/room_blocks): a house's blocks dressed in their rooms' wall pictures show them in
        // the 3D view alone - from above their faces take the makers' black they had
        const bool dressedShown = camera::wideView();
        const auto drawOf = [&](uint8_t layer, size_t t) {
            DrawCmd cmd;
            const bool keyed = t < g->textures.size() && g->textures[t].keyed;
            cmd.state = layer == terrain::kGround ? (keyed ? keyedState : state) : blendState;
            const size_t shown = !dressedShown && t < g->fromAbove.size() && g->fromAbove[t] >= 0
                                     ? size_t(g->fromAbove[t])
                                     : t;
            cmd.texture[0] = shown < o.ids.size() ? o.ids[shown] : 0;
            cmd.keyed[0] = keyed;
            cmd.texture[1] = sunless ? o.atlasSunless : o.atlas;  // 3.0.20-beta.50: no painted shadows under ours
            cmd.primitive = d3d7::PT_TRIANGLELIST;
            cmd.fvf = d3d7::FVF_TL2;
            return cmd;
        };
        const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
        const float fineDepth = own_ground::fineDepthOf(g->zoom, p.f1, camera::wideView());
        int relit = 0;  // 3.0.4-beta.28: chunks made again in the moved light this picture
        o.lightPending = false;
        const float from = own_ground::ownFrom(p);  // 3.0.4-beta.23: the triangles across it cut there
        const float morphFrom = fineDepth * kMorphFrom;  // 3.0.4-beta.29: the smooth ground eased from here on
        const float mostDepth = own_ground::mostDepthOf(camera::wideView(), camera::sightNow());  // .21, .24
        // 3.0.19-beta.46 (the owner's «делай все»: 2.7 ms of the renderer's thread a frame on the fields in the 3D view
        // - some 100000 vertices projected, coloured and cut a chunk at a time): the chunks in view chosen in turn
        // (their light's turns as before), made on a few threads at once (ownChunk: each its own chunks), then put
        // into the draws in the chunks' order - the same draws as before
        std::vector<OwnState::ChunkJob>& jobs = o.jobs;
        jobs.clear();
        // 3.0.20-beta.30 (the owner's «6,7,8,9», 6): in the 3D view on a map under the sky the land past its edges too
        // (core/outskirts.hpp) - its chunks after the map's, the third mesh
        const bool outskirts = camera::wideView() && mapOutdoors && !g->outskirts.chunks.empty();
        // 3.0.20-beta.48 (the audit's question 3): where the land is drawn the makers' filler round the ground is
        // left out - the land lies there (a black ring between the ground and it until now)
        // 3.0.20-beta.71: and from above the sides made for a house's dressed blocks - the makers left none there
        const std::vector<uint8_t>* leftOut = outskirts && !g->fillerTextures.empty() ? &g->fillerTextures
                                              : !dressedShown && !g->madeTextures.empty() ? &g->madeTextures
                                                                                           : nullptr;
        const size_t mapChunks = g->mesh.chunks.size();
        for (size_t n = 0; n < mapChunks + (outskirts ? g->outskirts.chunks.size() : 0); ++n) {
            const bool past = n >= mapChunks;
            const size_t k = past ? n - mapChunks : n;
            const terrain::Chunk& box = past ? g->outskirts.chunks[k] : g->mesh.chunks[k];
            float lo[3], hi[3];
            for (int a = 0; a < 3; ++a) {
                lo[a] = std::min(box.lo[a] * sign[a], box.hi[a] * sign[a]);
                hi[a] = std::max(box.lo[a] * sign[a], box.hi[a] * sign[a]);
            }
            float nearest = 0.0f, farthest = 0.0f;
            if (!own_ground::boxInView(view, p, gameWidth, gameHeight, lo, hi, mostDepth, nearest, &farthest))
                continue;
            const int which = past ? 2 : nearest > fineDepth && k < g->distant.chunks.size() ? 1 : 0;
            ++chunks;
            uint8_t& fresh = o.fresh[which][k];
            if (fresh == 2) {  // 3.0.4-beta.28: in the light before it moved - a few a picture
                if (relit < kRelitAPicture)
                    ++relit, fresh = 0;
                else
                    o.lightPending = true;
            }
            const bool recolour = !fresh;  // its colours, the game's round its corners (ownChunk)
            if (recolour) {
                ++o.chunksMade;
                fresh = 1;
                if (k < groundCardFresh[which].size()) groundCardFresh[which][k] = 0;  // 3.0.20-beta.23: sent anew
            }
            jobs.push_back(OwnState::ChunkJob{k, which, recolour, nearest, farthest});
        }
        if (o.outs.size() < jobs.size()) o.outs.resize(jobs.size());
        OwnMaking making;
        making.g = g;
        making.view = view;
        making.p = p;
        making.axes = axes;
        making.fineDepth = fineDepth;
        making.morphFrom = morphFrom;
        making.from = from;
        making.blends = blends;
        making.textures = o.ids.size();
        making.leftOut = leftOut;
        making.putting.view = view;
        making.putting.p = p;
        making.putting.axes = axes;
        making.putting.morphFrom = morphFrom;
        making.putting.fineDepth = fineDepth;
        making.putting.fogA = o.shading.fogA();  // 255 and 0 when not fogged: specularAt(0) everywhere, as before
        making.putting.fogB = o.shading.fogB();
        making.putting.fogRgb = o.shading.specularRgb();
        uint32_t recoloured = 0;  // 3.0.20-beta.18: for the minute's line
        for (const OwnState::ChunkJob& job : jobs) recoloured += job.recolour ? 1u : 0u;
        if (recoloured > o.minuteMostRecoloured) o.minuteMostRecoloured = recoloured;
        o.recolouredNow = recoloured;
        if (onCard) {
            // 3.0.20-beta.23 (the owner's «берем», item 1 - «Своя земля» on the card): the chunks' colours made on the
            // threads and sent, then their indices by layer, texture and side of the squeeze into the record's draws
            // (ten a vertex) - ground_vs puts their vertices
            {
                CpuTimeWithin t(cpuPartTicks[kCpuOwnChunks], cpuPartTicks[kCpuOwnGround]);
                parallelChunks(jobs.size(), [&](size_t n) {
                    if (jobs[n].recolour) ownColours(making, jobs[n]);
                });
            }
            CpuTimeWithin putting(cpuPartTicks[kCpuOwnDraws], cpuPartTicks[kCpuOwnGround]);
            flushDraws();  // the draws gathered so far before the colours' copies
            for (const OwnState::ChunkJob& job : jobs)
                if (job.recolour || job.k >= groundCardFresh[job.which].size() || !groundCardFresh[job.which][job.k])
                    sendGroundColours(*g, job.which, job.k);
            groundLists.begin(o.ids.size(), blends, leftOut);
            for (const OwnState::ChunkJob& job : jobs) {
                const terrain::Mesh& mesh = meshOf(*g, job.which);
                groundLists.add(mesh, job.which, job.k, own_ground::card::hasNear(job.nearest, from),
                                own_ground::card::hasPast(job.farthest, from));
                vertices += mesh.chunks[job.k].vertexCount;
            }
            const std::vector<own_ground::card::Draw>& planned = groundLists.plan();
            const uint32_t turned = (axes.x < 0 ? 8u : 0u) | (axes.y < 0 ? 16u : 0u) | (axes.z < 0 ? 32u : 0u);
            o.cardRooms.clear();
            for (const own_ground::card::Draw& dr : planned) {
                uint32_t n = (dr.count + 9) / 10;
                if (n == 4) n = 5;  // never a quad's four: draw() takes those for sprites and effects
                const uint32_t room = o.rec.drawRoom(drawOf(dr.layer, dr.texture), n, 0);
                o.cardRooms.push_back(room);
                o.drawLayer.push_back(dr.layer);
                o.drawCard.resize(size_t(room) + 1, 0u);
                o.drawBits.resize(size_t(room) + 1, 0u);
                o.drawCard[room] = dr.count;
                o.drawBits[room] = uint32_t(dr.layer) | (dr.past ? 4u : 0u) | turned;
                triangles += dr.count / 3;
            }
            uint8_t* into = o.rec.vertexData();
            const std::vector<DrawCmd>& recDraws = o.rec.draws();
            groundLists.fill(
                groundLayout,
                [&](size_t k) { return reinterpret_cast<uint32_t*>(into + recDraws[o.cardRooms[k]].vertexOffset); },
                parallelChunks);
            o.cardMorph[0] = morphFrom, o.cardMorph[1] = fineDepth;
            putting.stop();
        } else {
            {  // 3.0.20-beta.18: the parts' own times
                CpuTimeWithin t(cpuPartTicks[kCpuOwnChunks], cpuPartTicks[kCpuOwnGround]);
                parallelChunks(jobs.size(), [&](size_t n) { ownChunk(making, jobs[n], o.outs[n]); });
            }
            CpuTimeWithin putting(cpuPartTicks[kCpuOwnDraws], cpuPartTicks[kCpuOwnGround]);
            // 3.0.20-beta.20 (the owner's «делай 1 и 2», 2): the pieces placed in turn - a layer's texture's draw
            // takes them in the chunks' order, a new one when the next would pass 65535 vertices (its vertices, and
            // two more for each triangle cut where the depth is squeezed, as before)
            o.batches.clear();
            o.places.clear();
            for (std::vector<uint32_t>& c : o.currentBatch) c.assign(o.ids.size() + 1, ~0u);
            for (size_t n = 0; n < jobs.size(); ++n) {
                const OwnState::ChunkOut& out = o.outs[n];
                for (size_t q = 0; q < out.used; ++q) {
                    const OwnState::Piece& pc = out.pieces[q];
                    if (pc.layer >= 3 || pc.tex >= o.currentBatch[pc.layer].size() || pc.idx.empty()) continue;
                    uint32_t& b = o.currentBatch[pc.layer][pc.tex];
                    if (b == ~0u || o.batches[b].vertices + pc.vertexCount + pc.indexCount / 3 * 2 > 65535) {
                        b = uint32_t(o.batches.size());
                        o.batches.push_back(OwnState::Batch{pc.layer, uint32_t(pc.tex), 0, 0, 0});
                    }
                    OwnState::Batch& bt = o.batches[b];
                    o.places.push_back(OwnState::PiecePlace{uint32_t(n), uint32_t(q), b, bt.vertices, bt.indices});
                    bt.vertices += uint32_t(pc.verts.size());
                    bt.indices += uint32_t(pc.idx.size());
                }
            }
            // the draws' rooms in the record: the ground, then the own bands, then the neighbours' strips, each layer's
            // textures in their order, a texture's draws in theirs
            o.batchOrder.resize(o.batches.size());
            for (uint32_t k = 0; k < o.batchOrder.size(); ++k) o.batchOrder[k] = k;
            std::stable_sort(o.batchOrder.begin(), o.batchOrder.end(), [&o](uint32_t x, uint32_t y) {
                const OwnState::Batch &a = o.batches[x], &b = o.batches[y];
                return a.layer != b.layer ? a.layer < b.layer : a.tex < b.tex;
            });
            for (const uint32_t k : o.batchOrder) {
                OwnState::Batch& bt = o.batches[k];
                bt.draw = o.rec.drawRoom(drawOf(bt.layer, bt.tex), bt.vertices, bt.indices);
                o.drawLayer.push_back(bt.layer);
                vertices += bt.vertices;
                triangles += bt.indices / 3;
            }
            // then each piece copied into its place on the own ground's threads (every room made: none moves now)
            uint8_t* intoVertices = o.rec.vertexData();
            uint16_t* intoIndices = o.rec.indexData();
            const std::vector<DrawCmd>& recDraws = o.rec.draws();
            parallelChunks(o.places.size(), [&](size_t k) {
                const OwnState::PiecePlace& at = o.places[k];
                const OwnState::Piece& pc = o.outs[at.job].pieces[at.piece];
                const DrawCmd& d = recDraws[o.batches[at.batch].draw];
                std::memcpy(intoVertices + d.vertexOffset + size_t(at.vertex) * sizeof(own_ground::Tl2),
                            pc.verts.data(), pc.verts.size() * sizeof(own_ground::Tl2));
                uint16_t* to = intoIndices + d.firstIndex + at.index;
                for (size_t i = 0; i < pc.idx.size(); ++i) to[i] = uint16_t(at.vertex + pc.idx[i]);
            });
            putting.stop();
        }
        {
            CpuTimeWithin t(cpuPartTicks[kCpuFarModels], cpuPartTicks[kCpuOwnGround]);
            drawOwnModels(view, p, axes);  // 3.0.4-beta.14: the models past the far plane, into the same record
        }
        // 3.0.20-beta.4: the ones the driver left out (the processor's way, while the card's is missing)
        if (!cardModelsReady()) {  // 3.0.20-beta.16: their own part of the drawing's time
            CpuTimeWithin t(cpuPartTicks[kCpuGameModels], cpuPartTicks[kCpuOwnGround]);
            drawGameModels(view, p, o.rec, &o.drawLayer, o.shading);
        }
        o.madeChunks = chunks, o.madeVertices = vertices, o.madeTriangles = triangles;
    }
    const double built = msSince(start);
    if (!same && built > o.minuteSlowest) {  // 3.0.20-beta.18
        o.minuteSlowest = built;
        o.minuteSlowestRecoloured = o.recolouredNow;
    }
    if (!o.rec.draws().empty()) {
        CpuTimeWithin uploading(cpuPartTicks[kCpuOwnUpload], cpuPartTicks[kCpuOwnGround]);  // 3.0.20-beta.18
        flushDraws();  // the game's draws gathered so far are of its own vertices, uploaded before these
        prepareGeometry(o.rec);
        if (vertexBase) {
            ownPass = true;
            // 3.0.20-beta.23: the draws from the card read its vertices (t6), with this picture's fog and easing
            const bool fromCard = std::any_of(o.drawCard.begin(), o.drawCard.end(), [](uint32_t n) { return n != 0; });
            if (fromCard && groundCard) {
                transition(groundCard, groundCardState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                cardFog = &o.shading;
                cardGroundMorph[0] = o.cardMorph[0], cardGroundMorph[1] = o.cardMorph[1];
            }
            for (size_t k = 0; k < o.rec.draws().size(); ++k) {
                seamPass = o.drawLayer[k] == terrain::kOwnBand || o.drawLayer[k] == terrain::kNeighbour;
                seamOwnBand = o.drawLayer[k] == terrain::kOwnBand;
                cardGroundDraw = groundCard && k < o.drawCard.size() && o.drawCard[k] != 0;
                cardGroundCount = cardGroundDraw ? o.drawCard[k] : 0;
                cardGroundBits = cardGroundDraw ? o.drawBits[k] : 0;
                groundCardDraws += cardGroundDraw ? 1 : 0;
                draw(o.rec, o.rec.draws()[k]);  // layer 3: the models (as the game's own models are drawn)
            }
            if (fromCard) flushDraws();  // the last one from the card gathered with its constants
            groundCardFrames += fromCard ? 1 : 0;
            cardGroundDraw = false;
            cardGroundCount = cardGroundBits = 0;
            if (fromCard) cardFog = nullptr;
            seamPass = seamOwnBand = false;
            ownPass = false;
            // 3.0.20-beta.4: the game's still models drawn here when it leaves them
            gameModelsNow.drawn(o.modelLikeKnown);
            if (!gameModelsNow.frame().empty() && !cardModelsReady()) {  // 3.0.20-beta.7: for the log
                noteGameCasters();  // 3.0.20-beta.12: the reflections and the torches' shadows take them
                ++gameDrawnFrames;
                gameDrawnTriangles += o.gameTriangles;
                gameDrawnNodes += o.gameNodes;
                gameDrawnUnknown += o.gameUnknown;
                gameDrawnMoving += gameMovingPlaced;  // 3.0.20-beta.14
            }
        }
        geometryList = 0;  // the game's record's vertices go up again with its next draw
    }
    {  // 3.0.20-beta.11: from the card, every picture (3.0.20-beta.16: its own time)
        CpuTimeWithin t(cpuPartTicks[kCpuGameModels], cpuPartTicks[kCpuOwnGround]);
        if (cardModelsReady()) drawGameModelsCard(o.shading);
        noteGameModelsForFar();  // 3.0.20-beta.18: the far models learn their colours from ours
    }
    {  // 3.0.20-beta.13: their leaves as the game's trees (3.0.20-beta.16: their own time)
        CpuTimeWithin t(cpuPartTicks[kCpuGameLeaves], cpuPartTicks[kCpuOwnGround]);
        drawGameLeaves(o.shading);
    }
    probeModels(o.shading);  // 3.0.20-beta.61: the models' dump looks at the picture (a report's frame alone)
    ++o.frames;
    o.slowest = std::max(o.slowest, built);
    o.most = std::max(o.most, vertices);
    if (o.frames == 1 || o.frames == 600)  // the first picture, then ten seconds' or so worst
        LOG_INFO("own ground: %s - %zu chunks of %zu in view, %zu draws, %zu vertices, %zu triangles, %.2f ms on the "
                 "CPU (frame %u: at most %zu vertices, %.2f ms; a frame %.1f corners given another colour, %.2f "
                 "chunks' colours made again; %.0f %% of the pictures drawn again as they were); strips between the "
                 "textures %s; %s",
                 g->map.c_str(), chunks, g->mesh.chunks.size(), o.rec.draws().size(), vertices, triangles, built,
                 o.frames, o.most, o.slowest, double(o.cornersChanged) / double(o.frames),
                 double(o.chunksMade) / double(o.frames), 100.0 * double(o.reused) / double(o.frames),
                 blends ? "on" : "off",
                 onCard                ? "its vertices put by the card (ground_vs)"
                 : !groundOnCard()     ? "its vertices put by the processor (the card's way off)"
                 : groundCardSerial == g->serial && groundCard ? "its vertices put by the processor while going up"
                                                               : "its vertices put by the processor");
    // 3.0.17-beta.19 (R-7XY449: Prontera's ground white, its stones twice their colour - 242 for 120 - the rest of the
    // picture as it is): what the ground's colour is made of, the first picture and ten seconds on
    if (o.frames == 1 || o.frames == 600) {
        float lo = 1.0f, hi = 1.0f;
        if (!o.cornerLight.empty()) {
            const auto mm = std::minmax_element(o.cornerLight.begin(), o.cornerLight.end());
            lo = *mm.first, hi = *mm.second;
        }
        const StageSetup& s0 = o.like.stage[0];
        const StageSetup& s1 = o.like.stage[1];
        LOG_INFO("own ground: %s's colours - the game's corners' mean #%08X, its specular #%06X, stage 0 op %u (%u, "
                 "%u), stage 1 op %u (%u, %u), the hills' light %s %.2f-%.2f", g->map.c_str(),
                 o.shading.meanColour(), o.shading.specularAt(0.0f) & 0xFFFFFFu, s0.colorOp, s0.colorArg1,
                 s0.colorArg2, s1.colorOp, s1.colorArg1, s1.colorArg2, o.lightOn ? "on" : "off", lo, hi);
    }
    // 3.0.4-beta.27: a minute's worth - how much was drawn again as it was, and why it was made anew
    if (o.minuteFrames >= 3600) {
        // 3.0.18-beta.189: and the corners' mean, the fog's alpha 100 and 1000 units away and the state's ops now
        LOG_INFO("own ground: %s - the last %u pictures: %.0f %% drawn again as they were; made anew for the camera "
                 "%u, the ground's colours %u, their mean %u, the hills' light %u, the fog %u, the state %u, the "
                 "models' colours %u, the rest %u; now the corners' mean #%08X, the fog %u-%u, stage ops %u/%u; the "
                 "slowest made in %.2f ms (%u chunks given their colours anew in it, %u at most in a picture); from "
                 "the card in %u of the pictures, %.1f draws a picture",
                 g->map.c_str(), o.minuteFrames, 100.0 * double(o.minuteReused) / double(o.minuteFrames), o.anew[0],
                 o.anew[1], o.anew[2], o.anew[3], o.anew[4], o.anew[5], o.anew[6], o.anew[7], o.shading.meanColour(),
                 o.shading.specularAt(100.0f) >> 24, o.shading.specularAt(1000.0f) >> 24, o.like.stage[0].colorOp,
                 o.like.stage[1].colorOp, o.minuteSlowest, o.minuteSlowestRecoloured, o.minuteMostRecoloured,
                 groundCardFrames, groundCardFrames ? double(groundCardDraws) / double(groundCardFrames) : 0.0);
        groundCardFrames = groundCardDraws = 0;
        o.minuteFrames = o.minuteReused = 0;
        o.minuteSlowest = 0;
        o.minuteSlowestRecoloured = o.minuteMostRecoloured = 0;
        std::fill(o.anew, o.anew + 8, 0u);
    }
    // 3.0.4-beta.14: whether the own models stand where the game's do - of the game's model triangles near, how many
    // lay where one of ours does (all but the moving ones: placed as the game places them)
    if (o.frames == 600 && effects.ownFarModels)
        LOG_INFO("own models: %s - of the game's model triangles near %zu noted, %zu (%.0f %%) where ours lie; %zu "
                 "triangles of ours past the far plane in the last picture; noted in turn: all of them %u times, "
                 "up to %u a picture",
                 g->map.c_str(), o.learning.seen(), o.learning.matched(),
                 o.learning.seen() ? 100.0 * double(o.learning.matched()) / double(o.learning.seen()) : 0.0,
                 o.modelsDrawn, o.modelsTurn.rounds, o.modelsTurn.most);
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
    using own_models::kMostNoted;  // a picture's: the learning goes on over the pictures, in turn (3.0.4-beta.33)
    own_models::NotingTurn& turn = o.modelsTurn;
    if (turn.noted >= kMostNoted) return;
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
    if (turn.passed + triangles <= turn.from) {  // 3.0.4-beta.33: before this picture's turn
        turn.passed += triangles;
        return;
    }
    for (uint32_t t = 0; t < triangles && turn.noted < kMostNoted; ++t) {
        if (turn.passed++ < turn.from) continue;
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
        ++turn.noted;
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
    const float mostDepth = own_ground::mostDepthOf(camera::wideView(), camera::sightNow());  // .21, .24
    for (const terrain::Chunk& box : mesh.chunks) {
        float lo[3], hi[3];
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(box.lo[a] * sign[a], box.hi[a] * sign[a]);
            hi[a] = std::max(box.lo[a] * sign[a], box.hi[a] * sign[a]);
        }
        float nearest = 0.0f, farthest = 0.0f;
        if (!own_ground::boxInView(view, p, gameWidth, gameHeight, lo, hi, mostDepth, nearest, &farthest) ||
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
    const float mostDepth = own_ground::mostDepthOf(camera::wideView(), camera::sightNow());  // .21, .24
    for (const terrain::Chunk& box : mesh.chunks) {
        float lo[3], hi[3];
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(box.lo[a] * sign[a], box.hi[a] * sign[a]);
            hi[a] = std::max(box.lo[a] * sign[a], box.hi[a] * sign[a]);
        }
        float nearest = 0.0f, farthest = 0.0f;
        if (!own_ground::boxInView(view, p, gameWidth, gameHeight, lo, hi, mostDepth, nearest, &farthest) ||
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
    // 3.0.20-beta.31 (the owner's «6,7,8,9», 6 and 7): in the 3D view the sea past the map's wet edges
    // (outskirts::seaFrame, four quads round the map) at every depth - the game has none there: each triangle cut at
    // the near plane, then where the depth is squeezed (one depth each piece, as the own ground's); its picture every
    // four cells, as the game's water's
    if (camera::wideView() && mapOutdoors && g->seaFrame.size() >= 9) {
        const std::vector<float>& frame = g->seaFrame;
        const float tile = 1.0f / (4.0f * g->zoom);
        // the depths its triangles are cut at, ascending: where the depth is squeezed, where the fog begins and ends
        float seaCuts[own_ground::kMostDepthCuts] = {from, 0.0f, 0.0f, 0.0f};
        int seaCutCount = 1;
        if (fogged && std::fabs(o.shading.fogB()) > 1e-6f)
            for (const float alpha : {255.0f, 0.0f}) {
                const float at = (alpha - o.shading.fogA()) / o.shading.fogB();
                if (std::isfinite(at) && at > p.nearZ + 1.0f && at < 1.0e6f) seaCuts[seaCutCount++] = at;
            }
        for (int a = 1; a < seaCutCount; ++a)  // three at most: put in order by hand
            for (int b = a; b > 0 && seaCuts[b] < seaCuts[b - 1]; --b) std::swap(seaCuts[b], seaCuts[b - 1]);
        for (size_t t = 0; t + 9 <= frame.size(); t += 9) {
            float c[3][3], f[3][3];
            for (int k = 0; k < 3; ++k) {
                for (int a = 0; a < 3; ++a) f[k][a] = frame[t + size_t(k) * 3 + size_t(a)];
                const float w[3] = {f[k][0] * sign[0], f[k][1] * sign[1], f[k][2] * sign[2]};
                own_ground::toView(view, w, c[k]);
            }
            const float zd[3] = {c[0][2], c[1][2], c[2][2]};
            float front[4][3];
            const int n = own_ground::pastDepth(zd, p.nearZ + 0.5f, front);  // the part in front of the near plane
            for (int fan = 1; fan + 1 < n; ++fan) {
                const float* shares[3] = {front[0], front[fan], front[fan + 1]};
                float pc[3][3], pf[3][3];
                for (int k = 0; k < 3; ++k)
                    for (int a = 0; a < 3; ++a) {
                        pc[k][a] = shares[k][0] * c[0][a] + shares[k][1] * c[1][a] + shares[k][2] * c[2][a];
                        pf[k][a] = shares[k][0] * f[0][a] + shares[k][1] * f[1][a] + shares[k][2] * f[2][a];
                    }
                const float pz[3] = {pc[0][2], pc[1][2], pc[2][2]};
                // 3.0.20-beta.42 (the audit): cut where the depth is squeezed and where the fog's line begins and
                // ends too - across so large a triangle the fog went straight between its corners' clamped ends
                own_ground::DepthPiece pieces[own_ground::kMostDepthPieces];
                const int made = own_ground::cutByDepths(pz, seaCuts, seaCutCount, pieces);
                for (int piece = 0; piece < made; ++piece) {
                    const own_ground::DepthPiece& part = pieces[piece];
                    const int m = part.corners;
                    if (m < 3) continue;
                    if (o.waterVerts.size() + own_ground::kMostPieceCorners > 65535) emit();
                    const size_t base = o.waterVerts.size();
                    bool ok = true;
                    for (int q = 0; q < m && ok; ++q) {
                        float at[3] = {0, 0, 0}, place[3] = {0, 0, 0};
                        for (int k = 0; k < 3; ++k)
                            for (int a = 0; a < 3; ++a) {
                                at[a] += part.share[q][k] * pc[k][a];
                                place[a] += part.share[q][k] * pf[k][a];
                            }
                        own_ground::Tl2 placed;
                        ok = own_ground::placeView(p, at, placed);
                        own_ground::Tl1 v{};
                        v.x = placed.x, v.y = placed.y, v.z = placed.z, v.rhw = placed.rhw;
                        v.diffuse = colour;
                        v.specular = fogged ? o.shading.specularAt(at[2]) : plain;
                        v.u = place[0] * tile, v.v = place[2] * tile;
                        o.waterVerts.push_back(v);
                    }
                    if (!ok) {
                        o.waterVerts.resize(base);
                        continue;
                    }
                    for (int q = 1; q + 1 < m; ++q) {
                        o.waterIdx.push_back(uint16_t(base));
                        o.waterIdx.push_back(uint16_t(base + size_t(q)));
                        o.waterIdx.push_back(uint16_t(base + size_t(q) + 1));
                        ++triangles;
                    }
                }
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

// 3.0.20-beta.23 (the owner's «берем», item 1 - «Своя земля» on the card): the map's own ground made on the card - a
// default heap's buffer of its Layout, its vertices and their blends put up kGroundPiece a picture through this list
// (the upload ring's room), its colours sent a chunk at a time as they are made (sendGroundColours); true once all of
// it is up. A buffer the card cannot make: the processor's way from then on
bool Renderer::Impl::ensureGroundCard(const OwnGround& g) {
    namespace card = own_ground::card;
    if (!groundOnCard() || !open) return false;
    if (groundCardSerial != g.serial) {
        if (groundCard) discard(groundCard);  // the card may still read the last map's: let go with its list
        groundCard = nullptr;
        groundCardSerial = g.serial;
        groundCardUp = 0;
        groundCardPictures = 0;
        groundLayout = card::layoutOf(card::Meshes{&g.mesh, &g.distant, &g.outskirts});
        groundCardFresh[0].assign(g.mesh.chunks.size(), 0);
        groundCardFresh[1].assign(g.distant.chunks.size(), 0);
        groundCardFresh[2].assign(g.outskirts.chunks.size(), 0);  // 3.0.20-beta.30
        if (!groundLayout.all) return false;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = UINT64(groundLayout.size()) * 16;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const HRESULT hr =
            device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                            nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&groundCard));
        if (groundCardSrv == kNoSrv) groundCardSrv = allocSrv();
        if (FAILED(hr) || !groundCard || groundCardSrv == kNoSrv) {
            LOG_ERROR("own ground: no buffer on the card for %s's %u vertices (0x%08lX) - placed by the processor from "
                      "now on",
                      g.map.c_str(), groundLayout.all, static_cast<unsigned long>(hr));
            if (groundCard) groundCard->Release();
            groundCard = nullptr;
            groundCardFailed = true;
            return false;
        }
        nameResource(groundCard, "own ground on the card");
        groundCardState = D3D12_RESOURCE_STATE_COPY_DEST;
        D3D12_SHADER_RESOURCE_VIEW_DESC v{};
        v.Format = DXGI_FORMAT_R32G32B32A32_UINT;
        v.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        v.Buffer.NumElements = groundLayout.size();
        device->CreateShaderResourceView(groundCard, &v, cpuSrvHandle(groundCardSrv));
    }
    if (!groundCard) return false;
    if (groundCardUp >= groundLayout.all) return true;
    const uint32_t count = std::min(groundLayout.all - groundCardUp, kGroundPiece);
    const UINT64 vertexBytes = UINT64(count) * card::kVertexWords * 4;
    const Upload u = allocUpload(vertexBytes + UINT64(count) * 4, 16);
    if (!u.cpu) return false;
    uint32_t* words = reinterpret_cast<uint32_t*>(u.cpu);
    const card::Meshes meshes{&g.mesh, &g.distant, &g.outskirts};
    card::vertexWords(meshes, groundCardUp, count, words);
    card::blendWords(meshes, groundCardUp, count, words + size_t(count) * card::kVertexWords);
    flushDraws();  // the draws gathered so far before the copies' barriers
    transition(groundCard, groundCardState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(groundCard, UINT64(groundCardUp) * card::kVertexWords * 4, slots[slot].upload, u.offset,
                           vertexBytes);
    list->CopyBufferRegion(groundCard, UINT64(groundLayout.blendsAt()) * 16 + UINT64(groundCardUp) * 4,
                           slots[slot].upload, u.offset + vertexBytes, UINT64(count) * 4);
    groundCardUp += count;
    ++groundCardPictures;
    if (groundCardUp < groundLayout.all) return false;
    LOG_INFO("own ground: %s on the card - %u vertices (%u of the smooth ground), %.1f MB, put up in %d picture(s); "
             "ground_vs puts them",
             g.map.c_str(), groundLayout.all, groundLayout.fine, double(groundLayout.size()) * 16.0 / 1048576.0,
             groundCardPictures);
    return true;
}

// 3.0.20-beta.23: a chunk's colours (OwnState::colours, made by ownColours) into the card's buffer
void Renderer::Impl::sendGroundColours(const OwnGround& g, int which, size_t chunk) {
    const terrain::Mesh& mesh = meshOf(g, which);
    if (!groundCard || chunk >= mesh.chunks.size() || chunk >= groundCardFresh[which].size()) return;
    const terrain::Chunk& ch = mesh.chunks[chunk];
    const std::vector<uint32_t>& colours = ownState.colours[which];
    if (!ch.vertexCount || size_t(ch.firstVertex) + ch.vertexCount > colours.size()) return;
    const UINT64 bytes = UINT64(ch.vertexCount) * 4;
    const Upload u = allocUpload(bytes, 16);
    if (!u.cpu) return;
    std::memcpy(u.cpu, colours.data() + ch.firstVertex, size_t(bytes));
    const uint32_t first = groundLayout.baseOf(which) + ch.firstVertex;
    transition(groundCard, groundCardState, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(groundCard, UINT64(groundLayout.coloursAt()) * 16 + UINT64(first) * 4, slots[slot].upload,
                           u.offset, bytes);
    groundCardFresh[which][chunk] = 1;
}

}  // namespace uagfx::gpu
