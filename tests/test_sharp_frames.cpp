#include <cstring>
#include <vector>

#include "core/bridge_wire.hpp"
#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/sharp_frames.hpp"
#include "core/sprite_cells.hpp"
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::bridge;

// 3.0.18-beta.178 (the owner: «делай 1, только чтобы было полное соответсвие с игрой» - «как в игре, только чётко»):
// the damage digits, Miss, Guard, Critical!!, Lucky and the crits' stars - uaRO.exe's own sprite frames, drawn from
// copies made four times larger by xBR; the game keeps placing, colouring and moving them.

namespace {

// A TL1 vertex: x, y, z, rhw, diffuse, specular, u, v (32 bytes)
std::vector<uint8_t> quad(const float uv[4][2]) {
    std::vector<uint8_t> v(4 * 32, 0);
    for (int i = 0; i < 4; ++i) std::memcpy(&v[size_t(i) * 32 + 24], uv[i], 8);
    return v;
}

float uOf(const std::vector<uint8_t>& v, int i) {
    float f = 0;
    std::memcpy(&f, &v[size_t(i) * 32 + 24], 4);
    return f;
}

float vOf(const std::vector<uint8_t>& v, int i) {
    float f = 0;
    std::memcpy(&f, &v[size_t(i) * 32 + 28], 4);
    return f;
}

bool near(float a, float b) { return a > b - 1e-4f && a < b + 1e-4f; }

}  // namespace

TEST(sharp_frames_the_number_sprites_by_their_files) {
    // uaRO.exe loads them as 이팩트\숫자.spr (the digits), 이팩트\msg.spr and 이팩트\bluemsg.spr (Miss, Guard,
    // Critical!!, the crits' stars, Lucky)
    CHECK(sprite_exe::numberFile("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\\xBC\xFD\xC0\xDA.spr"));  // 숫자.spr
    CHECK(sprite_exe::numberFile("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\msg.spr"));
    CHECK(sprite_exe::numberFile("DATA/SPRITE/\xC0\xCC\xC6\xD1\xC6\xAE/BlueMsg.SPR"));
    CHECK(!sprite_exe::numberFile("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\miss.spr"));  // not this exe's
    CHECK(!sprite_exe::numberFile("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\emotion.spr"));
    CHECK(!sprite_exe::numberFile("data\\sprite\\npc\\msg.spr"));  // another folder's file of that name
    CHECK(!sprite_exe::numberFile("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\xmsg.spr"));
    CHECK(!sprite_exe::numberFile(nullptr));
}

TEST(sharp_frames_a_cell_keeps_its_frame_s_sharp_copy_until_written_over) {
    sprite_cells::Cells c;
    c.written(Rect{64, 0, 96, 32}, uint8_t(sprite_exe::Source::Effect), false, 0x1234, 10, 13);
    c.written(Rect{0, 0, 32, 32}, uint8_t(sprite_exe::Source::Monster), false);
    const sprite_cells::Cell* n = c.find(70.5f, 6.0f);
    CHECK(n != nullptr);
    CHECK(n && n->sharp == 0x1234 && n->frameW == 10 && n->frameH == 13);
    CHECK(n && n->rect == (Rect{64, 0, 96, 32}));
    const sprite_cells::Cell* m = c.find(5.0f, 5.0f);
    CHECK(m && m->sharp == 0);
    CHECK(c.find(200.0f, 200.0f) == nullptr);
    // the numbers' cells alone: what a sprite's draw asks first
    CHECK(c.anyNumber());
    CHECK(c.number(70.5f, 6.0f) && c.number(70.5f, 6.0f)->sharp == 0x1234);
    CHECK(c.number(5.0f, 5.0f) == nullptr);  // the monster's: no copy
    c.written(Rect{64, 0, 96, 32}, uint8_t(sprite_exe::Source::Effect), false);  // another frame in that cell
    CHECK(c.find(70.5f, 6.0f) && c.find(70.5f, 6.0f)->sharp == 0);
    CHECK(c.number(70.5f, 6.0f) == nullptr);
    CHECK(!c.anyNumber());
    CHECK_EQ(int(c.at(70.5f, 6.0f, nullptr)), int(sprite_exe::Source::Effect));  // at() as before
}

TEST(sharp_frames_the_numbers_cells_go_with_the_oldest) {
    // a cache filled with ever new frames: a number's cell pushed out past the bound is no number any more
    sprite_cells::Cells c;
    c.written(Rect{0, 0, 2, 2}, uint8_t(sprite_exe::Source::Effect), false, 7, 2, 2);
    for (int i = 1; i <= sprite_cells::Cells::kMost; ++i)
        c.written(Rect{i * 2, 0, i * 2 + 2, 2}, uint8_t(sprite_exe::Source::Monster), false);
    CHECK(c.find(1, 1) == nullptr);
    CHECK(c.number(1, 1) == nullptr);
    CHECK(!c.anyNumber());
}

TEST(sharp_frames_a_copy_larger_with_its_smaller_levels) {
    // a digit's frame: 10 x 13, a white body in a black outline, transparent round it
    const int w = 10, h = 13;
    std::vector<uint32_t> argb(size_t(w) * h, 0x00000000u);
    for (int y = 2; y < 11; ++y)
        for (int x = 2; x < 8; ++x) {
            const bool edge = x == 2 || x == 7 || y == 2 || y == 10;
            argb[size_t(y) * w + x] = edge ? 0xFF000000u : 0xFFFFFFFFu;
        }
    // 3.0.18-beta.185: a small frame (a digit, on a 4K screen 12 times its size) xBR'd twice - 16 times its size
    const sharpframes::Copy c = sharpframes::makeCopy(argb.data(), w, h);
    CHECK_EQ(c.width, 160);
    CHECK_EQ(c.height, 208);
    CHECK_EQ(int(c.levels.size()), 5);  // 16x, 8x, 4x, 2x and the game's own size: the card picks by how large it shows
    CHECK_EQ(c.levels[0].size(), size_t(160 * 208));
    CHECK_EQ(c.levels[2].size(), size_t(40 * 52));
    CHECK_EQ(c.levels[4].size(), size_t(10 * 13));
    CHECK_EQ(c.levels[0][0] >> 24, 0u);                         // the transparent corner stays so
    CHECK_EQ(c.levels[0][size_t(96) * 160 + 80], 0xFFFFFFFFu);  // the body's middle: the game's white
    CHECK_EQ(c.levels[4][size_t(6) * 10 + 5], 0xFFFFFFFFu);
    CHECK_EQ(c.levels[4][0] >> 24, 0u);
    // a larger frame (Critical!!, the crit's star): four times, as before
    std::vector<uint32_t> wide(size_t(70) * 60, 0xFFFFFFFFu);
    const sharpframes::Copy big = sharpframes::makeCopy(wide.data(), 70, 60);
    CHECK(big.width == 280 && big.height == 240 && big.levels.size() == 3);
    // an empty or too large frame: no copy
    CHECK(sharpframes::makeCopy(argb.data(), 0, 13).levels.empty());
    CHECK(sharpframes::makeCopy(argb.data(), 2000, 13).levels.empty());
}

TEST(sharp_frames_one_key_a_picture) {
    std::vector<uint32_t> a(8 * 8, 0xFFFFFFFFu), b = a;
    CHECK(sharpframes::keyOf(a.data(), 8, 8) != 0);
    CHECK_EQ(sharpframes::keyOf(a.data(), 8, 8), sharpframes::keyOf(b.data(), 8, 8));
    b[9] = 0xFF000000u;
    CHECK(sharpframes::keyOf(a.data(), 8, 8) != sharpframes::keyOf(b.data(), 8, 8));
    CHECK(sharpframes::keyOf(a.data(), 8, 8) != sharpframes::keyOf(a.data(), 4, 16));  // its size counts too
}

TEST(sharp_frames_a_quad_s_coordinates_from_the_cache_into_the_copy) {
    // the frame 10 x 13 at (64, 0) of a 256 x 256 cache, drawn as the game draws it (a strip: 0 1 2 3)
    const Rect frame{64, 0, 74, 13};
    const float uv[4][2] = {{64 / 256.f, 0}, {74 / 256.f, 0}, {64 / 256.f, 13 / 256.f}, {74 / 256.f, 13 / 256.f}};
    std::vector<uint8_t> v = quad(uv);
    CHECK(sharpframes::remap(v.data(), 4, 32, 256, 256, frame));
    CHECK(near(uOf(v, 0), 0) && near(vOf(v, 0), 0));
    CHECK(near(uOf(v, 1), 1) && near(vOf(v, 1), 0));
    CHECK(near(uOf(v, 2), 0) && near(vOf(v, 2), 1));
    CHECK(near(uOf(v, 3), 1) && near(vOf(v, 3), 1));
    // mirrored (an act's layer turned): its u runs back
    const float mirrored[4][2] = {{74 / 256.f, 0}, {64 / 256.f, 0}, {74 / 256.f, 13 / 256.f}, {64 / 256.f, 13 / 256.f}};
    v = quad(mirrored);
    CHECK(sharpframes::remap(v.data(), 4, 32, 256, 256, frame));
    CHECK(near(uOf(v, 0), 1) && near(uOf(v, 1), 0));
    // a texel's half past the frame (the game's own rounding) is still the frame
    const float loose[4][2] = {
        {63.6f / 256.f, 0}, {74.4f / 256.f, 0}, {63.6f / 256.f, 13 / 256.f}, {74.4f / 256.f, 13 / 256.f}};
    v = quad(loose);
    CHECK(sharpframes::remap(v.data(), 4, 32, 256, 256, frame));
    // past the frame: not the frame's draw - left as the game made it
    const float wide[4][2] = {{64 / 256.f, 0}, {80 / 256.f, 0}, {64 / 256.f, 13 / 256.f}, {80 / 256.f, 13 / 256.f}};
    v = quad(wide);
    const std::vector<uint8_t> before = v;
    CHECK(!sharpframes::remap(v.data(), 4, 32, 256, 256, frame));
    CHECK(v == before);
    CHECK(!sharpframes::remap(v.data(), 4, 32, 0, 256, frame));  // a texture of no size
}

TEST(sharp_frames_cross_the_bridge) {
    // a draw from a sharp copy and the copy's own upload reach the 64-bit renderer as the driver recorded them
    FrameRecord r;
    DrawSetup s;
    DrawCmd d;
    d.state = r.addState(s);
    d.texture[0] = 9;
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    d.sharpCopy = true;
    std::vector<uint8_t> vertices(4 * 32, 1);
    r.draw(d, vertices.data(), 4, nullptr, 0);
    const uint32_t pixels[4] = {1, 2, 3, 4};
    r.uploadOwn(9, 0, Rect{0, 0, 2, 2}, pixels, {{Rect{0, 0, 2, 2}, uint8_t(sprite_exe::Source::Effect)}});
    r.upload(10, 0, Rect{0, 0, 2, 2}, pixels);
    std::vector<uint8_t> bytes;
    WireWriter w(bytes);
    writeRecord(w, r);
    FrameRecord b;
    WireReader rd(bytes.data(), bytes.size());
    CHECK(readRecord(rd, b));
    CHECK(b.draws().size() == 1 && b.draws()[0].sharpCopy);
    CHECK(b.uploads().size() == 2 && b.uploads()[0].own == 1 && b.uploads()[1].own == 0);
    CHECK_EQ(b.uploads()[0].slotCount, 1u);
    CHECK_EQ(int(b.slots()[0].source), int(sprite_exe::Source::Effect));
}
