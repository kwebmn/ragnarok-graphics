#include "win/shadow_patch.hpp"

#include <windows.h>

#include <cstring>

#include "core/shadow_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {

bool installShadowFix() {
    static bool done = false;  // once: its call is the guard's after it
    if (done) return true;
    uint8_t at[shadow_exe::kOriginalSize] = {}, out[shadow_exe::kWayOutSize] = {};
    SIZE_T got = 0, gotOut = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(shadow_exe::kAt)), at,
                           sizeof at, &got) ||
        !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(shadow_exe::kWayOut)), out,
                           sizeof out, &gotOut) ||
        got != sizeof at || gotOut != sizeof out || std::memcmp(at, shadow_exe::kOriginal, sizeof at) != 0 ||
        std::memcmp(out, shadow_exe::kWayOutCode, sizeof out) != 0) {
        LOG_INFO("shadows: the game's own (its shadow code is not where this build of uaRO.exe has it)");
        return false;
    }
    auto* guard = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!guard) return false;
    const uint32_t where = uint32_t(reinterpret_cast<uintptr_t>(guard));
    const shadow_exe::Guard g = shadow_exe::guard(where);
    std::memcpy(guard, g.code, sizeof g.code);
    DWORD was = 0;
    if (!VirtualProtect(guard, 4096, PAGE_EXECUTE_READ, &was)) return false;
    uint8_t call[5];
    shadow_exe::callTo(where, call);
    auto* site = reinterpret_cast<uint8_t*>(uintptr_t(shadow_exe::kCall));
    if (!VirtualProtect(site, sizeof call, PAGE_EXECUTE_READWRITE, &was)) return false;
    std::memcpy(site, call, sizeof call);
    DWORD unused = 0;
    VirtualProtect(site, sizeof call, was, &unused);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof call);
    done = true;
    LOG_INFO("shadows: a character's shadow drawn while a map loads no longer crashes the game (uaRO.exe 0x%X)",
             shadow_exe::kCall);
    return true;
}

}  // namespace uagfx::win
