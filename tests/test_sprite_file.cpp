// 3.0.11-beta.14: the client's sprite files (core/sprite_file) - .spr pictures (palette with RLE, true colour), .act
// frames of layers - and the monsters' sprite names (refindex::spriteNames) for «Довідник»'s pictures.
#include <cstring>
#include <string>

#include "core/reference_index.hpp"
#include "core/sprite_file.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void put(std::string& s, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) s.push_back(char((v >> (8 * i)) & 0xFF));
}

// SP 2.1: one palette picture 3 x 2 (0 1 2 / 2 0 1, packed), one true colour 1 x 1 (red); palette 1 red, 2 green
std::string spr() {
    std::string s = "SP";
    put(s, 1, 1);  // minor
    put(s, 2, 1);  // major
    put(s, 1, 2);
    put(s, 1, 2);
    put(s, 3, 2);
    put(s, 2, 2);
    const unsigned char rle[] = {0, 1, 1, 2, 2, 0, 1, 1};
    put(s, sizeof rle, 2);
    s.append(reinterpret_cast<const char*>(rle), sizeof rle);
    put(s, 1, 2);
    put(s, 1, 2);
    const unsigned char abgr[] = {255, 0, 0, 255};
    s.append(reinterpret_cast<const char*>(abgr), 4);
    std::string pal(1024, '\0');
    pal[4] = char(255);      // 1: red
    pal[2 * 4 + 1] = char(255);  // 2: green
    return s + pal;
}

// AC 2.5: one action, one frame, `layers` layers at (x, 0), the first mirrored when asked
std::string act(int layers, bool mirror, int x2 = 0) {
    std::string s = "AC";
    put(s, 5, 1);
    put(s, 2, 1);
    put(s, 1, 2);
    s.append(10, '\0');
    put(s, 1, 4);  // frames
    s.append(32, '\0');
    put(s, uint32_t(layers), 4);
    for (int l = 0; l < layers; ++l) {
        put(s, uint32_t(l == 0 ? 0 : x2), 4);
        put(s, 0, 4);
        put(s, 0, 4);  // the palette picture
        put(s, l == 0 && mirror ? 1 : 0, 4);
        put(s, 0xFFFFFFFF, 4);  // white, opaque: no tint
        float one = 1;
        uint32_t f = 0;
        std::memcpy(&f, &one, 4);
        put(s, f, 4);
        put(s, f, 4);
        put(s, 0, 4);  // rotation
        put(s, 0, 4);  // palette type
        put(s, 3, 4);
        put(s, 2, 4);
    }
    put(s, 0xFFFFFFFF, 4);  // no sound
    put(s, 0, 4);           // no anchors
    return s;
}

}  // namespace

TEST(sprite_file_reads_pictures_and_frames) {
    spritefile::Sprite sp;
    CHECK(spritefile::readSpr(spr(), sp));
    CHECK_EQ(sp.palette.size(), size_t(1));
    CHECK_EQ(sp.truecolour.size(), size_t(1));
    const spritefile::Image& p = sp.palette[0];
    CHECK(p.w == 3 && p.h == 2);
    CHECK_EQ(p.argb[0], 0u);            // index 0: see-through
    CHECK_EQ(p.argb[1], 0xFFFF0000u);   // red
    CHECK_EQ(p.argb[2], 0xFF00FF00u);   // green
    CHECK_EQ(p.argb[4], 0u);
    CHECK_EQ(sp.truecolour[0].argb[0], 0xFFFF0000u);
    CHECK(!spritefile::readSpr("SP", sp));
    CHECK(!spritefile::readSpr(spr().substr(0, 20), sp));

    spritefile::Act a;
    CHECK(spritefile::readAct(act(1, false), a));
    CHECK_EQ(a.actions.size(), size_t(1));
    CHECK_EQ(a.actions[0][0].layers.size(), size_t(1));
    spritefile::Image im;
    CHECK(spritefile::frameImage(sp, a, 0, 0, im));
    CHECK(im.w == 3 && im.h == 2);
    CHECK_EQ(im.argb[1], 0xFFFF0000u);
    CHECK(spritefile::readAct(act(1, true), a));  // mirrored: the row read right to left
    CHECK(spritefile::frameImage(sp, a, 0, 0, im));
    CHECK_EQ(im.argb[0], 0xFF00FF00u);
    CHECK_EQ(im.argb[2], 0u);
    CHECK(spritefile::readAct(act(2, false, 3), a));  // two layers side by side: 6 wide
    CHECK(spritefile::frameImage(sp, a, 0, 0, im));
    CHECK_EQ(im.w, 6);
    CHECK_EQ(im.argb[4], 0xFFFF0000u);
    CHECK(!spritefile::frameImage(sp, a, 1, 0, im));  // no such action
    CHECK(!spritefile::readAct("AC\x05\x02", a));

    const spritefile::Image big = spritefile::fitted(im, 18, 18);  // grown by whole steps, at most 3 times
    CHECK(big.w == 18 && big.h == 18);
    CHECK_EQ(big.argb[size_t(6 * 18 + 3)], 0xFFFF0000u);  // red, sharp (premultiplied: opaque as is)
    const spritefile::Image small = spritefile::fitted(im, 3, 3);  // shrunk: averaged
    CHECK(small.w == 3 && (small.argb[size_t(1 * 3 + 0)] >> 24) > 0);
}

TEST(reference_sprite_names_of_a_monster) {
    refindex::Mob m;
    m.name = "Fabre";
    m.sprite = "fabre_";
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"fabre_", "fabre"}));
    m.name = "Desert Wolf";
    m.sprite = "m_desert_wolf";
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"m_desert_wolf", "desert_wolf"}));
    m.name = "Archer Skeleton";
    m.sprite = "";  // a pack before 3.0.11-beta.14
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"archer_skeleton"}));
    m.name = "Goblin (Dagger)";
    CHECK(refindex::spriteNames(m) == std::vector<std::string>({"goblin_dagger"}));
}

// 3.0.11-beta.76 (the owner: «иконки ... и всего остального ... грузить в текстур которые обработаны»): a frame put
// together 4 times bigger - a palette picture from the AI pack's frame when there is one, else stretched
TEST(sprite_file_frame_bigger) {
    spritefile::Sprite sp;
    CHECK(spritefile::readSpr(spr(), sp));
    spritefile::Act a;
    CHECK(spritefile::readAct(act(1, false), a));
    spritefile::Image big;  // the pack's picture of palette picture 0: 12 x 8, blue left, white right
    big.w = 12;
    big.h = 8;
    big.argb.assign(96, 0xFF0000FFu);
    for (int y = 0; y < 8; ++y)
        for (int x = 6; x < 12; ++x) big.argb[size_t(y * 12 + x)] = 0xFFFFFFFFu;
    spritefile::Image im;
    CHECK(spritefile::frameImageScaled(sp, a, 0, 0, 4, [&](int i) { return i == 0 ? &big : nullptr; }, im));
    CHECK(im.w == 12 && im.h == 8);
    CHECK_EQ(im.argb[size_t(3 * 12 + 2)], 0xFF0000FFu);
    CHECK_EQ(im.argb[size_t(3 * 12 + 9)], 0xFFFFFFFFu);
    CHECK(spritefile::readAct(act(1, true), a));  // mirrored: white on the left now
    CHECK(spritefile::frameImageScaled(sp, a, 0, 0, 4, [&](int i) { return i == 0 ? &big : nullptr; }, im));
    CHECK_EQ(im.argb[size_t(3 * 12 + 2)], 0xFFFFFFFFu);
    // none from the pack: the frame's own, 4 times (red at x 1: 4..7)
    CHECK(spritefile::readAct(act(1, false), a));
    CHECK(spritefile::frameImageScaled(sp, a, 0, 0, 4, [](int) { return nullptr; }, im));
    CHECK(im.w == 12 && im.h == 8);
    CHECK_EQ(im.argb[size_t(1 * 12 + 5)], 0xFFFF0000u);
    CHECK_EQ(im.argb[size_t(1 * 12 + 1)], 0u);
}
