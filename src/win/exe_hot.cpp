#include "win/exe_hot.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "core/exe_hot.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"

namespace uagfx::win::exehotwin {
namespace {

// The game's projection's own way in (thiscall on its view; a point, the matrix, out - ret 0xC)
void __attribute__((thiscall)) projectHook(const uint8_t* view, const float* p, const float* m, float* out) {
    float f[2];
    int32_t centre[2];
    std::memcpy(f, view, sizeof f);
    std::memcpy(centre, view + 0x2C, sizeof centre);
    const float nearZ = *reinterpret_cast<const float*>(uintptr_t(exehot::kNear));
    const float farZ = *reinterpret_cast<const float*>(uintptr_t(exehot::kFar));
    static float knownNear = -1.0f, knownFar = -1.0f, k = 1.0f;  // the game's thread's alone
    if (nearZ != knownNear || farZ != knownFar) {
        knownNear = nearZ, knownFar = farZ;
        k = exehot::depthScale(nearZ, farZ);
    }
    exehot::project(f[0], f[1], centre[0], centre[1], nearZ, k, p, m, out);
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
    uint8_t have[32] = {};
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
}

}  // namespace uagfx::win::exehotwin
