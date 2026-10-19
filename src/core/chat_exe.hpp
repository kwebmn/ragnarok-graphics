#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::chat_exe {

// 2.6.12: uaRO.exe keeps the last ten chat lines the player sent (its flood check: CGameMode +0x418, sixteen bytes
// each) and how many there are at +0x414. CGameMode::OnInit sets that count to 0 only after the map has loaded, and
// the loading lets window messages through: Enter in the chat box while a map loads sends the line with the count
// whatever the heap had there (0xF7150 in R-PVF8AV) and writes the line far past the ten - uaRO.exe+0xa89f6. The
// game's check before the write, count < 0 && count > 10, never holds; the fixed one, count above 10 unsigned (a
// negative one too), starts the count over at 0 as the game meant to.
constexpr uint32_t kCheck = 0x574487;  // just after mov eax, [ebx+0x414]
constexpr uint32_t kReset = 0x574490;  // mov dword [ebx+0x414], 0
constexpr uint32_t kKeep = 0x57449A;   // past the reset
constexpr size_t kSize = 9;
extern const uint8_t kOriginal[kSize];  // what this build has there
extern const uint8_t kFixed[kSize];     // cmp eax, 10; jbe kKeep; nop to the reset

}  // namespace uagfx::chat_exe
