#include <cstring>
#include <string>
#include <vector>

#include "core/texel_check.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.20-beta.82 (R-9VCE45: Assumptio's glow a whitened rectangle of the hotel's bricks round the merchant on
// Direct3D 11): the card's copy of a frame against the game's own texels, the frame's texels by its draw, its BMP

TEST(texel_check_the_same_texels_differ_nowhere) {
    const std::vector<uint32_t> a = {0x00000000u, 0xFF102030u, 0x80405060u, 0xFFFFFFFFu};
    const texelcheck::Compare c = texelcheck::compare(a.data(), a.data(), a.size());
    CHECK(c.texels == 4);
    CHECK(c.differ == 0);
    CHECK(c.cardOnly == 0);
    CHECK(c.gameOnly == 0);
    CHECK(c.cardOnlyMean == 0);
}

TEST(texel_check_a_wall_where_the_frame_is_see_through) {
    // the game's frame: a body in the middle, see-through round it; the card: the same body, bricks round it
    std::vector<uint32_t> game(16, 0x00000000u), card(16, 0xFFC0A090u);
    for (const int i : {5, 6, 9, 10}) game[size_t(i)] = card[size_t(i)] = 0xFF3050A0u;
    const texelcheck::Compare c = texelcheck::compare(game.data(), card.data(), game.size());
    CHECK(c.differ == 12);
    CHECK(c.cardOnly == 12);
    CHECK(c.gameOnly == 0);
    CHECK(c.cardOnlyMean == 0xFFC0A090u);
}

TEST(texel_check_colours_count_only_where_both_are_seen) {
    // see-through on both sides is the same texel whatever colour lies under it; a seen texel of another colour is not
    const std::vector<uint32_t> game = {0x00FF0000u, 0xFF000000u, 0xFF101010u, 0xFF000000u};
    const std::vector<uint32_t> card = {0x0000FF00u, 0xFF0A0A0Au, 0xFF808080u, 0x00000000u};
    const texelcheck::Compare c = texelcheck::compare(game.data(), card.data(), game.size());
    CHECK(c.differ == 2);  // the third by its colour, the fourth by its alpha
    CHECK(c.cardOnly == 0);
    CHECK(c.gameOnly == 1);  // the fourth: seen in the game, see-through on the card
}

TEST(texel_check_a_draw_s_texels_by_its_texture_coordinates) {
    // a transformed vertex: x y z rhw, diffuse, specular, then u v - 24 bytes in, 32 bytes a vertex
    std::vector<uint8_t> bytes(4 * 32, 0);
    const float uv[4][2] = {{0.25f, 0.125f}, {0.5f, 0.125f}, {0.25f, 0.5f}, {0.5f, 0.5f}};
    for (int i = 0; i < 4; ++i) std::memcpy(bytes.data() + size_t(i) * 32 + 24, uv[i], sizeof uv[i]);
    const Rect r = texelcheck::rectOf(bytes, 0, 32, 4, 256, 256);
    CHECK(r.left == 64 && r.top == 32 && r.right == 128 && r.bottom == 128);
    CHECK(texelcheck::rectOf(bytes, 0, 32, 5, 256, 256).empty());  // the fifth vertex is not there
    // a little past the texture's edge: kept inside it
    const float past[2] = {1.02f, -0.01f};
    std::memcpy(bytes.data() + 24, past, sizeof past);
    const Rect inside = texelcheck::rectOf(bytes, 0, 32, 4, 256, 256);
    CHECK(inside.left == 64 && inside.top == 0 && inside.right == 256 && inside.bottom == 128);
}

TEST(texel_check_a_bmp_of_the_texels) {
    const std::vector<uint32_t> t = {0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u, 0x01020304u, 0x05060708u};
    const std::string b = texelcheck::bmpOf(t.data(), 3, 2);
    CHECK(b.size() == 54 + 6 * 4);
    CHECK(b.compare(0, 2, "BM") == 0);
    uint32_t size = 0, offset = 0;
    int32_t w = 0, h = 0;
    uint16_t bpp = 0;
    std::memcpy(&size, b.data() + 2, 4);
    std::memcpy(&offset, b.data() + 10, 4);
    std::memcpy(&w, b.data() + 18, 4);
    std::memcpy(&h, b.data() + 22, 4);
    std::memcpy(&bpp, b.data() + 28, 2);
    CHECK(size == b.size() && offset == 54 && w == 3 && h == -2 && bpp == 32);  // top-down rows
    CHECK(uint8_t(b[54]) == 0x44 && uint8_t(b[55]) == 0x33 && uint8_t(b[56]) == 0x22 && uint8_t(b[57]) == 0x11);
    CHECK(texelcheck::bmpOf(t.data(), 0, 2).empty());
}

// 3.0.22-beta.8 (R-QVKRKE: Assumptio's square on Direct3D 12 too, where the renderer has no game memory to set the
// card against): the card's texels of the glow's frame read alone - the seen ones (alpha >= 128), their bounds, their
// mean, and how many of the frame's outer ring are seen: a character's frame little, a filled rectangle all of it
TEST(texel_check_seen_on_the_card_alone) {
    const int w = 20, h = 30;
    std::vector<uint32_t> sprite(size_t(w) * h, 0u);
    for (int y = 10; y < 25; ++y)
        for (int x = 6; x < 14; ++x) sprite[size_t(y) * w + size_t(x)] = 0xFF804020u;
    sprite[0] = 0x40FFFFFFu;  // faint: not seen
    const texelcheck::Seen a = texelcheck::seenOf(sprite.data(), w, h);
    CHECK_EQ(a.texels, size_t(w * h));
    CHECK_EQ(a.seen, size_t(15 * 8));
    CHECK(a.bounds.left == 6 && a.bounds.top == 10 && a.bounds.right == 14 && a.bounds.bottom == 25);
    CHECK_EQ(a.edge, size_t(2 * w + 2 * (h - 2)));
    CHECK_EQ(a.edgeSeen, size_t(0));
    CHECK_EQ(a.mean, 0xFF804020u);
    const std::vector<uint32_t> filled(size_t(w) * h, 0xFFFFFFFFu);
    const texelcheck::Seen b = texelcheck::seenOf(filled.data(), w, h);
    CHECK_EQ(b.edgeSeen, b.edge);
    CHECK_EQ(b.seen, size_t(w * h));
    const texelcheck::Seen none = texelcheck::seenOf(nullptr, 0, 0);
    CHECK(none.texels == 0 && none.seen == 0 && none.mean == 0);
}
