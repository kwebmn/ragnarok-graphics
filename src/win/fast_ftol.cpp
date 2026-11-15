#include "win/fast_ftol.hpp"

#include <windows.h>

#include <cpuid.h>

#include <cstdint>
#include <cstring>

#include "core/fast_ftol.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"

// The game's __ftol's own way in: the number in st(0), popped; the 64-bit integer, truncated, in edx:eax; ecx, ebx,
// esi and edi kept - one fisttp, the FPU's control word untouched
extern "C" void uagfxFastFtol() __asm__("uagfx_fast_ftol");
__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl uagfx_fast_ftol\n"
    "uagfx_fast_ftol:\n"
    "    subl $8, %esp\n"
    "    fisttpll (%esp)\n"
    "    movl (%esp), %eax\n"
    "    movl 4(%esp), %edx\n"
    "    addl $8, %esp\n"
    "    ret\n");

namespace uagfx::win::fastftolwin {

bool install() {
    static int done = 0;
    if (done) return done > 0;
    done = -1;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (!__get_cpuid(1, &a, &b, &c, &d) || !(c & bit_SSE3)) {
        LOG_INFO("speed: the processor has no SSE3 - uaRO.exe's own __ftol kept");
        return false;
    }
    uint8_t now[fastftol::kFtolSize] = {};
    if (!readGame(fastftol::kFtol, now, sizeof now) || !fastftol::isFtolEntry(now, sizeof now)) {
        LOG_INFO("speed: uaRO.exe's __ftol is not where this build has it - its own kept");
        return false;
    }
    auto* at = reinterpret_cast<uint8_t*>(uintptr_t(fastftol::kFtol));
    DWORD was = 0;
    if (!VirtualProtect(at, 8, PAGE_EXECUTE_READWRITE, &was)) return false;
    // its first 8 bytes at once (0x671970: 16 bytes aligned) - a jump there, the rest as they were: a thread of the
    // game's in it meanwhile meets either the old bytes or the jump
    int64_t before = 0, after = 0;
    std::memcpy(&before, at, 8);
    uint8_t next[8];
    std::memcpy(next, at, 8);
    next[0] = 0xE9;  // jmp rel32
    const int32_t rel = int32_t(uintptr_t(&uagfxFastFtol) - (uintptr_t(at) + 5));
    std::memcpy(next + 1, &rel, 4);
    std::memcpy(&after, next, 8);
    InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(at), after, before);
    VirtualProtect(at, 8, was, &was);
    FlushInstructionCache(GetCurrentProcess(), at, 8);
    done = 1;
    LOG_INFO("speed: uaRO.exe's __ftol (0x%X, 1026 calls) sent to one fisttp (SSE3) - the FPU's rounding no longer "
             "switched twice a call", fastftol::kFtol);
    return true;
}

}  // namespace uagfx::win::fastftolwin
