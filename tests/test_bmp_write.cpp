#include <cstdint>
#include <vector>

#include "core/bmp_write.hpp"
#include "core/pack_source.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(bmp_write_keeps_the_pixels_and_their_alpha) {
    // 0.49.1: the frames the pack had no picture of go into uaro-graphics\missing\ as 32-bit BMPs, for the developers.
    const std::vector<uint32_t> argb = {0xFF102030u, 0x00000000u, 0x80405060u, 0xFFFFFFFFu, 0xFF000000u, 0x7F010203u};
    const std::vector<uint8_t> b = bmp::write32(argb.data(), 3, 2);
    CHECK_EQ(b.size(), size_t(54 + 6 * 4));
    CHECK(b[0] == 'B' && b[1] == 'M');
    // Top-down (a negative height), BGRA as stored.
    CHECK(b[54] == 0x30 && b[55] == 0x20 && b[56] == 0x10 && b[57] == 0xFF);
    CHECK(b[54 + 5 * 4 + 3] == 0x7F);
    CHECK(bmp::write32(nullptr, 0, 0).empty());
}
