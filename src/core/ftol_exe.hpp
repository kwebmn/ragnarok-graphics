#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::ftol_exe {

// 2.5.20: uaRO.exe's __ftol (0x671970, MSVC 6's float to integer: 931 calls, every position and colour the game
// rounds) switches the x87 to chopping and back for each one - fstcw, fwait, fldcw twice: the slowest way there is.
// Where the processor has SSE3, fisttp chops by itself with the same result: the 64-bit integer in EDX:EAX, the value
// popped, a NaN or one too large the "integer indefinite" alike; the control word untouched.
constexpr uint32_t kFtol = 0x671970;
constexpr size_t kSize = 39;
extern const uint8_t kOriginal[kSize];  // what this build has there
extern const uint8_t kFast[kSize];      // fisttp, the rest int3 (never reached: the 931 calls come in at the top)

}  // namespace uagfx::ftol_exe
