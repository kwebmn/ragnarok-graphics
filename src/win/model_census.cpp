#include "win/model_census.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/game_models.hpp"
#include "core/game_models_card.hpp"
#include "core/model_census.hpp"
#include "core/view3d.hpp"
#include "win/context.hpp"
#include "win/dx7/ground_batch.hpp"
#include "win/entry_hook.hpp"
#include "win/game_camera.hpp"
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
    bool flipped = false;  // 3.0.20-beta.4: the draw's second argument (its actor's scale mirrored)
    const float* parent = nullptr;  // 3.0.20-beta.5: its first - the parent's matrix (the actor's x the view's)
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
uint32_t g_map = 1, g_captures = 0, g_animated = 0, g_sentNodes = 0, g_sentFaces = 0, g_under = 0, g_alike = 0;
constexpr uint32_t kCapturesAFrame = 8, kMostVertices = 65535;
// 3.0.20-beta.5: a node's keys of moving (+0xE8: 16 bytes - the frame, x y z; FUN_00401520) and turning (+0xF8: 20 -
// the frame, a quaternion; FUN_00401D30), each the first and the last of a vector; its +0x108 (32 bytes, FUN_00402090)
// holds the one default its setup FUN_004DF080 puts in
constexpr uint32_t kMoveKeys = 0xE8, kTurnKeys = 0xF8, kNodeTextureCount = 0x88,
                   kNodeTextures = 0x90, kTextureSurface = 0xC, kMeshVertexCount = 0, kMeshVertices = 0xC,
                   kFaceTexture = 0xC, kFaceTwoSided = 0x10, kViewMatrix = 0x94;

// 3.0.20-beta.4 (stage 2b): the still nodes captured (the renderer has them) with what placed and coloured them then -
// the mesh, its own matrix (+0xB4), its parent's in the world (3.0.20-beta.5), its colours' and lights' lists
// (+0x118, +0x144: the lights worked out by FUN_004DF930 -> FUN_004E0070 from the map's light grid) and the first
// face's of both; [view3d] own_models, the frame's nodes left out of the game's draw and how many of them were told
// 3.0.20-beta.5 (R-UQPC3E, the code read again): the actor's matrix gave way to the parent's in the world (the draw's
// first argument times the view's inverse) - a part hung on a moving node moves with it, its own matrix does not
struct Still {
    uint32_t mesh = 0, colours = 0, lits = 0;
    uint32_t colour0[3] = {};
    int32_t lit0[9] = {};
    float local[12] = {}, place[12] = {};
    bool placed = false;
};
std::unordered_map<uint32_t, Still> g_still;
// 3.0.20-beta.14 (stage 4): the moving nodes captured (their own vertices) with their mesh and colour lists then - the
// frame's moving nodes left out of the game's draw with the node-to-view matrices its draw makes, how many told
struct Moving {
    uint32_t mesh = 0, colours = 0, lits = 0;
};
std::unordered_map<uint32_t, Moving> g_moving;
std::vector<uint32_t> g_frameMoving;
std::vector<float> g_frameMatrices;
size_t g_toldMoving = 0;
uint32_t g_oursMoving = 0;
// 3.0.20-beta.22: «Записати моделі навколо» - armed for the next whole frame (Next), that frame's (Now): the game's own
// nodes within kDumpReach of the player, ours counted
enum class Dump { None, Next, Now };
Dump g_dump = Dump::None;
uint32_t g_dumpFrame = 0;  // 3.0.20-beta.65: the game's number of the dump's frame (0: not noted yet)
struct DumpNode {
    uint32_t id = 0, vertices = 0, faces = 0;
    bool flipped = false;
    // 3.0.20-beta.44 (R-HBPQ54 «зникли частини мосту»): why the game draws it itself - its actor's alpha (under 255:
    // the game fades a model over the player to 0x5A and draws it see-through), the way it is coloured
    uint8_t alpha = 0xFF;
    mc::Way way = mc::Way::Plain;
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
};
std::vector<DumpNode> g_dumpGame;
// 3.0.20-beta.55 (R-4PJUBK): the nodes the game drew itself in the dump's frame - against each model's root node
std::unordered_set<uint32_t> g_dumpDrawn;
uint32_t g_dumpOurs = 0, g_dumpOursMoving = 0, g_dumpGameAll = 0;
bool g_dumpTold = false;
float g_dumpAt[2] = {0, 0};
int g_dumpCell[2] = {0, 0}, g_dumpSize[2] = {0, 0};
constexpr float kDumpReach = 150.0f;  // the world's units: 30 cells
constexpr size_t kDumpMost = 60;
bool g_on = false;
std::vector<uint32_t> g_frameIds;
size_t g_told = 0;
uint32_t g_ours = 0, g_oursFrames = 0, g_keptBlended = 0, g_moved = 0;
using MatrixFn = void(__attribute__((thiscall)) *)(float* out, const void* local, const void* parent);
constexpr uint32_t kNodeMatrix = 0x409720, kNodeLocal = 0xB4, kNodeChildren = 0xAC;
// a texture of the game's with its own alpha (CTexture +0x10 format 1, A4R4G4B4): its faces go to the game's blended
// list (FUN_00412020 with 1) - such a node stays the game's; the game's draw inside a rectangle of the screen
// (0x7735EC, the faces whole within the renderer's +0x58..+0x64) likewise
constexpr uint32_t kTextureFormat = 0x10, kFormatAlpha = 1, kInsideRect = 0x7735EC;
// 3.0.20-beta.5: the view's inverse, worked out again when the view's matrix (CView +0x94) changes
uintptr_t g_viewAt = 0;
float g_viewSeen[12] = {}, g_viewInverse[12] = {};

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

// 3.0.20-beta.4: a node left out of the game's draw - its matrix by the game's own (FUN_00409720: thiscall on the
// out 4 x 3, its local +0xB4 and the parent's) and its children drawn as the game's draw does (its list +0xAC, each
// child's vtable slot 0 with the matrix and the two passed on); its half see-through flag cleared as the draw does
void drawChildren(uint8_t* node, uint32_t parent, uint32_t b, uint32_t c, float* toView = nullptr) {
    float own[12];
    float* m = toView ? toView : own;  // 3.0.20-beta.14: a moving node's - the frame's matrix for it
    const void* from = reinterpret_cast<const void*>(uintptr_t(parent));
    reinterpret_cast<MatrixFn>(uintptr_t(kNodeMatrix))(m, node + kNodeLocal, from);
    const uintptr_t head = at<uint32_t>(uintptr_t(node) + kNodeChildren);
    for (uintptr_t e = head ? at<uint32_t>(head) : 0; e && e != head; e = at<uint32_t>(e)) {
        uint8_t* child = reinterpret_cast<uint8_t*>(uintptr_t(at<uint32_t>(e + 8)));
        const uintptr_t table = child ? at<uint32_t>(uintptr_t(child)) : 0;
        if (table) reinterpret_cast<NodeDrawFn>(uintptr_t(at<uint32_t>(table)))(child, uint32_t(uintptr_t(m)), b, c);
    }
    std::memset(node + mc::kNodeSeeThrough, 0, 4);
}

const float* viewInverse() {
    const uintptr_t view = view3dwin::lastView();
    if (!view) return nullptr;
    float now[12];
    std::memcpy(now, reinterpret_cast<const void*>(view + kViewMatrix), sizeof now);
    if (view != g_viewAt || std::memcmp(now, g_viewSeen, sizeof now) != 0) {
        std::memcpy(g_viewSeen, now, sizeof now);
        view3d::inverse(now, g_viewInverse);
        g_viewAt = view;
    }
    return g_viewInverse;
}

Still stillOf(uintptr_t node, const float* parent) {
    Still s;
    s.mesh = at<uint32_t>(node + mc::kNodeMesh);
    s.colours = at<uint32_t>(node + mc::kNodeColours);
    s.lits = at<uint32_t>(node + mc::kNodeLit);
    if (s.colours && at<uint32_t>(node + mc::kNodeColours + 4) > s.colours)
        std::memcpy(s.colour0, reinterpret_cast<const void*>(uintptr_t(s.colours)), sizeof s.colour0);
    if (s.lits && at<uint32_t>(node + mc::kNodeLit + 4) > s.lits)
        std::memcpy(s.lit0, reinterpret_cast<const void*>(uintptr_t(s.lits)), sizeof s.lit0);
    std::memcpy(s.local, reinterpret_cast<const void*>(node + kNodeLocal), sizeof s.local);
    const float* inverse = viewInverse();
    if (parent && inverse) {
        gamemodels::compose(parent, inverse, s.place);
        s.placed = true;
    }
    return s;
}

bool same(const Still& a, const Still& b) {
    return a.mesh == b.mesh && a.colours == b.colours && a.lits == b.lits &&
           !std::memcmp(a.colour0, b.colour0, sizeof a.colour0) && !std::memcmp(a.lit0, b.lit0, sizeof a.lit0) &&
           !std::memcmp(a.local, b.local, sizeof a.local) && a.placed && b.placed &&
           gamemodels::samePlace(a.place, b.place);
}

// whether this node is the renderer's this frame: [view3d] own_models (3.0.20-beta.9: the view from above too - the
// owner's «делай все сразу»), the renderer's word, a still node it has, placed as when it was captured (else the
// game's from now on), coloured the plain or lit way (not half see-through, not the other way), its actor not fading
bool ours(const uint8_t* node, const mc::Colouring& how, const float* parent) {
    if (!g_on || how.alpha != 0xFF || (how.way != mc::Way::Plain && how.way != mc::Way::Lit) ||
        at<int32_t>(kInsideRect))
        return false;
    const auto it = g_still.find(uint32_t(uintptr_t(node)));
    if (it == g_still.end()) return false;
    if (!same(it->second, stillOf(uintptr_t(node), parent))) {
        g_still.erase(it);
        ++g_moved;
        return false;
    }
    auto* r = Context::get().renderer;
    return r && r->gameModelsLately();
}

// 3.0.20-beta.14 (stage 4): a moving node is the renderer's this frame as a still one (the switch, the way, its actor
// not fading, the renderer's word) while its mesh and colour lists are those of its capture - the game lights a node
// once, at the map's setup (FUN_004DF930 through the vtable's slot 2), and rebuilds its own matrix (+0xB4) from its
// keys each frame before the actor's draw (FUN_004FA8B0 -> FUN_004E0940 -> FUN_004DE950): placed by the frame's matrix
bool oursMoving(const uint8_t* node, const mc::Colouring& how) {
    // so many a frame as the batches' places hold (core/game_models_card kCardSlotShift): the rest the game's
    if (!g_on || how.alpha != 0xFF || (how.way != mc::Way::Plain && how.way != mc::Way::Lit) ||
        at<int32_t>(kInsideRect) || g_frameMoving.size() >= gamemodels::kCardMostMoving)
        return false;
    const auto it = g_moving.find(uint32_t(uintptr_t(node)));
    if (it == g_moving.end()) return false;
    const uintptr_t n = uintptr_t(node);
    const Moving& was = it->second;
    if (was.mesh != at<uint32_t>(n + mc::kNodeMesh) || was.colours != at<uint32_t>(n + mc::kNodeColours) ||
        was.lits != at<uint32_t>(n + mc::kNodeLit)) {
        g_moving.erase(it);
        ++g_moved;
        return false;
    }
    auto* r = Context::get().renderer;
    return r && r->gameModelsLately();
}

void __attribute__((thiscall)) drawHook(uint8_t* node, uint32_t a, uint32_t b, uint32_t c) {
    if (g_depth >= kDeepest || !node) {
        reinterpret_cast<NodeDrawFn>(g_draw)(node, a, b, c);
        return;
    }
    // 3.0.20-beta.65: the dump's frame's own number, as the game holds it while it draws that frame's models - by the
    // dump's end (the present) the game has counted on: R-64G2U9's lines told «did not come to it this frame» of
    // every model, the ones it had just tested too (their marks the frame before the number read then)
    if (g_dump == Dump::Now && !g_dumpFrame) {
        uint32_t renderer = 0;  // uaRO.exe's renderer (0x6EE3D8), its frame's number at +0x34 (dumpActors' own)
        if (readGame(0x6EE3D8, &renderer, 4) && renderer) readGame(renderer + 0x34, &g_dumpFrame, 4);
    }
    const mc::Colouring how = colouringOf(node);
    // 3.0.20-beta.4: the renderer draws it; its children as the game would
    if (ours(node, how, reinterpret_cast<const float*>(uintptr_t(a)))) {
        g_frameIds.push_back(uint32_t(uintptr_t(node)));
        ++g_ours;
        if (g_dump == Dump::Now) ++g_dumpOurs;  // 3.0.20-beta.22
        drawChildren(node, a, b, c);
        return;
    }
    if (oursMoving(node, how)) {  // 3.0.20-beta.14: its matrix as the game's draw makes it, into the frame's list
        float m[12];
        // on the stack while its children are drawn: one not captured yet is captured as moving (hung on it)
        const int depth = g_depth++;
        g_calls[depth] = Call{node};
        g_calls[depth].flipped = (b & 0xFF) != 0;
        g_calls[depth].parent = reinterpret_cast<const float*>(uintptr_t(a));
        drawChildren(node, a, b, c, m);
        g_depth = depth;
        g_frameMoving.push_back(uint32_t(uintptr_t(node)));
        g_frameMatrices.insert(g_frameMatrices.end(), m, m + 12);
        ++g_oursMoving;
        if (g_dump == Dump::Now) ++g_dumpOurs, ++g_dumpOursMoving;  // 3.0.20-beta.22
        return;
    }
    const int depth = g_depth++;
    g_calls[depth] = Call{node};
    g_calls[depth].flipped = (b & 0xFF) != 0;
    g_calls[depth].parent = reinterpret_cast<const float*>(uintptr_t(a));
    if (g_dump == Dump::Now) g_dumpDrawn.insert(uint32_t(uintptr_t(node)));  // 3.0.20-beta.55
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

// whether a node's keys of one kind move it: the values of its keys (`stride` bytes each, the value from 4 on, its
// frame before it) differing; `several` set with more than one
bool keysOf(uintptr_t node, uint32_t place, uint32_t stride, uint32_t bytes, bool& several) {
    const uint32_t first = at<uint32_t>(node + place), last = at<uint32_t>(node + place + 4);
    if (!first || last <= first) return false;
    const uint32_t count = (last - first) / stride;
    if (count > 1) several = true;
    if (count > 65535) return true;  // out of reason: left to the game
    return gamemodels::keysMove(reinterpret_cast<const uint8_t*>(uintptr_t(first)), count, stride, 4, bytes);
}

// a node the game moves - by its keys' values (3.0.20-beta.5, R-UQPC3E: by their count 865 of 1628 Prontera nodes
// «moving»; every model runs its animation - the actor's kind 2 from its constructor FUN_004E0540 - so the values
// decide); `alike` set for one with several keys all alike. 3.0.20-beta.4 (R-B2EM7U): not its actor's +0x1F0 - a
// blocking kind (the game's collision FUN_004EB3C0)
bool animated(uintptr_t node, bool* alike = nullptr) {
    bool several = false;
    const bool moves = keysOf(node, kMoveKeys, 16, 12, several) || keysOf(node, kTurnKeys, 0x14, 16, several);
    if (alike && several && !moves) *alike = true;
    return moves;
}

void capture(uintptr_t node, bool flipped, const float* parent, const float* points, const float* matrix, int count,
             bool moving = false) {
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
    n.flipped = flipped;
    bool drawable = true;  // 3.0.20-beta.4: whether the renderer may draw it in place of the game
    for (uint32_t t = 0; t < textureCount; ++t) {
        const uintptr_t texture = at<uint32_t>(textureList + 4 * t);
        n.textures.push_back(texture ? dx7::gpuTextureOf(reinterpret_cast<const void*>(
                                           uintptr_t(at<uint32_t>(texture + kTextureSurface))))
                                     : 0u);
        if (!n.textures.back() || at<int32_t>(texture + kTextureFormat) == int32_t(kFormatAlpha)) drawable = false;
    }
    n.vertices.resize(size_t(vertices) * 3);
    n.moving = moving;  // 3.0.20-beta.14: its own vertices, placed by the frame's matrix for it
    if (moving)
        std::memcpy(n.vertices.data(), points, n.vertices.size() * sizeof(float));
    else
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
    if (drawable && moving)  // 3.0.20-beta.14
        g_moving[uint32_t(node)] = Moving{at<uint32_t>(node + mc::kNodeMesh), at<uint32_t>(node + mc::kNodeColours),
                                          at<uint32_t>(node + mc::kNodeLit)};
    else if (drawable)
        g_still[uint32_t(node)] = stillOf(node, parent);  // 3.0.20-beta.4
    else
        ++g_keptBlended;
    ++g_sentNodes;
    g_sentFaces += faces;
}

}  // namespace

// 3.0.20-beta.22: a node the game draws itself in the dump's frame - where it stands in the world (its points by the
// draw's matrix and the view's inverse), kept when near the player
void dumpGameNode(const float* points, const float* matrix, int count) {
    ++g_dumpGameAll;
    const float* inverse = viewInverse();
    const uintptr_t node = uintptr_t(g_calls[g_depth - 1].node);
    if (!inverse || !matrix || !points || count <= 0 || count > int(kMostVertices)) return;
    float world[12];
    gamemodels::worldOf(matrix, inverse, world);
    DumpNode d;
    d.id = uint32_t(node);
    d.vertices = uint32_t(count);
    const uintptr_t mesh = at<uint32_t>(node + mc::kNodeMesh);
    d.faces = mesh ? at<uint32_t>(mesh + mc::kMeshFaces) : 0;
    d.flipped = g_calls[g_depth - 1].flipped;
    const mc::Colouring how = colouringOf(reinterpret_cast<const uint8_t*>(node));
    d.alpha = how.alpha;
    d.way = how.way;
    for (int a = 0; a < 3; ++a) d.lo[a] = 1e30f, d.hi[a] = -1e30f;
    for (int v = 0; v < count; ++v) {
        float w[3];
        gamemodels::transform(world, points + 3 * v, w);
        for (int a = 0; a < 3; ++a) d.lo[a] = std::min(d.lo[a], w[a]), d.hi[a] = std::max(d.hi[a], w[a]);
    }
    const float cx = (d.lo[0] + d.hi[0]) / 2, cz = (d.lo[2] + d.hi[2]) / 2;
    if (std::fabs(cx - g_dumpAt[0]) <= kDumpReach && std::fabs(cz - g_dumpAt[1]) <= kDumpReach &&
        g_dumpGame.size() < kDumpMost)
        g_dumpGame.push_back(d);
}

// 3.0.20-beta.55 (R-4PJUBK «там где портал дом куда-то пропал»: prontera's 프론테라\교역소.rsm, a house 165 ahead of
// the player, not in the picture - the dump of beta.22 and .44 looked 150 round the player, at our nodes alone, and
// could not tell why): the dump's frame asks the game itself. Each of the map's models (the world's actors, CView
// +0x1b4 the world, its vector +0x48..+0x4c; uaRO.exe FUN_004fa8b0 draws them by the view's leaves) that stands in
// front of the eye and was not drawn - neither left to our renderer nor drawn by the game: whether the game came to
// it this frame (its mark +0x1e4 against the frame's number, the renderer's +0x34 - set even when its own box fails
// the view's test), what the game's own test of its box says now (FUN_004fa0f0 on the view's planes, +0xF4: 0 out, 1
// in, 2 across), its alpha, its root node. The three answers tell the three causes apart: the game never came to it
// (its leaves out of the view's lists), its box failed the test, or it was drawn and lost on our side
constexpr uint32_t kRendererAt = 0x6EE3D8, kRendererFrame = 0x34, kViewWorldAt = 0x1B4, kViewPlanes = 0xF4;
constexpr uint32_t kWorldActors = 0x48, kActorNode = 0x28, kActorName = 0x2C, kActorBox = 0x130, kActorMark = 0x1E4;
constexpr uint32_t kActorAlpha = 0x1ED, kBoxTest = 0x4FA0F0;
constexpr uint8_t kBoxTestBytes[16] = {0x55, 0x8B, 0xEC, 0x8B, 0x55, 0x08, 0x53, 0x56,
                                       0x57, 0x32, 0xDB, 0x33, 0xF6, 0x8D, 0x79, 0x0C};
using BoxTestFn = char(__attribute__((thiscall)) *)(uint32_t planes, const float* box);
constexpr float kActorsReach = 800.0f;
constexpr size_t kActorsMost = 40;

// a name of the game's (CP949 bytes) in UTF-8 for the log; what is no text: its bytes' count
std::string nameOf(uint32_t at) {
    char raw[41] = {};
    if (!readGame(at, raw, 40)) return "?";
    const int length = int(strnlen(raw, 40));
    if (length <= 0) return "(no name)";
    wchar_t wide[48] = {};
    const int n = MultiByteToWideChar(949, 0, raw, length, wide, 47);
    char out[160] = {};
    const int m = n > 0 ? WideCharToMultiByte(CP_UTF8, 0, wide, n, out, int(sizeof out) - 1, nullptr, nullptr) : 0;
    return m > 0 ? std::string(out, size_t(m)) : std::string("(") + std::to_string(length) + " bytes)";
}

void dumpActors() {
    const uint32_t view = view3dwin::lastView();
    uint32_t world = 0, first = 0, last = 0, renderer = 0, frame = 0;
    float m[12];
    if (!view || !readGame(view + kViewWorldAt, &world, 4) || !world || !readGame(world + kWorldActors, &first, 4) ||
        !readGame(world + kWorldActors + 4, &last, 4) || last < first || last - first > 4 * 65536 ||
        !readGame(view + kViewMatrix, m, sizeof m) || !readGame(kRendererAt, &renderer, 4) || !renderer ||
        !readGame(renderer + kRendererFrame, &frame, 4)) {
        LOG_INFO("models dump: the game's own list of the map's models is not where this build has it - not asked");
        return;
    }
    // the frame the dump is of: its number noted as its models were drawn; none drawn - the one before the present's
    if (g_dumpFrame) frame = g_dumpFrame;
    else if (frame) --frame;
    uint8_t bytes[sizeof kBoxTestBytes];
    const bool test = readGame(kBoxTest, bytes, sizeof bytes) && !std::memcmp(bytes, kBoxTestBytes, sizeof bytes);
    std::unordered_set<uint32_t> ours(g_frameIds.begin(), g_frameIds.end());
    ours.insert(g_frameMoving.begin(), g_frameMoving.end());
    size_t inFront = 0, byUs = 0, byGame = 0, lost = 0, told = 0;
    // 3.0.20-beta.65 (R-64G2U9, R-7VX6Q9 «тут тоже нету дома» from above: the eye's cone holds a thousand of
    // Prontera's models there, 900 of them legitimately off the picture - the forty written were the map's first,
    // lamps and paving 600 away, not the house missing beside the player): the ones not drawn nearest the player
    // first, by the ground's distance from the dump's place
    struct Lost {
        float away;
        uint32_t actor, root, mark;
        float pos[3], box[15], ahead, aside;
        uint8_t alpha;
    };
    std::vector<Lost> losts;
    for (uint32_t p = first; p < last; p += 4) {
        Lost l{};
        if (!readGame(p, &l.actor, 4) || !l.actor || !readGame(l.actor, l.pos, sizeof l.pos) ||
            !readGame(l.actor + kActorBox, l.box, sizeof l.box) || !readGame(l.actor + kActorNode, &l.root, 4) ||
            !readGame(l.actor + kActorMark, &l.mark, 4) || !readGame(l.actor + kActorAlpha, &l.alpha, 1))
            continue;
        // its box's middle in the view (a row vector by the view's 4 x 3): ahead, aside, and its reach
        const float* c = l.box + 3;
        l.aside = c[0] * m[0] + c[1] * m[3] + c[2] * m[6] + m[9];
        l.ahead = c[0] * m[2] + c[1] * m[5] + c[2] * m[8] + m[11];
        const float reach = std::fabs(l.box[0]) + std::fabs(l.box[1]) + std::fabs(l.box[2]);
        if (!(l.ahead > 10.0f && l.ahead < kActorsReach) || std::fabs(l.aside) > l.ahead * 1.2f + reach) continue;
        ++inFront;
        if (ours.count(l.root)) {
            ++byUs;
            continue;
        }
        if (g_dumpDrawn.count(l.root)) {
            ++byGame;
            continue;
        }
        ++lost;
        l.away = std::hypot(c[0] - g_dumpAt[0], c[2] - g_dumpAt[1]);
        losts.push_back(l);
    }
    std::sort(losts.begin(), losts.end(), [](const Lost& a, const Lost& b) { return a.away < b.away; });
    for (const Lost& l : losts) {
        if (told >= kActorsMost) break;
        ++told;
        const int answer =
            test ? int(reinterpret_cast<BoxTestFn>(uintptr_t(kBoxTest))(view + kViewPlanes, const_cast<float*>(l.box)))
                 : -1;
        const bool sent = g_sent.count(l.root) != 0;
        LOG_INFO("models dump: NOT DRAWN - the game's model «%s» at x %.0f, z %.0f, y %.0f, %.0f from the player (its "
                 "box's middle %.0f ahead, %.0f aside; half sizes %.0f %.0f %.0f): the game %s this frame (its mark "
                 "%u, the frame %u); the game's own test of its box says %s; its alpha %u; its root node %08X %s",
                 nameOf(l.actor + kActorName).c_str(), l.pos[0], l.pos[2], l.pos[1], l.away, l.ahead, l.aside,
                 l.box[0], l.box[1], l.box[2], l.mark == frame ? "came to it" : "did not come to it", l.mark, frame,
                 answer == 0 ? "out of the view" : answer == 1 ? "in the view" : answer == 2 ? "across its edge"
                                                                                               : "nothing (not asked)",
                 unsigned(l.alpha), l.root,
                 g_still.count(l.root) ? "captured, still - ours when the game draws it"
                 : g_moving.count(l.root) ? "captured, moving - ours when the game draws it"
                 : sent ? "captured once, the game's since (moved or coloured anew)" : "never captured");
    }
    LOG_INFO("models dump: of the map's %u models %zu stand in front of the eye within %.0f - %zu left to our renderer "
             "this frame, %zu drawn by the game itself, %zu not drawn at all%s",
             unsigned((last - first) / 4), inFront, kActorsReach, byUs, byGame, lost,
             lost > told ? " (the first of them written above)" : lost ? " (written above)" : "");
}

void finishDump() {
    LOG_INFO("models dump at cell %d, %d (the world's x %.0f, z %.0f; the map %dx%d): this frame %u nodes drawn by our "
             "renderer (%u of them moving), %u by the game itself - %zu of those within %.0f of the player:",
             g_dumpCell[0], g_dumpCell[1], g_dumpAt[0], g_dumpAt[1], g_dumpSize[0], g_dumpSize[1], g_dumpOurs,
             g_dumpOursMoving, g_dumpGameAll, g_dumpGame.size(), kDumpReach);
    for (const DumpNode& d : g_dumpGame)
        LOG_INFO("models dump: the game's own node %08X - %u vertices, %u faces%s, its actor's alpha %u%s, coloured "
                 "the %s way; x %.0f..%.0f, z %.0f..%.0f, y %.0f..%.0f (about cell %.0f, %.0f)",
                 d.id, d.vertices, d.faces, d.flipped ? ", mirrored" : "", unsigned(d.alpha),
                 d.alpha != 0xFF ? " (faded by the game: see-through)" : "",
                 d.way == mc::Way::Plain ? "plain" : d.way == mc::Way::Lit ? "lit"
                 : d.way == mc::Way::SeeThrough ? "half see-through" : "other",
                 d.lo[0], d.hi[0], d.lo[2], d.hi[2], d.lo[1], d.hi[1],
                 (d.lo[0] + d.hi[0]) / 10.0f + float(g_dumpSize[0]) / 2.0f,
                 (d.lo[2] + d.hi[2]) / 10.0f + float(g_dumpSize[1]) / 2.0f);
    dumpActors();  // 3.0.20-beta.55
}

void noteProjection(const float* points, const float* matrix, int count) {
    if (g_draw && g_dump == Dump::Now && g_depth > 0 && count > 0) dumpGameNode(points, matrix, count);
    if (!g_draw || g_depth <= 0 || count <= 0 || g_captures >= kCapturesAFrame) return;
    const uintptr_t node = uintptr_t(g_calls[g_depth - 1].node);
    if (!node || g_sent.count(uint32_t(node))) return;
    ++g_captures;
    bool alike = false;
    bool moves = animated(node, &alike);  // 3.0.20-beta.4: or one of the nodes it hangs on, the game's draws round it
    bool under = false;
    for (int d = 0; d + 1 < g_depth && !moves; ++d) moves = under = animated(uintptr_t(g_calls[d].node));
    if (moves) {
        g_sent.insert(uint32_t(node));  // looked at once
        ++g_animated;
        if (under) ++g_under;
        // 3.0.20-beta.14 (stage 4): captured too - its own vertices, placed by the frame's matrix for it
        capture(node, g_calls[g_depth - 1].flipped, g_calls[g_depth - 1].parent, points, matrix, count, true);
        return;
    }
    if (alike) ++g_alike;
    capture(node, g_calls[g_depth - 1].flipped, g_calls[g_depth - 1].parent, points, matrix, count);
}

void mapChanged() {
    g_sent.clear();
    g_still.clear();
    g_moving.clear();  // 3.0.20-beta.14
    g_frameIds.clear();
    g_frameMoving.clear();
    g_frameMatrices.clear();
    g_told = g_toldMoving = 0;
    ++g_map;
}

void dumpAround() {
    int x = 0, y = 0, w = 0, h = 0;
    if (!readPlayerCell(x, y, w, h)) {
        LOG_INFO("models dump: the player's place is not known - nothing written");
        return;
    }
    g_dumpCell[0] = x, g_dumpCell[1] = y, g_dumpSize[0] = w, g_dumpSize[1] = h;
    g_dumpAt[0] = (float(x) - float(w) / 2.0f) * 5.0f;  // a cell's place in the world (the map's axes aside)
    g_dumpAt[1] = (float(y) - float(h) / 2.0f) * 5.0f;
    g_dump = Dump::Next;
    LOG_INFO("models dump: asked at cell %d, %d on %s - the next whole frame written", x, y, readGameMap().c_str());
}

void setOn(bool on) { g_on = on; }

void beforeRecord() {
    const bool dumping = g_dump == Dump::Now && !g_dumpTold;  // 3.0.20-beta.22: the renderer asked once
    if (g_frameIds.size() == g_told && g_frameMoving.size() == g_toldMoving && !dumping) return;
    gamemodels::Frame f;
    f.dump = g_dump == Dump::Now;
    f.dumpAt[0] = g_dumpAt[0], f.dumpAt[1] = g_dumpAt[1];
    g_dumpTold = g_dumpTold || dumping;
    f.ids = g_frameIds;
    f.moving = g_frameMoving;  // 3.0.20-beta.14
    f.matrices = g_frameMatrices;
    for (int k = 0; k < 3; ++k) f.factors[k] = at<float>(mc::kFactors + uintptr_t(4 * k));
    f.lit = at<uint8_t>(mc::kLitWay) != 0;
    if (auto* r = Context::get().renderer) r->gameModelsFrame(f);
    g_told = g_frameIds.size();
    g_toldMoving = g_frameMoving.size();
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
    if (g_dump == Dump::Now) {  // 3.0.20-beta.22: the dump's frame done
        finishDump();
        g_dump = Dump::None;
    } else if (g_dump == Dump::Next) {
        g_dump = Dump::Now;
        g_dumpFrame = 0;  // 3.0.20-beta.65
        g_dumpGame.clear();
        g_dumpDrawn.clear();
        g_dumpOurs = g_dumpOursMoving = g_dumpGameAll = 0;
        g_dumpTold = false;
    }
    if (!g_frameIds.empty() || !g_frameMoving.empty()) ++g_oursFrames;  // 3.0.20-beta.4
    g_frameIds.clear();
    g_frameMoving.clear();  // 3.0.20-beta.14
    g_frameMatrices.clear();
    g_told = g_toldMoving = 0;
    g_ticks = 0;
    const DWORD tick = GetTickCount();
    if (!g_toldAt) g_toldAt = tick;
    if (tick - g_toldAt < kTellEveryMs) return;
    g_toldAt = tick;
    if (const std::string line = g_tally.line(); !line.empty()) LOG_INFO("%s", line.c_str());
    g_tally.clear();
    if (g_sentNodes || g_animated)  // 3.0.20-beta.3; 3.0.20-beta.5: those under a moving one, the alike keys
        LOG_INFO("models: %u nodes captured as the game draws them (%u faces) and sent to the renderer (%u of them "
                 "with several keys all alike), %u of them moving - their own vertices, placed by the frame's "
                 "matrices (%u of them hung on a moving one)", g_sentNodes, g_sentFaces, g_alike, g_animated, g_under);
    g_sentNodes = g_sentFaces = g_animated = g_under = g_alike = 0;
    if (g_ours || g_oursMoving || g_keptBlended || g_moved)  // 3.0.20-beta.4; 3.0.20-beta.14: the moving ones
        LOG_INFO("models: %u nodes drawn by our renderer in place of the game's in %u frames (%.0f a frame; %.0f of "
                 "them moving, by their matrices) - [view3d] own_models; %u captured ones left to the game (a texture "
                 "with its own alpha, or one the renderer does not know), %u given back to it (moved or coloured anew "
                 "since)",
                 g_ours + g_oursMoving, g_oursFrames,
                 g_oursFrames ? double(g_ours + g_oursMoving) / g_oursFrames : 0.0,
                 g_oursFrames ? double(g_oursMoving) / g_oursFrames : 0.0, g_keptBlended, g_moved);
    g_ours = g_oursFrames = g_keptBlended = g_moved = g_oursMoving = 0;
}

}  // namespace uagfx::win::modelcensus
