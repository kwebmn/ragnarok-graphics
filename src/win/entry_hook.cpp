#include "win/entry_hook.hpp"

#include <windows.h>

#include <cstring>

namespace uagfx::win {

// A function's entry, its first n bytes `expect` (whole instructions), to `hook`; those bytes run in a trampoline
// of their own before the rest of it
bool hookEntryBytes(uint32_t entry, const uint8_t* expect, size_t n, const void* hook, uintptr_t& trampoline) {
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(entry));
    uint8_t now[16] = {};
    SIZE_T got = 0;
    if (n < 5 || n > 11 || !ReadProcessMemory(GetCurrentProcess(), at, now, n, &got) || got != n ||
        std::memcmp(now, expect, n) != 0)
        return false;
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return false;
    std::memcpy(t, now, n);
    t[n] = 0xE9;  // jmp entry + n
    const int32_t back = int32_t(uintptr_t(entry) + n - (reinterpret_cast<uintptr_t>(t) + n + 5));
    std::memcpy(t + n + 1, &back, sizeof back);
    FlushInstructionCache(GetCurrentProcess(), t, 32);
    DWORD old = 0;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(t, 0, MEM_RELEASE);
        return false;
    }
    uint8_t jump[16];
    std::memset(jump, 0x90, sizeof jump);  // jmp hook, nops after it
    jump[0] = 0xE9;
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(hook) - (uintptr_t(entry) + 5));
    std::memcpy(jump + 1, &to, sizeof to);
    std::memcpy(at, jump, n);
    DWORD unused = 0;
    VirtualProtect(at, n, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    trampoline = reinterpret_cast<uintptr_t>(t);
    return true;
}

}  // namespace uagfx::win
