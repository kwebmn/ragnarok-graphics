#pragma once
#include <cstddef>
#include <cstdint>

#include "win/game_camera.hpp"

// 3.0.18-beta.167 (step 3, the shared layer of the driver's work with the game): a word of the game's memory read and
// written in one place - before, 60 modules each had their own read32 and 43 their own write32 (a WriteProcessMemory
// each time). The reads by readGame (win/safe_read's copy: no call into Windows).
namespace uagfx::win {

// always inlined (3.0.18-beta.169): readGame's tally then sees the module that asked, not this one function
inline __attribute__((always_inline)) uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

// `size` bytes into the game's memory: its heap by the safe store (win/safe_read), uaRO.exe's own image (its code,
// its tables, its globals) by WriteProcessMemory as before - it makes a page that is not writable writable for it
bool writeGame(uint32_t at, const void* in, size_t size);
inline __attribute__((always_inline)) bool write32(uint32_t at, uint32_t v) { return writeGame(at, &v, 4); }

// The game's SendMsg of its windows and actors (thiscall: the object, the sender, the message, three arguments) - 43
// modules declared it each
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

}  // namespace uagfx::win
