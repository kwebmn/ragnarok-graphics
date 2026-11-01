#include "win/sort_guard.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/depth_sort.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: the sort (cdecl: the first pair, past the last, a tag) and its comparison, byte for byte; the renderer's
// four calls of it
constexpr uint32_t kSort = 0x415E20, kLess = 0x416C20;
const uint8_t kSortCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x24, 0x8B, 0x4D, 0x0C, 0x53, 0x8B, 0x5D, 0x08};
const uint8_t kLessCode[] = {0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08, 0x8B, 0x55, 0x0C, 0xD9, 0x01, 0xD8, 0x1A,
                             0xDF, 0xE0, 0xF6, 0xC4, 0x01};
constexpr uint32_t kCalls[] = {0x40E992, 0x40E9FA, 0x40EA6F, 0x40EB35};
using SortFn = void (*)(depthsort::Pair* first, depthsort::Pair* last, int tag);
constexpr long kMostPairs = 1L << 22;

int g_told = 0;
unsigned g_lists = 0, g_pairs = 0;

void sortMended(depthsort::Pair* first, depthsort::Pair* last, int tag) {
    if (first && last > first && last - first < kMostPairs) {
        if (const int mended = depthsort::mend(first, last)) {
            ++g_lists;
            g_pairs += unsigned(mended);
            if (g_told < 10) {
                ++g_told;
                const uint32_t from = uint32_t(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
                LOG_INFO("sort guard: %d of the game's %ld see-through faces with a depth not a number made the "
                         "farthest before its sort (its list's call 0x%X, on %s) - unguarded, its sort would have run "
                         "past the list's end (R-DEHAHA); %u lists, %u faces so far",
                         mended, long(last - first), from - 5, readGameMap().c_str(), g_lists, g_pairs);
            }
        }
    }
    reinterpret_cast<SortFn>(uintptr_t(kSort))(first, last, tag);
}

bool same(uint32_t at, const uint8_t* code, size_t size) {
    uint8_t got[32] = {};
    SIZE_T n = 0;
    return size <= sizeof got &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), got, size, &n) &&
           n == size && std::memcmp(got, code, size) == 0;
}

// A call's target where this build has it: E8 and the sort
bool callsSort(uint32_t at) {
    uint8_t op[5] = {};
    SIZE_T n = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), op, sizeof op, &n) ||
        n != sizeof op || op[0] != 0xE8)
        return false;
    int32_t rel = 0;
    std::memcpy(&rel, op + 1, sizeof rel);
    return at + 5 + uint32_t(rel) == kSort;
}

}  // namespace

bool installSortGuard() {
    static int done = -1;  // once: 1 in, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    bool known = same(kSort, kSortCode, sizeof kSortCode) && same(kLess, kLessCode, sizeof kLessCode);
    for (const uint32_t at : kCalls) known = known && callsSort(at);
    if (!known) {
        LOG_INFO("sort guard: the game's sort of its see-through faces is not where this build of uaRO.exe has it - "
                 "left as it is");
        return false;
    }
    for (const uint32_t at : kCalls) {
        auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(at + 1));
        DWORD old = 0;
        if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) {
            LOG_INFO("sort guard: the game's code cannot be changed (error %lu) - left as it is", GetLastError());
            return false;  // the calls changed so far go through the guard too: each one alike
        }
        const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&sortMended) - (uintptr_t(at) + 5));
        std::memcpy(rel, &to, sizeof to);
        DWORD unused = 0;
        VirtualProtect(rel, 4, old, &unused);
        FlushInstructionCache(GetCurrentProcess(), rel, 4);
    }
    done = 1;
    LOG_INFO("sort guard: the game's 4 sorts of its see-through faces (uaRO.exe 0x%X) go through the driver - a depth "
             "not a number made the farthest first", kSort);
    return true;
}

}  // namespace uagfx::win
