#include <cstdint>
#include <vector>

#include "core/ai/black_key.hpp"
#include "core/ai/texture_cache.hpp"
#include "core/pack_source.hpp"
#include "core/pixel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A BMP's texels (opaque) as the game makes them A1R5G5B5 and the driver 8-bit again - with the .str animations' black
// key or without (pack_source.hpp's emulation of FUN_004176c0).
std::vector<uint32_t> gameTexels(const std::vector<uint32_t>& bmp, bool blackKey) {
    std::vector<uint32_t> out;
    for (uint32_t c : bmp) out.push_back(toArgb8(PixelFormat::A1R5G5B5, pack::gameA1R5G5B5(c, blackKey)));
    return out;
}

std::vector<uint32_t> glow(int w, int h) {  // an effect's picture: a coloured spot on black, near-black at its rim
    std::vector<uint32_t> p(size_t(w) * size_t(h), 0xFF000000u);
    for (int y = 2; y < h - 2; ++y)
        for (int x = 2; x < w - 2; ++x) {
            const uint32_t v = uint32_t(40 * (x + y)) & 0xFF;
            p[size_t(y) * size_t(w) + size_t(x)] = 0xFF000000u | v << 16 | (v / 2) << 8 | 5;
        }
    return p;
}

}  // namespace

TEST(black_key_texture_gives_back_the_plain_one_and_its_key) {
    std::vector<uint32_t> bmp = glow(8, 8);
    bmp[4 * 8 + 3] = 0xFF060606u;  // near black: no colour left in 5 bits - the key takes it too
    const std::vector<uint32_t> keyed = gameTexels(bmp, true), plainGame = gameTexels(bmp, false);
    std::vector<uint32_t> plain;
    std::vector<uint8_t> shown;
    CHECK(ai::plainOfBlackKey(keyed.data(), 8, 8, plain, shown));
    CHECK(plain == plainGame);  // the builder's texels: its picture's key
    CHECK_EQ(ai::keyName(ai::textureKey(plain.data(), 8, 8)), ai::keyName(ai::textureKey(plainGame.data(), 8, 8)));
    CHECK_EQ(int(shown[0]), 0);          // black: taken away
    CHECK_EQ(int(shown[4 * 8 + 3]), 0);  // near black too
    CHECK_EQ(int(shown[3 * 8 + 3]), 1);  // the spot: seen
}

TEST(black_key_leaves_other_textures_alone) {
    std::vector<uint32_t> plain;
    std::vector<uint8_t> shown;
    const std::vector<uint32_t> bmp = glow(8, 8);
    const std::vector<uint32_t> seen = gameTexels(bmp, false);  // black seen: no such key
    CHECK(!ai::plainOfBlackKey(seen.data(), 8, 8, plain, shown));
    std::vector<uint32_t> opaque(16, 0xFF336699u);  // nothing taken away
    CHECK(!ai::plainOfBlackKey(opaque.data(), 4, 4, plain, shown));
    std::vector<uint32_t> soft(16, 0x88336699u);  // A4R4G4B4's alpha between: its plain alpha is lost
    soft[0] = 0;
    CHECK(!ai::plainOfBlackKey(soft.data(), 4, 4, plain, shown));
}

TEST(black_key_on_the_picture_hides_what_the_game_hides) {
    // 2 x 2 texels, the left column taken away; the picture four times as large.
    const std::vector<uint8_t> shown = {0, 1, 0, 1};
    std::vector<uint32_t> picture(64, 0xFF102030u);
    ai::keyAway(picture, 8, 8, shown, 2, 2);
    CHECK_EQ(picture[0] >> 24, 0u);               // far left: transparent
    CHECK_EQ(picture[0] & 0xFFFFFFu, 0x102030u);  // its colour kept for the filtering
    CHECK_EQ(picture[7] >> 24, 255u);             // far right: seen
    CHECK_EQ(picture[3 * 8 + 1] >> 24, 0u);
    CHECK_EQ(picture[3 * 8 + 6] >> 24, 255u);
    std::vector<uint32_t> odd(9, 0xFFFFFFFFu);  // no whole factor: the nearest texel's
    ai::keyAway(odd, 3, 3, shown, 2, 2);
    CHECK_EQ(odd[0] >> 24, 0u);
    CHECK_EQ(odd[2] >> 24, 255u);
}
