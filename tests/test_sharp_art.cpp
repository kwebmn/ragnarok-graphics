// 3.0.20-beta.68 (the owner's item 7, «умения как в игре, только чётко», the buffs seen in town all day: Increase
// AGI's «AGI UP!», Gloria's nun, Magnificat's angel, Kyrie's bells, Impositio's angel, Assumptio's priest and wings):
// a skill's painted picture - pixel art the game keys out of magenta, 64 to 128 texels, shown two to five times its
// size and smoothed into a blur - found by its texels alone, for a copy four times larger by xBR (core/sharp_frames).
// The texels are the game's: 16-bit (five bits a channel), and a .str animation's pictures have their black keyed out
// too (uaRO.exe FUN_0042c850) - a glow on black comes with the same whole-or-nothing alpha as a painted bell.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/bridge_wire.hpp"
#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "core/sharp_art.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::bridge;

namespace {

constexpr uint32_t kClear = 0x00000000;

// A figure of flat blocks of colour on a keyed ground: a body, a head, a belt - as the nun or a bell
std::vector<uint32_t> figure(int w, int h) {
    std::vector<uint32_t> px(size_t(w) * size_t(h), kClear);
    const auto box = [&](int x0, int y0, int x1, int y1, uint32_t rgb) {
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) px[size_t(y) * size_t(w) + size_t(x)] = 0xFF000000u | rgb;
    };
    box(w / 4, h / 3, 3 * w / 4, h - 2, 0x8A7460);          // the robe
    box(3 * w / 8, h / 8, 5 * w / 8, h / 3, 0xF0C8A8);      // the head
    box(w / 4, h / 2, 3 * w / 4, h / 2 + 2, 0xD8A040);      // the belt
    box(w / 4, h / 3, w / 4 + 1, h - 2, 0x503828);          // its outline
    return px;
}

// A glow as the game holds a .str's: light on black in the game's 32 levels, the black keyed out
std::vector<uint32_t> glowOnKeyedBlack(int side) {
    std::vector<uint32_t> px(size_t(side) * size_t(side), kClear);
    const float mid = float(side) / 2, reach = float(side) * 0.45f;
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            const float dx = float(x) + 0.5f - mid, dy = float(y) + 0.5f - mid;
            const float d = std::sqrt(dx * dx + dy * dy) / reach;
            const int level = d < 1.0f ? int((1.0f - d) * 31.0f) : 0;  // 5 bits
            if (!level) continue;  // black: keyed
            const uint32_t v = uint32_t(level << 3 | level >> 2);
            px[size_t(y) * size_t(side) + size_t(x)] = 0xFF000000u | v << 16 | v << 8 | v;
        }
    return px;
}

// A figure shaded softly: its colour a level brighter every other texel across (the ice blocks, the smooth hearts)
std::vector<uint32_t> softShaded(int w, int h) {
    std::vector<uint32_t> px(size_t(w) * size_t(h), kClear);
    for (int y = h / 4; y < 3 * h / 4; ++y)
        for (int x = w / 8; x < 7 * w / 8; ++x) {
            const int level = 6 + (x - w / 8) / 2 + (y - h / 4) / 4;  // 5 bits, up to 31
            const uint32_t v = uint32_t(std::min(level, 31));
            const uint32_t c = v << 3 | v >> 2;
            px[size_t(y) * size_t(w) + size_t(x)] = 0xFF000000u | (c / 2) << 16 | c << 8 | 0xFFu;
        }
    return px;
}

// A mask with a black outline all round (the dark-outlined pictures: «Failed», the armour, the status signs)
std::vector<uint32_t> darkOutlined(int side) {
    std::vector<uint32_t> px(size_t(side) * size_t(side), kClear);
    for (int y = side / 4; y < 3 * side / 4; ++y)
        for (int x = side / 4; x < 3 * side / 4; ++x) {
            const bool edge = y < side / 4 + 2 || y >= 3 * side / 4 - 2 || x < side / 4 + 2 || x >= 3 * side / 4 - 2;
            px[size_t(y) * size_t(side) + size_t(x)] = edge ? 0xFF101010u : ((x / 6) % 2 ? 0xFFD8B080u : 0xFF906040u);
        }
    return px;
}

// The same shape, every texel a colour of its own: painted in full colour with soft shading (the ice blocks)
std::vector<uint32_t> shaded(int w, int h) {
    std::vector<uint32_t> px = figure(w, h);
    uint32_t n = 0;
    for (uint32_t& p : px)
        if (p >> 24) p = 0xFF000000u | ((n * 7919u) & 0xFFFFFFu), ++n;
    return px;
}

}  // namespace

TEST(sharp_art_takes_pixel_art_on_a_key) {
    const std::vector<uint32_t> nun = figure(64, 128);
    CHECK(sharpart::painted(nun.data(), 64, 128));
    const sharpart::Facts f = sharpart::factsOf(nun.data(), 64, 128);
    CHECK(f.binary);
    CHECK(f.clear > 50 && f.clear < 90);  // percent of its texels keyed out
    CHECK(f.flat > 90);                   // of the painted ones beside one of their own colour
    CHECK_EQ(f.smooth, 0);                // its unlike neighbours: no step of one level
    CHECK(f.big > 90);                    // ... all of them hard steps
    CHECK_EQ(f.rimDark, 0);               // its edge beside the key: its own colours, none of them dim
    const std::vector<uint32_t> bell = figure(64, 64);
    CHECK(sharpart::painted(bell.data(), 64, 64));
    const std::vector<uint32_t> letters = figure(64, 32);  // «AGI UP!»
    CHECK(sharpart::painted(letters.data(), 64, 32));
    // its outline black all round: its edge beside the key is dark as a glow's - but every step in it is a hard one
    const std::vector<uint32_t> mask = darkOutlined(64);
    const sharpart::Facts m = sharpart::factsOf(mask.data(), 64, 64);
    CHECK(m.rimDark > 90);
    CHECK_EQ(m.smooth, 0);
    CHECK(m.big >= 50);
    CHECK(sharpart::painted(mask.data(), 64, 64));
}

TEST(sharp_art_leaves_light_and_soft_pictures) {
    // light on black, no key at all (a ring, a glow, the lightning): the game adds it - smoothed is right
    std::vector<uint32_t> glow(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const int d = (x - 32) * (x - 32) + (y - 32) * (y - 32);
            const uint32_t v = d < 900 ? uint32_t(255 - d * 255 / 900) : 0;
            glow[size_t(y) * 64 + size_t(x)] = 0xFF000000u | v << 16 | v << 8 | v;
        }
    CHECK(!sharpart::painted(glow.data(), 64, 64));
    // a soft edge of its own (a .tga's alpha): not keyed pixel art
    std::vector<uint32_t> soft = figure(64, 64);
    for (size_t i = 0; i < soft.size(); ++i)
        if (soft[i] >> 24) soft[i] = (soft[i] & 0xFFFFFFu) | uint32_t(40 + i % 200) << 24;
    CHECK(!sharpart::painted(soft.data(), 64, 64));
    CHECK(!sharpart::factsOf(soft.data(), 64, 64).binary);
    // painted in full colour, no two texels alike: xBR finds no edges to follow there
    const std::vector<uint32_t> ice = shaded(64, 64);
    CHECK(!sharpart::painted(ice.data(), 64, 64));
    CHECK(sharpart::factsOf(ice.data(), 64, 64).flat < 5);
    // hardly any key (a picture with a keyed corner), hardly anything but the key (a few sparks)
    std::vector<uint32_t> full = figure(64, 64);
    for (size_t i = 0; i < full.size(); ++i)
        if (i % 20) full[i] = 0xFF556677u;
    CHECK(!sharpart::painted(full.data(), 64, 64));
    std::vector<uint32_t> sparks(64 * 64, kClear);
    for (size_t i = 0; i < sparks.size(); i += 97) sparks[i] = 0xFFFFEEAAu, sparks[i + 1] = 0xFFFFEEAAu;
    CHECK(!sharpart::painted(sparks.data(), 64, 64));
    // a .str animation's glow: light on black, its black keyed out by the game - its alpha whole or nothing as a
    // painted picture's, its light in steps of one of the game's 32 levels, fading to the dark at its edge
    const std::vector<uint32_t> keyedGlow = glowOnKeyedBlack(64);
    const sharpart::Facts g = sharpart::factsOf(keyedGlow.data(), 64, 64);
    CHECK(g.binary);
    CHECK(g.clear >= 10 && g.clear <= 97 && g.flat >= 25);  // by the key and the flat colours alone it would pass
    CHECK(g.smooth > 60);                                    // its steps: one level each
    CHECK(g.rimDark > 80);                                   // its edge: the dimmest light
    CHECK(!sharpart::painted(keyedGlow.data(), 64, 64));
    // painted in full colour with soft shading (an ice block), in the game's 32 levels: steps of one level
    const std::vector<uint32_t> softArt = softShaded(64, 64);
    CHECK(sharpart::factsOf(softArt.data(), 64, 64).smooth > 25);
    CHECK(!sharpart::painted(softArt.data(), 64, 64));
    // too large to gain (shown near its own size), too small to be a picture, nothing at all
    const std::vector<uint32_t> large = figure(256, 256);
    CHECK(!sharpart::painted(large.data(), 256, 256));
    const std::vector<uint32_t> tiny = figure(4, 4);
    CHECK(!sharpart::painted(tiny.data(), 4, 4));
    CHECK(!sharpart::painted(nullptr, 64, 64));
    CHECK(!sharpart::painted(glow.data(), 0, 64));
}

TEST(sharp_art_copy_is_four_times_the_picture) {
    const std::vector<uint32_t> bell = figure(64, 64);
    const sharpframes::Copy copy = sharpart::makeCopy(bell.data(), 64, 64);
    CHECK_EQ(copy.width, 256);
    CHECK_EQ(copy.height, 256);
    CHECK_EQ(copy.levels.size(), size_t(3));  // 256, 128 and the picture's own 64
    if (!copy.levels.empty()) {
        CHECK_EQ(copy.levels[0].size(), size_t(256 * 256));
        // the key stays clear, the robe's middle its own colour
        CHECK_EQ(copy.levels[0][0] >> 24, 0u);
        const uint32_t mid = copy.levels[0][size_t(4 * 44) * 256 + size_t(4 * 32)];
        CHECK_EQ(mid, 0xFF8A7460u);
    }
    // a wide strip keeps its shape; what is not painted has no copy
    const std::vector<uint32_t> letters = figure(64, 32);
    const sharpframes::Copy strip = sharpart::makeCopy(letters.data(), 64, 32);
    CHECK_EQ(strip.width, 256);
    CHECK_EQ(strip.height, 128);
    const std::vector<uint32_t> ice = shaded(64, 64);
    CHECK(sharpart::makeCopy(ice.data(), 64, 64).levels.empty());
}

// The copy goes with its draw: the game's own texture stays the draw's (the renderer draws the pack's AI picture of it
// when it has one), the copy's id beside it
TEST(sharp_art_goes_with_its_draw_over_the_bridge) {
    FrameRecord r;
    DrawSetup s;
    DrawCmd d;
    d.state = r.addState(s);
    d.texture[0] = 9;
    d.sharpArt = 77;
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    std::vector<uint8_t> vertices(4 * 32, 1);
    r.draw(d, vertices.data(), 4, nullptr, 0);
    DrawCmd plain = d;
    plain.sharpArt = 0;
    r.draw(plain, vertices.data(), 4, nullptr, 0);
    std::vector<uint8_t> bytes;
    WireWriter w(bytes);
    writeRecord(w, r);
    FrameRecord b;
    WireReader rd(bytes.data(), bytes.size());
    CHECK(readRecord(rd, b));
    CHECK_EQ(b.draws().size(), size_t(2));
    if (b.draws().size() == 2) {
        CHECK_EQ(b.draws()[0].texture[0], 9u);
        CHECK_EQ(b.draws()[0].sharpArt, 77u);
        CHECK(!b.draws()[0].sharpCopy);
        CHECK_EQ(b.draws()[1].sharpArt, 0u);
    }
}

// A copy is 6 ms of the game's thread on average and 23 for a 128 x 128 picture (all 233 of the game's, timed) - a
// frame and a half. Made a few rows a frame instead: the same copy to the texel (xBR reads two texels round each)
TEST(sharp_art_copy_made_in_steps_is_the_same) {
    struct Case {
        std::vector<uint32_t> px;
        int w, h;
    };
    const Case cases[] = {{figure(64, 128), 64, 128}, {darkOutlined(64), 64, 64}, {figure(64, 32), 64, 32},
                          {figure(128, 128), 128, 128}};
    for (const Case& c : cases) {
        const sharpframes::Copy whole = sharpart::makeCopy(c.px.data(), c.w, c.h);
        sharpart::CopyJob job(c.px.data(), c.w, c.h);
        CHECK(!job.done());
        int steps = 0;
        while (!job.step(512) && steps < 1000) ++steps;
        CHECK(job.done());
        // 512 texels a step: its rows in w * h / 512 steps, then a step a half
        CHECK(steps >= c.w * c.h / 512);
        CHECK(steps <= c.w * c.h / 512 + 4);
        const sharpframes::Copy stepped = job.take();
        CHECK_EQ(stepped.width, whole.width);
        CHECK_EQ(stepped.height, whole.height);
        CHECK_EQ(stepped.levels.size(), whole.levels.size());
        CHECK(stepped.levels == whole.levels);
        CHECK(job.take().levels.empty());  // taken once
    }
    // rows of an odd count a step, a step larger than the picture
    const std::vector<uint32_t> nun = figure(64, 128);
    for (int texels : {64 * 5, 64 * 7 + 3, 1, 1 << 20}) {
        sharpart::CopyJob job(nun.data(), 64, 128);
        int steps = 0;
        while (!job.step(texels) && steps < 100000) ++steps;
        CHECK(job.take().levels == sharpart::makeCopy(nun.data(), 64, 128).levels);
    }
    // not a painted picture: nothing to do, no copy
    const std::vector<uint32_t> ice = shaded(64, 64);
    sharpart::CopyJob none(ice.data(), 64, 64);
    CHECK(none.done());
    CHECK(none.step(512));
    CHECK(none.take().levels.empty());
    sharpart::CopyJob empty;
    CHECK(empty.done() && empty.take().levels.empty());
}
