#include "win/code_detour.hpp"

#include <windows.h>

#include <cstring>

namespace uagfx::win::codedetour {

uintptr_t detour(uint32_t at, const uint8_t* code, size_t size, const void* ours) {
    uint8_t now[16] = {};
    SIZE_T got = 0;
    if (size > sizeof now || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), now,
                                                size, &got) ||
        got != size || std::memcmp(now, code, size) != 0)
        return 0;
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return 0;
    std::memcpy(t, code, size);
    t[size] = 0xE9;
    const int32_t back = int32_t(uintptr_t(at) + size - (reinterpret_cast<uintptr_t>(t) + size + 5));
    std::memcpy(t + size + 1, &back, 4);
    FlushInstructionCache(GetCurrentProcess(), t, 32);
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(uintptr_t(at)), size, PAGE_EXECUTE_READWRITE, &old)) return 0;
    uint8_t jump[16];
    std::memset(jump, 0x90, size);  // the rest of the first instructions nothing
    jump[0] = 0xE9;
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(ours) - (uintptr_t(at) + 5));
    std::memcpy(jump + 1, &to, 4);
    std::memcpy(reinterpret_cast<void*>(uintptr_t(at)), jump, size);
    DWORD unused = 0;
    VirtualProtect(reinterpret_cast<void*>(uintptr_t(at)), size, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), size);
    return reinterpret_cast<uintptr_t>(t);
}

}  // namespace uagfx::win::codedetour
