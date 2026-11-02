#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::win::codedetour {

// A function of uaRO.exe sent to ours: its first instructions (exactly `code`, else nothing is touched) copied to a
// block that goes on to the rest, a jump to `ours` in their place. The copy's address (call it for the game's own
// work), 0 when the bytes are not this build's. 3.0.11-beta.151 in win/msg_box; shared since 3.0.11-beta.172.
uintptr_t detour(uint32_t at, const uint8_t* code, size_t size, const void* ours);

}  // namespace uagfx::win::codedetour
