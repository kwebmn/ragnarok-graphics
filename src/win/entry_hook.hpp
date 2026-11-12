#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::win {

// A function's entry, its first n bytes (5 to 11) `expect` - whole instructions - to `hook`; those bytes run in a
// trampoline of their own before the rest of it (`trampoline`, the function to call for the game's own). False, the
// code untouched, when the bytes are not those (another build of uaRO.exe). 3.0.2-beta.5's of win/updates_hook,
// 3.0.18-beta.230 shared (win/wings).
bool hookEntryBytes(uint32_t entry, const uint8_t* expect, size_t n, const void* hook, uintptr_t& trampoline);

}  // namespace uagfx::win
