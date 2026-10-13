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
