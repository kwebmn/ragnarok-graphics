#include <cstdint>
#include <cstring>
#include <vector>

#include "core/texture_exe.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// FUN_004176c0's loops as the decompiler gives them, texel by texel: the reference the fast one is held to.
void gameRow(uint32_t format, const uint32_t* src, int step, int count, bool key, uint8_t* dst) {
    for (int i = 0; i < count; ++i) {
        const uint32_t c = src[size_t(i) * size_t(step)];
        if (format == 0) {
            const uint32_t v = (c >> 6 & 0x3e0) + (c >> 3 & 0x1f) + (c >> 0x10 & 0x8000) + (c >> 9 & 0x7c00);
            uint16_t w = 0;
            if (v == 0xfc1f) w = 0;
            else if (!key || (v & 0x7fff) != 0) w = uint16_t(v);
            std::memcpy(dst + 2 * i, &w, 2);
        } else if (format == 1) {
            const uint32_t v = (c >> 0xc & 0xf00) + (c >> 8 & 0xf0) + (c >> 4 & 0xf) + (c >> 0x10 & 0xf000);
            const uint32_t rgb = v & 0xfff;
            uint16_t w = 0;
            if (rgb == 0xf0f) w = 0;
            else if (!key || rgb != 0) w = uint16_t(v);
            std::memcpy(dst + 2 * i, &w, 2);
        } else if (format == 2) {
            const uint32_t v = (c >> 5 & 0x7e0) + (c >> 3 & 0x1f) + (c >> 8 & 0xf800);
            if (!key || v != 0xf81f) {
                const uint16_t w = uint16_t(v);
                std::memcpy(dst + 2 * i, &w, 2);
            }
        } else if (format == 4) {
            const uint32_t w = !key || c != 0 ? c : 0u;
            std::memcpy(dst + 4 * i, &w, 4);
        }
    }
}

}  // namespace

TEST(texture_exe_fast_rows_are_the_games_to_the_bit) {
    uint32_t seed = 2026;
    const auto next = [&seed] { return seed = seed * 1103515245u + 12345u; };
    std::vector<uint32_t> src(4000);
    for (size_t i = 0; i < src.size(); ++i) {
        switch (next() % 6) {  // the cases its keys look at, among any colour
            case 0: src[i] = 0xFFFF00FFu; break;           // magenta
            case 1: src[i] = 0xFF000000u | (next() & 0x070707u); break;  // black in 5 bits
            case 2: src[i] = next() & 0x0F0F0F0Fu; break;  // black in 4 bits, a low alpha
            case 3: src[i] = 0u; break;
            default: src[i] = next(); break;
        }
    }
    for (uint32_t format : {0u, 1u, 2u, 3u, 4u})
        for (int step : {1, 2, 3})
            for (bool key : {false, true})
                for (int count : {0, 1, 7, 8, 9, 15, 16, 17, 333, 1000}) {
                    std::vector<uint8_t> fast(4 * 1001, 0xA5), game(4 * 1001, 0xA5);  // what was there stays
                    texture_exe::convertRow(format, src.data(), step, count, key, fast.data());
                    gameRow(format, src.data(), step, count, key, game.data());
                    CHECK(fast == game);
                }
}
