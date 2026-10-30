#include <cstdint>
#include <vector>

#include "core/ai/texture_cache.hpp"
#include "core/pack_build.hpp"
#include "core/pack_source.hpp"
#include "core/pixel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void put16(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}

// A bottom-up BMP: 24-bit from rows of 0xRRGGBB (top first), or 8-bit with a palette.
std::vector<uint8_t> bmp(int w, int h, int depth, const std::vector<uint32_t>& top, const std::vector<uint32_t>& pal) {
    const size_t stride = depth == 8 ? (size_t(w) + 3) & ~size_t(3) : (size_t(w) * 3 + 3) & ~size_t(3);
    const uint32_t bits = 0x36 + uint32_t(depth == 8 ? 1024 : 0);
    std::vector<uint8_t> b{'B', 'M'};
    put32(b, uint32_t(bits + stride * size_t(h)));
    put32(b, 0);
    put32(b, bits);
    put32(b, 40);
    put32(b, uint32_t(w));
    put32(b, uint32_t(h));
    put16(b, 1);
    put16(b, uint32_t(depth));
    put32(b, 0);
    put32(b, uint32_t(stride * size_t(h)));
    put32(b, 0);
    put32(b, 0);
    put32(b, depth == 8 ? 256 : 0);
    put32(b, 0);
    if (depth == 8)
        for (int i = 0; i < 256; ++i) put32(b, i < int(pal.size()) ? pal[size_t(i)] : 0);
    for (int y = h - 1; y >= 0; --y) {
        size_t row = 0;
        for (int x = 0; x < w; ++x) {
            const uint32_t v = top[size_t(y * w + x)];
            if (depth == 8) {
                b.push_back(uint8_t(v));
                ++row;
            } else {
                b.push_back(uint8_t(v));
                b.push_back(uint8_t(v >> 8));
                b.push_back(uint8_t(v >> 16));
                row += 3;
            }
        }
        for (; row < stride; ++row) b.push_back(0);
    }
    return b;
}

}  // namespace

TEST(pack_source_world_textures_as_the_game_uploads_them) {
    // 0.43: a BMP into A1R5G5B5 by the game (5 bits a channel, magenta transparent), back to 8 bits by the driver.
    const std::vector<uint8_t> b = bmp(2, 2, 24, {0xFF00FF, 0xFF0000, 0x6496C8, 0x000000}, {});
    pack::Picture p;
    CHECK(pack::worldTexture(b.data(), b.size(), false, p));
    CHECK(p.group == pack::Group::World && p.width == 2 && p.height == 2);
    CHECK_EQ(p.argb[0], 0u);           // magenta
    CHECK_EQ(p.argb[1], 0xFFFF0000u);  // red
    CHECK_EQ(p.argb[2], 0xFF6394CEu);  // 100, 150, 200 at 5 bits a channel
    CHECK_EQ(p.argb[3], 0xFF000000u);  // black stays (no black key)
    CHECK(p.key == ai::keyName(ai::textureKey(p.argb.data(), 2, 2)));
    // An 8-bit one through its palette; no power of two: the game rounds it up and the driver takes no part of one.
    const std::vector<uint8_t> c = bmp(2, 2, 8, {1, 0, 0, 1}, {0x000000, 0x123456});
    CHECK(pack::worldTexture(c.data(), c.size(), false, p));
    CHECK_EQ(p.argb[0], toArgb8(PixelFormat::A1R5G5B5, pack::gameA1R5G5B5(0xFF123456u, false)));
    const std::vector<uint8_t> odd = bmp(3, 2, 24, std::vector<uint32_t>(6, 0x808080), {});
    CHECK(!pack::worldTexture(odd.data(), odd.size(), false, p));
}

TEST(pack_source_tga_with_alpha_as_a4r4g4b4) {
    // A 32-bit TGA (the game's textures with alpha): A4R4G4B4, magenta transparent. RLE, top-down.
    std::vector<uint8_t> t = {0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 32, 0x28};
    t.push_back(0x81);  // a run of two
    for (uint8_t v : {0x00, 0x00, 0xFF, 0x80}) t.push_back(v);  // B, G, R, A: red at half alpha
    pack::Picture p;
    CHECK(pack::worldTexture(t.data(), t.size(), true, p));
    CHECK(p.width == 2 && p.height == 1);
    CHECK_EQ(p.argb[0], 0x88FF0000u);
    CHECK_EQ(p.argb[1], 0x88FF0000u);
}

TEST(pack_source_textures_at_a_lower_detail_as_the_game_takes_them) {
    // 0.45: the game's texture detail (TEXTUREMODE 1, 0) makes a texture a half or a quarter a side, every second or
    // fourth pixel of every second or fourth row from the top-left (FUN_004176c0) - its own key.
    const std::vector<uint8_t> b = bmp(4, 4, 24, std::vector<uint32_t>{0x080808, 0x101010, 0x181818, 0x202020,
                                                                       0x282828, 0x303030, 0x383838, 0x404040,
                                                                       0x484848, 0x505050, 0x585858, 0x606060,
                                                                       0x686868, 0x707070, 0x787878, 0x808080},
                                       {});
    pack::Picture full, half, quarter, none;
    CHECK(pack::worldTexture(b.data(), b.size(), false, full));
    CHECK(pack::lowerDetail(full, 2, half));
    CHECK(half.width == 2 && half.height == 2 && half.group == pack::Group::World);
    CHECK(half.argb[0] == full.argb[0] && half.argb[1] == full.argb[2] && half.argb[2] == full.argb[8] &&
          half.argb[3] == full.argb[10]);
    CHECK(half.key == ai::keyName(ai::textureKey(half.argb.data(), 2, 2)) && half.key != full.key);
    CHECK(pack::lowerDetail(full, 4, quarter));
    CHECK(quarter.width == 1 && quarter.height == 1 && quarter.argb[0] == full.argb[0]);
    CHECK(!pack::lowerDetail(quarter, 2, none));  // no side left
    CHECK(!pack::lowerDetail(full, 3, none));     // the game halves, twice at most
}

TEST(pack_source_big_interface_pictures_also_as_the_games_textures) {
    // 0.45: a big picture of the interface the game draws as a Direct3D texture (a minimap: 128 to 1024 a side, a
    // power of two) - its 16-bit pixels, the key the driver finds it by - besides the crisp text's copy of it.
    std::vector<uint32_t> top(128 * 128);
    for (size_t i = 0; i < top.size(); ++i) top[i] = uint32_t(i * 2654435761u) & 0xFFFFFF;
    const std::vector<uint8_t> big = bmp(128, 128, 24, top, {});
    pack::Source ui{pack::Source::Interface, ai::Kind::Icons};
    const auto two = pack::picturesOf(ui, big.data(), big.size(), pack::Choice());
    CHECK_EQ(two.size(), size_t(2));
    pack::Picture texture;
    CHECK(pack::worldTexture(big.data(), big.size(), false, texture));
    CHECK(two.size() == 2 && two[0].group == pack::Group::Ui && two[1].group == pack::Group::Ui);
    CHECK(two.size() == 2 && two[1].key == texture.key && two[0].key != texture.key);
    // An icon, or a big picture of no power of two (the game's texture of it is not whole): the crisp text's alone.
    const std::vector<uint8_t> icon = bmp(32, 32, 24, std::vector<uint32_t>(32 * 32, 0x123456), {});
    CHECK_EQ(pack::picturesOf(ui, icon.data(), icon.size(), pack::Choice()).size(), size_t(1));
    std::vector<uint32_t> odd(160 * 128);
    for (size_t i = 0; i < odd.size(); ++i) odd[i] = uint32_t(i * 40503u) & 0xFFFFFF;
    const std::vector<uint8_t> wide = bmp(160, 128, 24, odd, {});
    CHECK_EQ(pack::picturesOf(ui, wide.data(), wide.size(), pack::Choice()).size(), size_t(1));
}

TEST(pack_source_refuses_a_tga_past_the_largest_side) {
    // 0.44: as a BMP - past 8192 a side a header is broken, not a texture: refused before anything is made of it.
    std::vector<uint8_t> t = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    put16(t, 8193);
    put16(t, 1);
    t.push_back(24);
    t.push_back(0x20);
    t.resize(t.size() + 8193 * 3, 0x40);
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    bool alpha = false;
    CHECK(!pack::decodeTga(t.data(), t.size(), argb, w, h, alpha));
    t[12] = 0x00;  // 8192: taken
    t[13] = 0x20;
    CHECK(pack::decodeTga(t.data(), t.size(), argb, w, h, alpha) && w == 8192 && h == 1);
}

TEST(pack_source_interface_pictures_as_crisp_text_sees_them) {
    const std::vector<uint8_t> b = bmp(2, 1, 24, {0xFA05FC, 0x203040}, {});  // magenta at 5 bits, a colour
    pack::Picture p;
    CHECK(pack::interfacePicture(b.data(), b.size(), p));
    CHECK(p.group == pack::Group::Ui);
    CHECK_EQ(p.argb[0], 0u);
    CHECK_EQ(p.argb[1], 0xFF203040u);
}

TEST(pack_source_sprite_frames_in_their_cache_cells) {
    // An SPR 2.1: one 3x2 frame - two transparent ones (a run), then palette colours; its palette at the end.
    std::vector<uint8_t> s = {'S', 'P', 1, 2};
    put16(s, 1);  // palette frames
    put16(s, 0);  // 32-bit frames
    put16(s, 3);
    put16(s, 2);
    const std::vector<uint8_t> frame = {0, 2, 1, 2, 1, 1};
    put16(s, uint32_t(frame.size()));
    s.insert(s.end(), frame.begin(), frame.end());
    std::vector<uint8_t> pal(1024, 0);
    pal[4] = 10, pal[5] = 20, pal[6] = 30;    // 1
    pal[8] = 255, pal[9] = 0, pal[10] = 255;  // 2: magenta
    s.insert(s.end(), pal.begin(), pal.end());
    std::vector<pack::Picture> frames;
    CHECK(pack::spriteFrames(s.data(), s.size(), frames));
    CHECK_EQ(int(frames.size()), 1);
    const pack::Picture& f = frames[0];
    CHECK(f.group == pack::Group::Sprites && f.width == 32 && f.height == 32);  // its cache cell
    CHECK_EQ(f.frame, 0);  // 3.0.11-beta.76: the sprite's own frame it is
    CHECK(f.argb[0] == 0 && f.argb[1] == 0);
    CHECK_EQ(f.argb[2], 0xFF081018u);        // 10, 20, 30 at 5 bits
    CHECK_EQ(f.argb[32], 0x00080808u);       // magenta: the game's 0x421
    CHECK_EQ(f.argb[33], 0xFF081018u);
    CHECK_EQ(f.argb[3], 0u);                 // past the frame: nothing
    CHECK(pack::spriteCell(32) == 32 && pack::spriteCell(33) == 64 && pack::spriteCell(256) == 256);
    CHECK_EQ(pack::spriteCell(257), 0);
}

// 3.0.11-beta.76: a sprite's frames - those asked for alone, each with its number
TEST(pack_source_sprite_frames_asked_for) {
    std::vector<uint8_t> s = {'S', 'P', 1, 2};
    put16(s, 2);  // palette frames
    put16(s, 0);
    for (int i = 0; i < 2; ++i) {
        put16(s, 2);
        put16(s, 1);
        const std::vector<uint8_t> frame = {uint8_t(1 + i), 1};
        put16(s, uint32_t(frame.size()));
        s.insert(s.end(), frame.begin(), frame.end());
    }
    std::vector<uint8_t> pal(1024, 0);
    pal[4] = 10, pal[8] = 200;
    s.insert(s.end(), pal.begin(), pal.end());
    std::vector<pack::Picture> frames;
    CHECK(pack::spriteFrames(s.data(), s.size(), frames));
    CHECK(frames.size() == 2u && frames[0].frame == 0 && frames[1].frame == 1 && frames[0].key != frames[1].key);
    const std::vector<int> second = {1};
    std::vector<pack::Picture> one;
    CHECK(pack::spriteFrames(s.data(), s.size(), one, nullptr, &second));
    CHECK(one.size() == 1u && one[0].frame == 1 && one[0].key == frames[1].key);
}
