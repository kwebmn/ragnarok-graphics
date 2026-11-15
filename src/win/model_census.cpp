#include "win/model_census.hpp"

#include <windows.h>

#include <cstring>
#include <unordered_set>
#include <vector>

#include "core/game_models.hpp"
#include "core/model_census.hpp"
#include "core/view3d.hpp"
#include "win/context.hpp"
#include "win/dx7/ground_batch.hpp"
#include "win/entry_hook.hpp"
#include "win/log.hpp"
#include "win/view3d.hpp"

namespace uagfx::win::modelcensus {
namespace mc = ::uagfx::modelcensus;
namespace {

static_assert(sizeof(mc::FaceColours) == mc::kColourSize, "a face's colours and texture coordinates as the game's");
static_assert(sizeof(mc::FaceLit) == 36, "a face's vertices' ints as the game's (+0x144)");

using NodeDrawFn = void(__attribute__((thiscall)) *)(uint8_t* node, uint32_t a, uint32_t b, uint32_t c);
constexpr int kDeepest = 64;     // nodes in nodes
constexpr uint32_t kChecksAFrame = 4;
constexpr uint32_t kMostFaces = 65535;
constexpr DWORD kTellEveryMs = 30000;

struct Call {
    uint8_t* node = nullptr;
    uint32_t vector = 0, offset = 0, count = 0;
    bool grown = false;
};

uintptr_t g_draw = 0;
Call g_calls[kDeepest];
int g_depth = 0;
uint32_t g_nodes = 0, g_faces = 0, g_checks = 0;
LONGLONG g_ticks = 0, g_lastFrame = 0;
double g_perMs = 0;
mc::Tally g_tally;
DWORD g_toldAt = 0;
// 3.0.20-beta.3 (stage 2a): the nodes sent on this map, its serial; the captures this frame; for the log
std::unordered_set<uint32_t> g_sent;
uint32_t g_map = 1, g_captures = 0, g_animated = 0, g_sentNodes = 0, g_sentFaces = 0;
constexpr uint32_t kCapturesAFrame = 8, kMostVertices = 65535;
constexpr uint32_t kRotKeys = 0xF8, kPosKeys = 0x108, kActorAnimation = 0x1F0, kNodeTextureCount = 0x88,
                   kNodeTextures = 0x90, kTextureSurface = 0xC, kMeshVertexCount = 0, kMeshVertices = 0xC,
                   kFaceTexture = 0xC, kFaceTwoSided = 0x10, kViewMatrix = 0x94;

std::vector<mc::FaceColours> g_memory;
std::vector<mc::FaceLit> g_lits;
std::vector<mc::DrawnVertex> g_drawn;

LONGLONG now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

template <typename T>
T at(uintptr_t place) {
    T v;
    std::memcpy(&v, reinterpret_cast<const void*>(place), sizeof v);
    return v;
}

// the game's colouring of this node this frame - read before its draw, which clears the half see-through flag: the way,
// its three factors, the actor's alpha
mc::Colouring colouringOf(const uint8_t* node) {
    mc::Colouring c;
    if (at<int32_t>(mc::kOtherWay))
        c.way = mc::Way::Other;
    else if (at<int32_t>(uintptr_t(node) + mc::kNodeSeeThrough))
        c.way = mc::Way::SeeThrough;
    else if (at<uint8_t>(mc::kLitWay))
        c.way = mc::Way::Lit;
    const uintptr_t factors = c.way == mc::Way::Other ? mc::kOtherFactors : mc::kFactors;
    for (int k = 0; k < 3; ++k) c.factors[k] = at<float>(factors + uintptr_t(4 * k));
    const uintptr_t actor = at<uint32_t>(uintptr_t(node) + mc::kNodeActor);
    c.alpha = actor ? at<uint8_t>(actor + mc::kActorAlpha) : 0xFF;
    return c;
}

// the node's faces in memory against the vertices its draw wrote into its block of the pool (the block found again by
// its place from the pool's start: a child's growth may have moved the pool)
void check(const uint8_t* node, const Call& call, const mc::Colouring& how) {
    const uintptr_t mesh = at<uint32_t>(uintptr_t(node) + mc::kNodeMesh);
    if (!mesh) return;
    const uint32_t faces = at<uint32_t>(mesh + mc::kMeshFaces);
    const uint32_t first = at<uint32_t>(uintptr_t(node) + mc::kNodeColours);
    const uint32_t last = at<uint32_t>(uintptr_t(node) + mc::kNodeColours + 4);
    if (!faces || faces > kMostFaces || !first || last < first || (last - first) / mc::kColourSize != faces) return;
    g_memory.resize(faces);
    std::memcpy(g_memory.data(), reinterpret_cast<const void*>(uintptr_t(first)), size_t(faces) * mc::kColourSize);
    g_lits.clear();
    if (how.way == mc::Way::Lit) {  // 3.0.20-beta.2: each vertex's ints - its colour plus its place's light
        const uint32_t litFirst = at<uint32_t>(uintptr_t(node) + mc::kNodeLit);
        const uint32_t litLast = at<uint32_t>(uintptr_t(node) + mc::kNodeLit + 4);
        if (!litFirst || litLast < litFirst || (litLast - litFirst) / sizeof(mc::FaceLit) != faces) return;
        g_lits.resize(faces);
        std::memcpy(g_lits.data(), reinterpret_cast<const void*>(uintptr_t(litFirst)),
                    size_t(faces) * sizeof(mc::FaceLit));
    }
    const uint32_t begin = at<uint32_t>(call.vector + 4), end = at<uint32_t>(call.vector + 8);
    const uint64_t from = uint64_t(begin) + call.offset, to = from + uint64_t(call.count) * 32;
    if (!begin || to > end || call.count > kMostFaces * 3) return;
    g_drawn.resize(call.count);
    for (uint32_t i = 0; i < call.count; ++i) {
        const uintptr_t v = uintptr_t(from + uint64_t(i) * 32);
        g_drawn[i] = mc::DrawnVertex{at<uint32_t>(v + 16), at<float>(v + 24), at<float>(v + 28)};
    }
    g_tally.check(mc::compare(g_memory, g_lits, how, g_drawn.data(), g_drawn.size()), how.way);
}

void __attribute__((thiscall)) drawHook(uint8_t* node, uint32_t a, uint32_t b, uint32_t c) {
    if (g_depth >= kDeepest || !node) {
        reinterpret_cast<NodeDrawFn>(g_draw)(node, a, b, c);
        return;
    }
    const int depth = g_depth++;
    g_calls[depth] = Call{node};
    const mc::Colouring how = colouringOf(node);
    const LONGLONG start = depth == 0 ? now() : 0;
    reinterpret_cast<NodeDrawFn>(g_draw)(node, a, b, c);
    if (depth == 0) g_ticks += now() - start;
    g_depth = depth;
    ++g_nodes;
    const uintptr_t mesh = at<uint32_t>(uintptr_t(node) + mc::kNodeMesh);
    if (mesh) g_faces += at<uint32_t>(mesh + mc::kMeshFaces);
    if (g_calls[depth].grown && g_checks < kChecksAFrame) {
        ++g_checks;
        check(node, g_calls[depth], how);
    }
}

// a node the game moves (its keys of turning or moving, or its actor's own animation) - left to the game
bool animated(uintptr_t node) {
    const uint32_t rot0 = at<uint32_t>(node + kRotKeys), rot1 = at<uint32_t>(node + kRotKeys + 4);
    const uint32_t pos0 = at<uint32_t>(node + kPosKeys), pos1 = at<uint32_t>(node + kPosKeys + 4);
    if (rot0 && rot1 > rot0 && (rot1 - rot0) / 0x14 > 1) return true;
    if (pos0 && pos1 > pos0 && (pos1 - pos0) / 0x20 > 1) return true;
    const uintptr_t actor = at<uint32_t>(node + mc::kNodeActor);
    return actor && at<int32_t>(actor + kActorAnimation) != 0;
}

void capture(uintptr_t node, const float* points, const float* matrix, int count) {
    const uintptr_t view = view3dwin::lastView(), mesh = at<uint32_t>(node + mc::kNodeMesh);
    if (!view || !mesh || !points || !matrix) return;
    const uint32_t vertices = at<uint32_t>(mesh + kMeshVertexCount), faces = at<uint32_t>(mesh + mc::kMeshFaces);
    const uintptr_t faceList = at<uint32_t>(mesh + mc::kMeshFaceList);
    const uint32_t first = at<uint32_t>(node + mc::kNodeColours), last = at<uint32_t>(node + mc::kNodeColours + 4);
    const uint32_t litFirst = at<uint32_t>(node + mc::kNodeLit), litLast = at<uint32_t>(node + mc::kNodeLit + 4);
    const uint32_t textureCount = at<uint32_t>(node + kNodeTextureCount);
    const uintptr_t textureList = at<uint32_t>(node + kNodeTextures);
    if (uint32_t(count) != vertices || !vertices || vertices > kMostVertices || !faces || faces > kMostFaces ||
        !faceList || !first || (last - first) / mc::kColourSize != faces || !textureCount || textureCount > 255 ||
        !textureList)
        return;
    const bool lit = litFirst && litLast > litFirst && (litLast - litFirst) / sizeof(mc::FaceLit) == faces;
    float viewMatrix[12], inverse[12], world[12];
    std::memcpy(viewMatrix, reinterpret_cast<const void*>(view + kViewMatrix), sizeof viewMatrix);
    view3d::inverse(viewMatrix, inverse);
    gamemodels::worldOf(matrix, inverse, world);
    gamemodels::Node n;
    n.id = uint32_t(node);
    n.map = g_map;
    for (uint32_t t = 0; t < textureCount; ++t) {
        const uintptr_t texture = at<uint32_t>(textureList + 4 * t);
        n.textures.push_back(texture ? dx7::gpuTextureOf(reinterpret_cast<const void*>(
                                           uintptr_t(at<uint32_t>(texture + kTextureSurface))))
                                     : 0u);
    }
    n.vertices.resize(size_t(vertices) * 3);
    for (uint32_t v = 0; v < vertices; ++v) gamemodels::transform(world, points + 3 * v, &n.vertices[3 * v]);
    n.faces.resize(faces);
    for (uint32_t f = 0; f < faces; ++f) {
        gamemodels::Face& out = n.faces[f];
        const uintptr_t face = faceList + uintptr_t(f) * 0x18;
        for (int k = 0; k < 3; ++k) out.v[k] = at<uint16_t>(face + uintptr_t(2 * k));
        if (out.v[0] >= vertices || out.v[1] >= vertices || out.v[2] >= vertices) return;
        out.texture = uint8_t(at<uint16_t>(face + kFaceTexture) % textureCount);
        out.flags = at<int32_t>(face + kFaceTwoSided) ? gamemodels::kTwoSided : 0;
        const uintptr_t colours = first + uintptr_t(f) * mc::kColourSize;
        std::memcpy(out.colour, reinterpret_cast<const void*>(colours), sizeof out.colour);
        std::memcpy(out.uv, reinterpret_cast<const void*>(colours + 12), sizeof out.uv);
        if (lit) std::memcpy(out.lit, reinterpret_cast<const void*>(litFirst + uintptr_t(f) * 36), sizeof out.lit);
    }
    auto* r = Context::get().renderer;
    if (!r) return;
    r->addGameModel(n);
    g_sent.insert(uint32_t(node));
    ++g_sentNodes;
    g_sentFaces += faces;
}

}  // namespace

void noteProjection(const float* points, const float* matrix, int count) {
    if (!g_draw || g_depth <= 0 || count <= 0 || g_captures >= kCapturesAFrame) return;
    const uintptr_t node = uintptr_t(g_calls[g_depth - 1].node);
    if (!node || g_sent.count(uint32_t(node))) return;
    ++g_captures;
    if (animated(node)) {
        g_sent.insert(uint32_t(node));  // looked at once: left to the game
        ++g_animated;
        return;
    }
    capture(node, points, matrix, count);
}

void mapChanged() {
    g_sent.clear();
    ++g_map;
}

void install() {
    if (g_draw) return;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_perMs = f.QuadPart > 0 ? double(f.QuadPart) / 1000.0 : 0.0;
    if (!hookEntryBytes(mc::kNodeDraw, mc::kNodeDrawBytes, mc::kNodeDrawSize, reinterpret_cast<const void*>(&drawHook),
                        g_draw)) {
        LOG_INFO("models: the game's model node draw is not where this build of uaRO.exe has it - not measured");
        return;
    }
    LOG_INFO("models: the game's model node draw hooked (uaRO.exe 0x%X) - its time and faces, its colours checked "
             "against its memory", mc::kNodeDraw);
}

void noteGrow(uint32_t vector, uint32_t atPlace, uint32_t n) {
    if (!g_draw || g_depth <= 0) return;
    Call& top = g_calls[g_depth - 1];
    if (top.grown) return;  // a node's own first growth: its faces' block
    const uint32_t begin = at<uint32_t>(vector + 4);
    if (!begin || atPlace < begin) return;
    top.grown = true;
    top.vector = vector;
    top.offset = atPlace - begin;
    top.count = n;
}

void frame() {
    if (!g_draw) return;
    const LONGLONG t = now();
    const double frameMs = g_lastFrame && g_perMs > 0 ? double(t - g_lastFrame) / g_perMs : 0.0;
    g_lastFrame = t;
    if (g_nodes && frameMs > 0 && frameMs < 1000)
        g_tally.frame(mc::Frame{g_nodes, g_faces, g_perMs > 0 ? double(g_ticks) / g_perMs : 0.0}, frameMs);
    g_nodes = g_faces = g_checks = g_captures = 0;
    g_ticks = 0;
    const DWORD tick = GetTickCount();
    if (!g_toldAt) g_toldAt = tick;
    if (tick - g_toldAt < kTellEveryMs) return;
    g_toldAt = tick;
    if (const std::string line = g_tally.line(); !line.empty()) LOG_INFO("%s", line.c_str());
    g_tally.clear();
    if (g_sentNodes || g_animated)  // 3.0.20-beta.3
        LOG_INFO("models: %u static nodes captured as the game draws them (%u faces) and sent to the renderer, %u "
                 "moving ones left to the game", g_sentNodes, g_sentFaces, g_animated);
    g_sentNodes = g_sentFaces = g_animated = 0;
}

}  // namespace uagfx::win::modelcensus
