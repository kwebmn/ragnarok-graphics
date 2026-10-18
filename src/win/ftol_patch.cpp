#include "win/ftol_patch.hpp"

#include <windows.h>
#include <cpuid.h>

#include <cstring>

#include "core/ftol_exe.hpp"
#include "win/log.hpp"

namespace uagfx::win {

bool installFastFtol(bool on) {
    if (!on) {
        LOG_INFO("float to integer: the game's own ([debug] fast_ftol = 0)");
        return false;
    }
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (!__get_cpuid(1, &a, &b, &c, &d) || !(c & bit_SSE3)) {
        LOG_INFO("float to integer: the game's own (the processor has no SSE3)");
        return false;
    }
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(ftol_exe::kFtol));
    uint8_t now[ftol_exe::kSize] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), at, now, sizeof now, &got) || got != sizeof now ||
        std::memcmp(now, ftol_exe::kOriginal, sizeof now) != 0) {
        LOG_INFO("float to integer: the game's own (its __ftol is not where this build of uaRO.exe has it)");
        return false;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, ftol_exe::kSize, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("float to integer: the game's own (its code cannot be changed, error %lu)", GetLastError());
        return false;
    }
    std::memcpy(at, ftol_exe::kFast, ftol_exe::kSize);
    DWORD unused = 0;
    VirtualProtect(at, ftol_exe::kSize, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, ftol_exe::kSize);
    LOG_INFO("float to integer: uaRO.exe's __ftol by fisttp (SSE3, 0x%X)", ftol_exe::kFtol);
    return true;
}

}  // namespace uagfx::win
