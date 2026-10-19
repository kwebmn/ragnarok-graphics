#include <cstdint>
#include <vector>

#include "core/ai/recolour.hpp"
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(sprite_cell_texels_as_the_game_writes_them) {
    // 2.5.23: FUN_00417ae0 - index 0 starts a run of transparent texels (its count next, rows go on through it), any
    // other the palette's entry, its low half; the cell's texels past the frame cleared
    uint32_t palette[256] = {};
    palette[1] = 0x12348001u;  // the game's entries: the 16-bit colour in the low half
    palette[2] = 0x0000FC00u;
    // a 3x2 frame: row 0 - entry 1, a run of 2; row 1 - entry 2, a run of 3 (one more than the row has left)
    const uint8_t rle[] = {1, 0, 2, 2, 0, 3, 1};
    std::vector<uint16_t> out;
    CHECK(sprite_exe::cellTexels(rle, sizeof rle, 3, 2, palette, 4, 3, out));
    CHECK_EQ(out.size(), size_t(12));
    const uint16_t want[12] = {0x8001, 0, 0, 0, 0xFC00, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 12; ++i) CHECK_EQ(int(out[size_t(i)]), int(want[i]));
    CHECK(!sprite_exe::cellTexels(rle, 2, 3, 2, palette, 4, 3, out));  // the data ends before the frame does
    CHECK(!sprite_exe::cellTexels(rle, sizeof rle, 5, 2, palette, 4, 3, out));  // a frame wider than its cell
}

TEST(sprite_dye_palettes_compared_by_their_colours) {
    uint32_t own[256], dye[256];
    for (uint32_t i = 0; i < 256; ++i) own[i] = dye[i] = 0x8000u | i;
    dye[7] |= 0x55550000u;  // the high half is not a colour
    CHECK(sprite_exe::sameColours(own, dye));
    dye[9] = 0x8000u | 0x7C00u;
    CHECK(!sprite_exe::sameColours(own, dye));
}

TEST(recolour_moves_a_picture_by_the_dyes_change_by_place) {
    // 2.5.23: a frame's picture in its own colours, the dyed frame's change added - blended between the texels'
    // places as the picture is larger; the transparent texels add nothing
    const uint32_t own[2] = {0xFF102030u, 0xFF102030u}, dyed[2] = {0xFF203040u, 0xFF0A1A2Au};  // +16, -6
    std::vector<uint32_t> pic(32, 0xFF808080u);  // the 2x1 frame 4 times as large
    ai::recolour(pic, 8, 4, own, dyed, 2, 1);
    CHECK_EQ(pic[0], 0xFF909090u);   // the left edge: the first texel's change whole
    CHECK_EQ(pic[7], 0xFF7A7A7Au);   // the right edge: the second's
    const uint32_t mid = pic[3];     // between them: some of each
    CHECK(((mid >> 16) & 255) > 0x7A && ((mid >> 16) & 255) < 0x90);
    CHECK_EQ(pic[24], 0xFF909090u);  // every row alike
    // a transparent texel's colours count for nothing - over it alone the picture stays; its alpha stays
    const uint32_t own2[2] = {0x00000000u, 0xFF102030u}, dyed2[2] = {0x00FFFFFFu, 0xFF203040u};
    std::vector<uint32_t> p2(32, 0x80808080u);
    ai::recolour(p2, 8, 4, own2, dyed2, 2, 1);
    for (int x = 0; x < 8; ++x) CHECK_EQ(p2[size_t(x)], x < 2 ? 0x80808080u : 0x80909090u);
    // clamped at the ends
    const uint32_t own3[1] = {0xFF000000u}, dyed3[1] = {0xFFFF0000u};
    std::vector<uint32_t> p3(16, 0xFFF00000u);
    ai::recolour(p3, 4, 4, own3, dyed3, 1, 1);
    CHECK_EQ(p3[5], 0xFFFF0000u);
}
