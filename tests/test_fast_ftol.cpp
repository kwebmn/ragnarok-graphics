// 3.0.19-beta.48 (the owner: «давай» - uaRO.exe's own parts rewritten, the first: its __ftol, 3.5 % of the game's
// thread): the game's float-to-integer (MSVC 6's __ftol at 0x671970 - the FPU's rounding switched to truncation and
// back each call, some 100000 calls a frame) replaced by one SSE3 instruction, fisttp - the same numbers
#include <cstdint>
#include <limits>

#include "core/fast_ftol.hpp"
#include "test.hpp"

using namespace uagfx;

#if defined(__x86_64__) || defined(__i386__)
namespace {
int64_t viaFisttp(long double v) {
    int64_t out = 0;
    __asm__ volatile("fldt %1\n\tfisttpll %0" : "=m"(out) : "m"(v));
    return out;
}
}  // namespace

TEST(fast_ftol_truncates_as_the_games_own) {
    for (const long double v : {0.0L, 0.49L, 0.5L, 1.99L, -0.5L, -1.99L, 254.999L, 255.5L, -7.0L, 1e10L, -1e10L,
                                4294967295.5L, -2147483648.75L}) {
        CHECK(viaFisttp(v) == int64_t(v));  // toward zero, as __ftol's (its rounding set to truncate)
    }
    CHECK(viaFisttp(1e30L) == std::numeric_limits<int64_t>::min());  // past its range: the integer indefinite
    CHECK(viaFisttp(-1e30L) == std::numeric_limits<int64_t>::min());
    CHECK(viaFisttp(std::numeric_limits<long double>::quiet_NaN()) == std::numeric_limits<int64_t>::min());
}
#endif

TEST(fast_ftol_knows_the_games_own_entry) {
    CHECK(fastftol::isFtolEntry(fastftol::kFtolBytes, fastftol::kFtolSize));
    uint8_t other[fastftol::kFtolSize];
    for (size_t i = 0; i < fastftol::kFtolSize; ++i) other[i] = fastftol::kFtolBytes[i];
    other[20] ^= 0xFF;  // another build's: left alone
    CHECK(!fastftol::isFtolEntry(other, fastftol::kFtolSize));
    CHECK(!fastftol::isFtolEntry(fastftol::kFtolBytes, fastftol::kFtolSize - 1));
}
