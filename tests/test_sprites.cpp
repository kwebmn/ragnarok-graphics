#include <cstdlib>
#include <vector>

#include "core/ai/texture_cache.hpp"
#include "core/sprite_exe.hpp"
#include "core/texture_levels.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(sprite_exe_the_write_call_is_the_known_one) {
    // uaRO.exe 0x40E0D8: E8 03 9A 00 00, the call of 0x417AE0 (CTexture::UpdateSprite) in the sprite cache.
    const uint8_t known[5] = {0xE8, 0x03, 0x9A, 0x00, 0x00};
    CHECK(sprite_exe::isWriteCall(known));
    const uint8_t elsewhere[5] = {0xE8, 0x04, 0x9A, 0x00, 0x00};
    CHECK(!sprite_exe::isWriteCall(elsewhere));
    const uint8_t jump[5] = {0xE9, 0x03, 0x9A, 0x00, 0x00};
    CHECK(!sprite_exe::isWriteCall(jump));
}

TEST(texlevels_sprite_cells_go_with_the_next_upload_of_level_0) {
    // The game writes frames into cells of a cache texture, locking all of it: one upload for all the writes since
    // the last draw, and the cells it filled with it (a cell written twice, once).
    TextureLevels t(256, 256, PixelFormat::A1R5G5B5, 1);
    FrameRecord r;
    t.sync(r, 9);
    CHECK_EQ(r.uploads()[0].slotCount, 0u);
    r.reset();
    t.changed(0, Rect{0, 0, 256, 256});
    t.slotWritten(Rect{64, 0, 128, 128});
    t.changed(0, Rect{0, 0, 256, 256});
    t.slotWritten(Rect{0, 128, 64, 256});
    t.slotWritten(Rect{64, 0, 128, 128});
    t.slotWritten(Rect{300, 0, 400, 64});  // outside: dropped
    t.sync(r, 9);
    CHECK_EQ(r.uploads().size(), size_t(1));
    CHECK_EQ(r.uploads()[0].slotCount, 2u);
    CHECK(r.slots()[r.uploads()[0].firstSlot] == (Rect{64, 0, 128, 128}));
    CHECK(r.slots()[r.uploads()[0].firstSlot + 1] == (Rect{0, 128, 64, 256}));
    r.reset();
    t.changed(0, Rect{0, 0, 8, 8});
    t.sync(r, 9);
    CHECK_EQ(r.uploads()[0].slotCount, 0u);  // gone with the upload before
    CHECK(r.slots().empty());
}

TEST(ai_enlarged_cell_keeps_colours_and_its_cut_out_shape) {
    // A 2x2 cell, one texel hidden: enlarged 4 times its colours stay in the corners, the hidden texel's place stays
    // transparent and takes its neighbours' colour (no black fringe), and every level halves as the GPU does.
    const uint32_t red = 0xFFFF0000, green = 0xFF00FF00, blue = 0xFF0000FF;
    const uint32_t cell[4] = {red, green, blue, 0x00000000};
    std::vector<uint32_t> big;
    ai::enlargeCell(cell, 2, 2, 4, big);
    CHECK_EQ(big.size(), size_t(64));
    CHECK_EQ(big[0], red);
    CHECK_EQ(big[7] >> 24, 255u);                        // green's corner: opaque
    CHECK_EQ(big[63] >> 24, 0u);                         // the hidden texel's corner: transparent
    CHECK(((big[63] >> 8) & 255) > 0 && (big[63] & 255) > 0);  // green and blue from its neighbours, not black
    std::vector<uint32_t> half;
    int w = 0, h = 0;
    ai::halve(big, 8, 8, half, w, h);
    CHECK_EQ(w, 4);
    CHECK_EQ(h, 4);
    CHECK_EQ(half[0], red);
    CHECK_EQ(half[15] >> 24, 0u);
    std::vector<uint32_t> one;
    ai::halve({0xFF000000, 0x00FFFFFF, 0xFF000000, 0xFF000000}, 2, 2, one, w, h);
    CHECK_EQ(w, 1);
    CHECK_EQ(one[0] >> 24, 255u);  // three of four opaque
}

namespace {

// prepareInput before 0.13: every pass over all texels fills the hidden ones next to known ones.
void ringByRing(const uint32_t* argb, int width, int height, std::vector<float>& rgb) {
    const size_t n = size_t(width) * size_t(height);
    std::vector<uint8_t> known(n);
    rgb.assign(n * 3, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        known[i] = (argb[i] >> 24) >= 128;
        for (int c = 0; c < 3; ++c) rgb[i * 3 + size_t(c)] = float((argb[i] >> (16 - 8 * c)) & 0xFF) / 255.0f;
    }
    for (bool changed = true; changed;) {
        changed = false;
        std::vector<size_t> fill;
        std::vector<float> value;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const size_t i = size_t(y) * size_t(width) + size_t(x);
                if (known[i]) continue;
                float s[3] = {0, 0, 0};
                int k = 0;
                const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    const int xx = x + dx[d], yy = y + dy[d];
                    if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
                    const size_t j = size_t(yy) * size_t(width) + size_t(xx);
                    if (!known[j]) continue;
                    for (int c = 0; c < 3; ++c) s[c] += rgb[j * 3 + size_t(c)];
                    ++k;
                }
                if (!k) continue;
                fill.push_back(i);
                for (int c = 0; c < 3; ++c) value.push_back(s[c] / float(k));
            }
        for (size_t f = 0; f < fill.size(); ++f) {
            for (int c = 0; c < 3; ++c) rgb[fill[f] * 3 + size_t(c)] = value[f * 3 + size_t(c)];
            known[fill[f]] = 1;
            changed = true;
        }
    }
}

}  // namespace

TEST(ai_hidden_texels_fill_as_before_and_in_one_walk) {
    // 0.13 fills hidden texels in one walk outwards (sprite cache cells are mostly hidden): exactly what the passes
    // over the whole texture gave.
    std::srand(13);
    for (int round = 0; round < 6; ++round) {
        const int w = 5 + round * 7, h = 3 + round * 5;
        std::vector<uint32_t> argb(size_t(w) * size_t(h));
        for (uint32_t& p : argb) {
            const uint32_t a = (std::rand() % 100) < (round * 15) ? 0xFF000000u : 0u;  // more and more visible
            p = a | uint32_t(std::rand() & 0xFFFFFF);
        }
        std::vector<float> want, planes;
        ringByRing(argb.data(), w, h, want);
        ai::prepareInput(argb.data(), w, h, planes);
        const size_t n = size_t(w) * size_t(h);
        bool same = true;
        for (size_t i = 0; i < n; ++i)
            for (int c = 0; c < 3; ++c) same = same && planes[size_t(c) * n + i] == want[i * 3 + size_t(c)];
        CHECK(same);
    }
}
