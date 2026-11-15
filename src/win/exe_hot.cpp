#include "win/exe_hot.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "core/exe_hot.hpp"
#include "win/entry_hook.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/model_census.hpp"

namespace uagfx::win::exehotwin {
namespace {

// The game's projection's numbers: its view's scale and screen centre, its near and far (k kept while they stay)
struct View {
    float f[2];
    int32_t centre[2];
    float nearZ, k;
};

View viewOf(const uint8_t* view) {
    View v;
    std::memcpy(v.f, view, sizeof v.f);
    std::memcpy(v.centre, view + 0x2C, sizeof v.centre);
    v.nearZ = *reinterpret_cast<const float*>(uintptr_t(exehot::kNear));
    const float farZ = *reinterpret_cast<const float*>(uintptr_t(exehot::kFar));
    static float knownNear = -1.0f, knownFar = -1.0f, k = 1.0f;  // the game's thread's alone
    if (v.nearZ != knownNear || farZ != knownFar) {
        knownNear = v.nearZ, knownFar = farZ;
        k = exehot::depthScale(v.nearZ, farZ);
    }
    v.k = k;
    return v;
}

// The game's projection's own way in (thiscall on its view; a point, the matrix, out - ret 0xC)
void __attribute__((thiscall)) projectHook(const uint8_t* view, const float* p, const float* m, float* out) {
    const View v = viewOf(view);
    exehot::project(v.f[0], v.f[1], v.centre[0], v.centre[1], v.nearZ, v.k, p, m, out);
}

// 3.0.19-beta.53: the models' vertices, their pool - what went ours, told once
uint64_t g_nodes = 0, g_vertices = 0, g_grown = 0, g_growOwn = 0;
bool g_told = false;

void tellOnce() {
    if (g_told || g_grown + g_growOwn < 200000) return;
    g_told = true;
    LOG_INFO("speed: uaRO.exe's models - %llu nodes' %llu vertices projected in a row; their pool grown %llu times "
             "without blanks, %llu by the game's own (not at its end or no room)",
             (unsigned long long)g_nodes, (unsigned long long)g_vertices, (unsigned long long)g_grown,
             (unsigned long long)g_growOwn);
}

// 3.0.19-beta.53: the model's vertices in a row (exehot::modelStub's call, cdecl); the stack made 16 bytes for SSE
__attribute__((force_align_arg_pointer)) void modelProjection(const uint8_t* renderer, const float* points,
                                                              const float* m, float* out, int count) {
    if (!renderer || count <= 0) return;
    const View v = viewOf(renderer);
    exehot::projectMany(v.f[0], v.f[1], v.centre[0], v.centre[1], v.nearZ, v.k, points, m, out, count);
    ++g_nodes;
    g_vertices += uint64_t(count);
}

// 3.0.19-beta.53: the game's vertex pool grown at its end with nothing written (exehot::growAtEnd), else its own
// insert through the trampoline (thiscall on the vector: the place, a count, the blank; ret 0xC)
using GrowFn = void(__attribute__((thiscall)) *)(exehot::GameVector* self, uint32_t at, uint32_t n, const void* blank);
uintptr_t g_grow = 0;

void __attribute__((thiscall)) growHook(exehot::GameVector* self, uint32_t at, uint32_t n, const void* blank) {
    modelcensus::noteGrow(uint32_t(uintptr_t(self)), at, n);  // 3.0.20-beta.1: the node being drawn - its faces' block
    if (self && exehot::growAtEnd(*self, at, n)) {
        ++g_grown;
        tellOnce();
        return;
    }
    ++g_growOwn;
    reinterpret_cast<GrowFn>(g_grow)(self, at, n, blank);
}

// The game's sort's own way in (cdecl: first, last, two of the STL's own) - the faces' list sorted whole; the
// insertion passes the game makes after it find nothing to move
void sortHook(uint32_t* first, uint32_t* last, uint32_t, uint32_t) {
    if (!first || last <= first + 1) return;
    const size_t n = size_t(last - first);
    static std::vector<exehot::FaceKey> keys;  // the game's thread's alone
    keys.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t face = first[i];
        const auto* f = reinterpret_cast<const uint32_t*>(uintptr_t(face));
        keys[i] = exehot::FaceKey{exehot::faceKey(f[exehot::kFaceKey1 / 4], f[exehot::kFaceKey2 / 4]), uint32_t(i),
                                  face};
    }
    exehot::sortFaces(keys);
    for (size_t i = 0; i < n; ++i) first[i] = keys[i].face;
}

// A jump to `to` put into the first 8 bytes at `at` at once (16 bytes aligned: a thread of the game's there meets
// either the old bytes or the jump), when its first `n` bytes are `want`
bool jumpTo(uint32_t at, const uint8_t* want, size_t n, const void* to, const char* what) {
    uint8_t have[64] = {};
    if (n > sizeof have || !readGame(at, have, n) || !exehot::sameBytes(have, want, n)) {
        LOG_INFO("speed: uaRO.exe's %s is not where this build has it - its own kept", what);
        return false;
    }
    auto* code = reinterpret_cast<uint8_t*>(uintptr_t(at));
    DWORD was = 0;
    if (!VirtualProtect(code, 8, PAGE_EXECUTE_READWRITE, &was)) return false;
    int64_t before = 0, after = 0;
    std::memcpy(&before, code, 8);
    uint8_t next[8];
    std::memcpy(next, code, 8);
    next[0] = 0xE9;  // jmp rel32
    const int32_t rel = int32_t(uintptr_t(to) - (uintptr_t(code) + 5));
    std::memcpy(next + 1, &rel, 4);
    std::memcpy(&after, next, 8);
    InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(code), after, before);
    VirtualProtect(code, 8, was, &was);
    FlushInstructionCache(GetCurrentProcess(), code, 8);
    return true;
}

}  // namespace

void install() {
    static bool done = false;
    if (done) return;
    done = true;
    const bool projection = jumpTo(exehot::kProject, exehot::kProjectBytes, exehot::kProjectSize,
                                   reinterpret_cast<const void*>(&projectHook), "vertex projection");
    const bool sort = jumpTo(exehot::kSort, exehot::kSortBytes, exehot::kSortSize,
                             reinterpret_cast<const void*>(&sortHook), "faces' sort");
    LOG_INFO("speed: uaRO.exe's vertex projection (0x%X) %s, its faces' sort (0x%X) %s", exehot::kProject,
             projection ? "ours - one division a vertex, not three" : "its own", exehot::kSort,
             sort ? "ours - the keys read once, in a row" : "its own");
    // 3.0.19-beta.53 (the owner's «берись»): the models' vertex pool grown without blanks, their vertices projected
    // in a row - the loop's way to ours made in a page of its own
    const bool grow = hookEntryBytes(exehot::kGrow, exehot::kGrowBytes, exehot::kGrowSize,
                                     reinterpret_cast<const void*>(&growHook), g_grow);
    bool models = false;
    auto* stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (stub) {
        exehot::modelStub(uint32_t(uintptr_t(stub)), uint32_t(uintptr_t(&modelProjection)), stub);
        FlushInstructionCache(GetCurrentProcess(), stub, 64);
        models = projection && jumpTo(exehot::kModelLoop, exehot::kModelLoopBytes, exehot::kModelLoopSize, stub,
                                      "models' projection loop");
        if (!models) VirtualFree(stub, 0, MEM_RELEASE);
    }
    LOG_INFO("speed: uaRO.exe's models' vertices (0x%X) %s, their vertex pool (0x%X) %s", exehot::kModelLoop,
             models ? "projected in a row by ours" : "a call each (its own)", exehot::kGrow,
             grow ? "grown without blanks" : "filled with blanks (its own)");
}

}  // namespace uagfx::win::exehotwin
