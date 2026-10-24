#include <cstdint>
#include <cstring>

#include "core/shadow_exe.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.9 (R-VBDSCP: uaRO.exe+0x147bce on the way into moscovia): a character's shadow drawn while a map loads -
// the game mode asked for is none yet; the guard takes the shadow function's own way out
TEST(shadow_exe_the_guard_s_code) {
    const uint32_t at = 0x10000000;  // where the guard lands
    const shadow_exe::Guard g = shadow_exe::guard(at);
    // mov eax, GetGameMode; call eax
    CHECK(g.code[0] == 0xB8);
    uint32_t target = 0;
    std::memcpy(&target, g.code + 1, 4);
    CHECK_EQ(target, shadow_exe::kGetGameMode);
    CHECK(g.code[5] == 0xFF && g.code[6] == 0xD0);
    CHECK(g.code[7] == 0x85 && g.code[8] == 0xC0);  // test eax, eax
    CHECK(g.code[9] == 0x75 && size_t(11 + g.code[10]) == shadow_exe::kGuardSize - 1);  // jnz to the ret
    uint32_t out = 0;
    std::memcpy(&out, g.code + 14, 4);  // mov dword [esp], the function's way out
    CHECK(g.code[11] == 0xC7 && g.code[12] == 0x04 && g.code[13] == 0x24 && out == shadow_exe::kWayOut);
    CHECK(g.code[shadow_exe::kGuardSize - 1] == 0xC3);  // ret
    // the game's call there, now to the guard: E8 with the distance from its next instruction
    uint8_t call[5];
    shadow_exe::callTo(at, call);
    int32_t rel = 0;
    std::memcpy(&rel, call + 1, 4);
    CHECK(call[0] == 0xE8 && uint32_t(int32_t(shadow_exe::kCall + 5) + rel) == at);
    // the game's own bytes the change is made over: mov ecx, 0x74b578; call 0x4f6ad0
    CHECK(shadow_exe::kOriginal[0] == 0xB9 && shadow_exe::kOriginal[5] == 0xE8);
}
