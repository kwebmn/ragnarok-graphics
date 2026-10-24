#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::shadow_exe {

// 3.0.4-beta.9 (R-VBDSCP: an access violation at uaRO.exe+0x147bce two seconds into moscovia): FUN_00547b80 draws a
// character's shadow (shadow.spr under its feet) and asks for the game mode first - GetGameMode 0x4f6ad0 on the
// mode manager 0x74b578, its world at +0xc4 - but while a map loads there is no game mode yet: 0, and +0xc4 of it
// faults. Its call at kCall goes to a guard instead: the mode as before, and none - the function's own way out
// (kWayOut: pop edi, esi, ebx, its frame, ret 8), the same its checks of a zero size take; that frame has no shadow.
constexpr uint32_t kAt = 0x547BC1;    // mov ecx, 0x74b578
constexpr uint32_t kCall = 0x547BC6;  // call 0x4f6ad0
constexpr uint32_t kGetGameMode = 0x4F6AD0;
constexpr uint32_t kWayOut = 0x547E0E;
constexpr size_t kOriginalSize = 10;
extern const uint8_t kOriginal[kOriginalSize];  // mov ecx, 0x74b578; call 0x4f6ad0 - this build's bytes at kAt
constexpr size_t kWayOutSize = 9;
extern const uint8_t kWayOutCode[kWayOutSize];  // pop edi; pop esi; pop ebx; mov esp, ebp; pop ebp; ret 8

constexpr size_t kGuardSize = 19;
struct Guard {
    uint8_t code[kGuardSize];
};
// The guard's code where it lands (`at`): mov eax, GetGameMode; call eax; test eax, eax; jnz ret; mov dword [esp],
// kWayOut; ret - ECX is the caller's
Guard guard(uint32_t at);
// The game's call at kCall, to the guard at `at`
void callTo(uint32_t at, uint8_t out[5]);

}  // namespace uagfx::shadow_exe
