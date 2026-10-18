#include "core/full_colour.hpp"
#include "core/texture_levels.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(texlevels_new_levels_are_uploaded_whole_once) {
    TextureLevels t(8, 4, PixelFormat::A1R5G5B5, 3);
    CHECK_EQ(t.levels(), 3);
    CHECK_EQ(t.level(2).width(), 2);
    CHECK_EQ(t.level(2).height(), 1);
    CHECK(t.pending());
    FrameRecord r;
    t.sync(r, 7);
    CHECK_EQ(r.uploads().size(), size_t(3));
    CHECK(r.uploads()[0].rect == (Rect{0, 0, 8, 4}));
    CHECK_EQ(r.uploads()[1].level, 1u);
    CHECK_EQ(r.uploads()[2].texture, 7u);
    CHECK_EQ(r.pixels()[0], 0u);  // zeroed memory: transparent black (the glyph atlas relies on it)
    CHECK(!t.pending());
    t.sync(r, 7);
    CHECK_EQ(r.uploads().size(), size_t(3));  // nothing changed since
}

TEST(texlevels_changes_merge_into_one_upload_per_level) {
    TextureLevels t(8, 8, PixelFormat::A1R5G5B5, 1);
    FrameRecord r;
    t.sync(r, 1);
    r.reset();
    t.level(0).setPixel(1, 1, 0xFFFF);
    t.changed(0, Rect{1, 1, 2, 2});
    t.changed(0, Rect{4, 2, 6, 3});
    t.sync(r, 1);
    CHECK_EQ(r.uploads().size(), size_t(1));
    CHECK(r.uploads()[0].rect == (Rect{1, 1, 6, 3}));
    CHECK_EQ(r.pixels()[r.uploads()[0].pixels], 0xFFFFFFFFu);
}

TEST(texlevels_change_outside_the_level_is_clipped) {
    TextureLevels t(4, 4, PixelFormat::A4R4G4B4, 2);
    FrameRecord r;
    t.sync(r, 1);
    r.reset();
    t.changed(1, Rect{1, 1, 9, 9});  // level 1 is 2x2
    t.sync(r, 1);
    CHECK_EQ(r.uploads().size(), size_t(1));
    CHECK(r.uploads()[0].rect == (Rect{1, 1, 2, 2}));
}

TEST(texlevels_colour_key_sends_keyed_texels_as_alpha_zero) {
    TextureLevels t(2, 1, PixelFormat::R5G6B5, 1);
    t.level(0).setPixel(0, 0, 0xF81F);
    t.level(0).setPixel(1, 0, 0x07E0);
    FrameRecord r;
    t.sync(r, 1);
    CHECK_EQ(r.pixels()[0], 0xFFFF00FFu);
    r.reset();
    t.setKey(ColorKey{true, 0xF81F, 0xF81F});
    CHECK(t.pending());  // the alpha of the keyed texels changed: everything goes again
    t.sync(r, 1);
    CHECK_EQ(r.pixels()[0], 0u);
    CHECK_EQ(r.pixels()[1], 0xFF00FF00u);
}

namespace {

// What CTexture::Update writes of a picture's texel into a texture of format f (FUN_004176c0, its keys aside).
void gameWrites(TextureLevels& t, int x, int y, uint32_t argb) {
    t.level(0).setPixel(x, y, fromArgb8(t.level(0).format(), argb));
}

}  // namespace

TEST(texlevels_full_colour_goes_up_where_the_game_wrote_it) {
    TextureLevels t(4, 1, PixelFormat::A4R4G4B4, 1);
    FrameRecord r;
    t.sync(r, 1);
    r.reset();
    const uint32_t full[4] = {0x2AFFC080u, 0x9C123456u, 0x00FFFFFFu, 0xF0ABCDEFu};
    for (int x = 0; x < 4; ++x) gameWrites(t, x, 0, full[x]);
    CHECK(t.fullColour(Rect{0, 0, 4, 1}, full, false));
    CHECK(TextureLevels::fullBytes() >= 16);
    t.level(0).setPixel(3, 0, 0x1234);  // written again by another way before the upload: the game's texel
    t.sync(r, 1);
    CHECK_EQ(r.uploads().size(), size_t(1));
    CHECK_EQ(int(r.uploads()[0].fullFrom), int(PixelFormat::A4R4G4B4));
    const uint32_t* up = r.pixels().data() + r.uploads()[0].pixels;
    CHECK_EQ(up[0], full[0]);  // 42 of alpha, not 34
    CHECK_EQ(up[1], full[1]);
    CHECK_EQ(up[2], toArgb8(PixelFormat::A4R4G4B4, 0x0FFF));  // nothing to show: as the game sends it
    CHECK_EQ(up[3], toArgb8(PixelFormat::A4R4G4B4, 0x1234));
    CHECK_EQ(TextureLevels::fullBytes(), size_t(0));  // gone with the upload
    for (int x = 0; x < 4; ++x)  // the AI textures' key: as the 16-bit texels were
        CHECK_EQ(full_colour::gameTexel(PixelFormat::A4R4G4B4, up[x]),
                 toArgb8(PixelFormat::A4R4G4B4, t.level(0).pixel(x, 0)));
}

TEST(texlevels_full_colour_makes_the_mips_too) {
    TextureLevels t(2, 2, PixelFormat::A1R5G5B5, 2);
    FrameRecord r;
    t.sync(r, 1);
    r.reset();
    const uint32_t full[4] = {0xFF0A0B0Cu, 0xFF0E0F10u, 0xFF121314u, 0xFF161718u};
    for (int i = 0; i < 4; ++i) gameWrites(t, i % 2, i / 2, full[i]);
    t.level(1).setPixel(0, 0, 0x8000 | 0x0421);  // the game's mip: shown, its colour 16-bit
    t.changed(1, Rect{0, 0, 1, 1});
    CHECK(t.fullColour(Rect{0, 0, 2, 2}, full, true));
    t.sync(r, 1);
    CHECK_EQ(r.uploads().size(), size_t(2));
    CHECK_EQ(r.uploads()[1].level, 1u);
    CHECK_EQ(r.pixels()[r.uploads()[1].pixels], 0xFF101112u);  // the four full colours' mean
    const uint32_t* up = r.pixels().data() + r.uploads()[0].pixels;
    CHECK_EQ(up[3], full[3]);
}

TEST(texlevels_full_colour_only_for_16_bit_textures_inside_them) {
    TextureLevels t(2, 2, PixelFormat::A8R8G8B8, 1);
    const uint32_t full[4] = {1, 2, 3, 4};
    CHECK(!t.fullColour(Rect{0, 0, 2, 2}, full, false));  // full colour already
    TextureLevels s(2, 2, PixelFormat::R5G6B5, 1);
    CHECK(!s.fullColour(Rect{1, 1, 3, 3}, full, false));  // not inside
    {
        TextureLevels k(2, 2, PixelFormat::R5G6B5, 1);
        CHECK(k.fullColour(Rect{0, 0, 2, 2}, full, false));
    }
    CHECK_EQ(TextureLevels::fullBytes(), size_t(0));  // gone with its texture
}
