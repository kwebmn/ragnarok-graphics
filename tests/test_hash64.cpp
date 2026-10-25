#include <cstdint>
#include <set>

#include "core/hash64.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.5-beta.11 (R-455GWU): keys that differ in their high half alone - the 32-bit build's size_t keeps the low half
// of a hash: those must differ there too
TEST(hash64_spreads_the_high_half_into_the_low_one) {
    std::set<uint32_t> low;
    for (int x = -200; x < 200; ++x)
        for (int z = 0; z < 4; ++z) low.insert(uint32_t(Hash64{}(int64_t(x) << 32 | int64_t(uint32_t(z)))));
    CHECK_EQ(int(low.size()), 1600);
    std::set<uint32_t> strips;  // a strip's key: its group in the high half, its texture's group in the low
    for (uint32_t inside = 0; inside < 500; ++inside) strips.insert(uint32_t(Hash64{}(uint64_t(inside) << 32 | 7u)));
    CHECK_EQ(int(strips.size()), 500);
}
