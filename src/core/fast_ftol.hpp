#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::fastftol {

// 3.0.19-beta.48 (the owner: «давай» - uaRO.exe's own parts rewritten, the first): the game's float-to-integer, MSVC
// 6's __ftol at kFtol (1026 calls in the exe, some 100000 a frame - the ground's colours alone a dozen a cell): the
// FPU's rounding set to truncation and back around a fistp each call, 3.5 % of the game's thread. Its kFtolSize bytes
// as this build has them: then it is sent to win/fast_ftol's fisttp (SSE3: truncation whatever the rounding) - the
// same numbers, the integer indefinite past the range and for NaN as before
constexpr uint32_t kFtol = 0x671970;
constexpr size_t kFtolSize = 39;
extern const uint8_t kFtolBytes[kFtolSize];
bool isFtolEntry(const uint8_t* bytes, size_t n);

}  // namespace uagfx::fastftol
