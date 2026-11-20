#include <cstdint>
#include <cstring>

#include "core/aura_exe.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.22-beta.31 (R-JBG5MM: uaRO.exe+0x1b9878 reading 0, a level 99 knight's fly wing 40 s after «Змінити акаунт»):
// the own character put anew on a same-map teleport (FUN_0055b1d0 clears the world's player, then makes it) - its
// third actor since the way in asks the world's player (none yet) to show its icons and the 99's aura again; the
// guard skips that call while there is no player
TEST(aura_exe_the_guard_skips_a_missing_player) {
    const uint32_t at = 0x10000000;  // where the guard lands
    const aura_exe::Guard g = aura_exe::guard(at);
    CHECK(g.code[0] == 0x85 && g.code[1] == 0xC9);  // test ecx, ecx (the world's player, the callee's this)
    CHECK(g.code[2] == 0x74 && size_t(4 + g.code[3]) == aura_exe::kGuardSize - 1);  // jz to the ret
    int32_t rel = 0;
    std::memcpy(&rel, g.code + 5, 4);  // jmp to the game's own function: its ret back to the game's caller
    CHECK(g.code[4] == 0xE9 && uint32_t(int32_t(at + 9) + rel) == aura_exe::kCallee);
    CHECK(g.code[aura_exe::kGuardSize - 1] == 0xC3);  // ret (the callee takes no arguments off the stack)
    // the game's call there, now to the guard
    uint8_t call[5];
    aura_exe::callTo(at, call);
    std::memcpy(&rel, call + 1, 4);
    CHECK(call[0] == 0xE8 && uint32_t(int32_t(aura_exe::kCall + 5) + rel) == at);
    // the game's own bytes the change is made over: mode, its world, the world's player, the call
    CHECK(aura_exe::kOriginal[0] == 0xB9 && aura_exe::kOriginal[5] == 0xE8 && aura_exe::kOriginal[16] == 0x8B &&
          aura_exe::kOriginal[19] == 0xE8);
    int32_t was = 0;
    std::memcpy(&was, aura_exe::kOriginal + 20, 4);
    CHECK(uint32_t(int32_t(aura_exe::kCall + 5) + was) == aura_exe::kCallee);
}
