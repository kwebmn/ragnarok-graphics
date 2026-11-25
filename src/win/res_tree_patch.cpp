#include "win/res_tree_patch.hpp"

#include <windows.h>

#include <cstring>

#include "core/res_tree_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

using LessFn = bool(__attribute__((thiscall)) *)(uint32_t key, uint32_t asked);
volatile LONG g_broken = 0;

uint32_t at(uint32_t a) { return *reinterpret_cast<const uint32_t*>(uintptr_t(a)); }

// The game's lower_bound's place: thiscall (this = the tree), the key's reference on the stack, popped by it
uint32_t __attribute__((thiscall)) guardedLowerBound(uint32_t map, uint32_t keyRef) {
    const LessFn less = reinterpret_cast<LessFn>(uintptr_t(res_tree_exe::kLess));
    bool broken = false;
    const uint32_t found = res_tree_exe::lowerBound(
        map, keyRef, at(res_tree_exe::kNilAt), [](uint32_t a) { return a ? at(a) : 0u; },
        [less](uint32_t key, uint32_t asked) { return less(key, asked); }, broken);
    if (broken && InterlockedIncrement(&g_broken) <= 5)
        LOG_INFO("files: the game's resource cache met a node another thread is still putting in (or no name asked) - "
                 "taken as not found, the file loaded anew (uaRO.exe 0x%X)", res_tree_exe::kLowerBound);
    return found;
}

}  // namespace

bool installResTreeFix() {
    static bool done = false;
    if (done) return true;
    uint8_t fn[res_tree_exe::kFnSize] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(res_tree_exe::kLowerBound)),
                           fn, sizeof fn, &got) ||
        got != sizeof fn || std::memcmp(fn, res_tree_exe::kFn, sizeof fn) != 0) {
        LOG_INFO("files: the game's own resource lookup (its lower_bound is not where this build of uaRO.exe has it)");
        return false;
    }
    uint8_t calls[2][5];
    for (int i = 0; i < 2; ++i) {
        uint8_t now[5] = {}, want[5];
        res_tree_exe::callTo(res_tree_exe::kCalls[i], res_tree_exe::kLowerBound, want);
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(res_tree_exe::kCalls[i])),
                               now, sizeof now, &got) ||
            std::memcmp(now, want, sizeof now) != 0) {
            LOG_INFO("files: the game's own resource lookup (a call at 0x%X is not this build's)",
                     res_tree_exe::kCalls[i]);
            return false;
        }
        res_tree_exe::callTo(res_tree_exe::kCalls[i], uint32_t(reinterpret_cast<uintptr_t>(&guardedLowerBound)),
                             calls[i]);
    }
    for (int i = 0; i < 2; ++i) {
        auto* site = reinterpret_cast<uint8_t*>(uintptr_t(res_tree_exe::kCalls[i]));
        DWORD was = 0, unused = 0;
        if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &was)) return false;
        std::memcpy(site, calls[i], 5);
        VirtualProtect(site, 5, was, &unused);
        FlushInstructionCache(GetCurrentProcess(), site, 5);
    }
    done = true;
    LOG_INFO("files: a resource another thread is still putting in no longer crashes the game (uaRO.exe 0x%X, 0x%X)",
             res_tree_exe::kCalls[0], res_tree_exe::kCalls[1]);
    return true;
}

}  // namespace uagfx::win
