#include "win/aura_patch.hpp"

#include <windows.h>

#include <cstring>

#include "core/aura_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {

bool installAuraFix() {
    static bool done = false;  // once: its call is the guard's after it
    if (done) return true;
    uint8_t at[aura_exe::kOriginalSize] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(aura_exe::kAt)), at, sizeof at,
                           &got) ||
        got != sizeof at || std::memcmp(at, aura_exe::kOriginal, sizeof at) != 0) {
        LOG_INFO("aura: the game's own (its call is not where this build of uaRO.exe has it)");
        return false;
    }
    auto* guard = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!guard) return false;
    const uint32_t where = uint32_t(reinterpret_cast<uintptr_t>(guard));
    const aura_exe::Guard g = aura_exe::guard(where);
    std::memcpy(guard, g.code, sizeof g.code);
    DWORD was = 0;
    if (!VirtualProtect(guard, 4096, PAGE_EXECUTE_READ, &was)) return false;
    uint8_t call[5];
    aura_exe::callTo(where, call);
    auto* site = reinterpret_cast<uint8_t*>(uintptr_t(aura_exe::kCall));
    if (!VirtualProtect(site, sizeof call, PAGE_EXECUTE_READWRITE, &was)) return false;
    std::memcpy(site, call, sizeof call);
    DWORD unused = 0;
    VirtualProtect(site, sizeof call, was, &unused);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof call);
    done = true;
    LOG_INFO("aura: a level 99 character's teleport on the same map no longer crashes the game (uaRO.exe 0x%X)",
             aura_exe::kCall);
    return true;
}

}  // namespace uagfx::win
