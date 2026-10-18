#include <cstdint>
#include <vector>

#include "core/full_colour.hpp"
#include "core/pixel.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(full_colour_writes_where_the_game_writes) {
    full_colour::Write w = full_colour::writeOf(0, 0, 100, 60, 128, 64);  // a picture in a texture's corner
    CHECK(w.rect == (Rect{0, 0, 100, 60}));
    CHECK_EQ(w.stepX, 1);
    w = full_colour::writeOf(0, 0, 256, 256, 128, 128);  // half the texture detail: every second texel
    CHECK(w.rect == (Rect{0, 0, 128, 128}));
    CHECK_EQ(w.stepX, 2);
    CHECK_EQ(w.stepY, 2);
    w = full_colour::writeOf(32, 10, 300, 40, 128, 64);  // the smallest step it fits by, the place divided by it
    CHECK_EQ(w.stepX, 3);
    CHECK_EQ(w.stepY, 1);
    CHECK(w.rect == (Rect{10, 10, 110, 50}));
    CHECK(full_colour::writeOf(0, 0, 0, 8, 8, 8).rect.empty());
    const std::vector<uint32_t> picture = {1, 2, 3, 4, 5, 6, 7, 8};  // 4 x 2, pitch 4
    std::vector<uint32_t> texels;
    w = full_colour::writeOf(0, 0, 4, 2, 2, 2);
    full_colour::texelsOf(picture.data(), 4, w, texels);
    CHECK(texels == (std::vector<uint32_t>{1, 3, 5, 7}));  // every second across, both rows (2 fits)
}

TEST(full_colour_merge_keeps_what_the_16_bit_texel_says) {
    const PixelFormat formats[] = {PixelFormat::A1R5G5B5, PixelFormat::X1R5G5B5, PixelFormat::A4R4G4B4,
                                   PixelFormat::R5G6B5};
    uint32_t seed = 12345;
    const auto next = [&seed] { return seed = seed * 1664525u + 1013904223u; };
    for (PixelFormat f : formats)
        for (int i = 0; i < 4000; ++i) {
            const uint32_t full = next() | (f == PixelFormat::A4R4G4B4 ? 0u : 0x80000000u);
            const bool same = i % 3 != 0;  // the texel the game made of it, or one written again since
            const uint32_t raw = same ? fromArgb8(f, full) : next() & 0xFFFF;
            const uint32_t argb = i % 7 == 0 ? 0u : toArgb8(f, raw);  // now and then keyed away
            const uint32_t up = full_colour::merge(f, raw, argb, full);
            CHECK_EQ(full_colour::gameTexel(f, up), argb);  // the AI textures' keys stay
            if (argb >> 24 && (fromArgb8(f, full) & 0x7FFF) == (raw & 0x7FFF) && (f != PixelFormat::A1R5G5B5 ||
                                                                                 fromArgb8(f, full) == raw))
                CHECK_EQ(up & 0xFFFFFFu, full & 0xFFFFFFu);  // its own colour
            else
                CHECK_EQ(up, argb);
        }
}

TEST(full_colour_gives_a_gradient_back_its_shades) {
    // A glow's alpha in A4R4G4B4: 16 shades; its picture's 256 come back.
    int shades16 = 0, shades = 0;
    uint32_t last16 = ~0u, last = ~0u;
    for (uint32_t a = 16; a < 256; ++a) {
        const uint32_t full = a << 24 | 0xFFC080u;
        const uint32_t raw = fromArgb8(PixelFormat::A4R4G4B4, full);
        const uint32_t game = toArgb8(PixelFormat::A4R4G4B4, raw);
        const uint32_t up = full_colour::merge(PixelFormat::A4R4G4B4, raw, game, full);
        shades16 += game != last16;
        shades += up != last;
        last16 = game;
        last = up;
        CHECK_EQ(up, full);
    }
    CHECK_EQ(shades16, 15);
    CHECK_EQ(shades, 240);
}

TEST(full_colour_mips_by_alpha) {
    // Two transparent texels lend no colour; the alpha is the mean.
    const std::vector<uint32_t> in = {0xFF804020u, 0x00FFFFFFu, 0x00000000u, 0xFF804020u};
    std::vector<uint32_t> out;
    int w = 0, h = 0;
    full_colour::halve(in, 2, 2, out, w, h);
    CHECK_EQ(w, 1);
    CHECK_EQ(h, 1);
    CHECK_EQ(out[0], 0x80804020u);
    full_colour::halve(std::vector<uint32_t>(4, 0x00123456u), 2, 2, out, w, h);
    CHECK_EQ(out[0], 0u);
    // The game's mip tells what shows; A1R5G5B5 its alpha, A4R4G4B4 the full colours'.
    CHECK_EQ(full_colour::mergeMip(PixelFormat::A1R5G5B5, 0u, 0xFF112233u), 0u);
    CHECK_EQ(full_colour::mergeMip(PixelFormat::A1R5G5B5, 0xFF102030u, 0x80112233u), 0xFF112233u);
    CHECK_EQ(full_colour::mergeMip(PixelFormat::A4R4G4B4, 0x88102030u, 0x80112233u), 0x80112233u);
    CHECK_EQ(full_colour::mergeMip(PixelFormat::A4R4G4B4, 0x88102030u, 0u), 0x88102030u);
}
