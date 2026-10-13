#include <vector>

#include "core/twin.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
const uint32_t kMagenta = 0xFFFF00FF;  // the game's transparent clear
uint32_t at(const TwinPicture& t, int x, int y) { return t.twin()[size_t(y) * size_t(t.twinWidth()) + size_t(x)]; }
}  // namespace

TEST(twin_transparency_is_the_game_upload_rule) {
    CHECK(transparentAtUpload(0xFFFF00FF));
    CHECK(transparentAtUpload(0x00FF00FF));   // GDI writes alpha 0; the game forces 0xFF before the upload
    CHECK(transparentAtUpload(0xFFF807F8));   // magenta in 5 bits
    CHECK(!transparentAtUpload(0xFFF808F8));  // green 0x08 is 1 in 5 bits
    CHECK(!transparentAtUpload(0xFFF0F0F0));
    CHECK_EQ(twinValue(0x00123456), 0xFF123456u);
    CHECK_EQ(twinValue(kMagenta), 0u);
}

TEST(twin_over_is_premultiplied) {
    CHECK_EQ(over(0xFF102030, 0xFF405060), 0xFF102030u);
    CHECK_EQ(over(0, 0xFF405060), 0xFF405060u);
    CHECK_EQ(over(0x80400000, 0xFF0000FF), 0xFF40007Fu);  // half red over blue
    CHECK_EQ(over(0x80400000, 0), 0x80400000u);
}

TEST(twin_picture_enlarges_the_game_pixels) {
    // 3 x 2 at 1.75: columns [0,1) [1,3) [3,5), rows [0,1) [1,3); the magenta pixel is transparent.
    const std::vector<uint32_t> dib = {0xFF0000AA, 0xFF0000BB, kMagenta, 0xFF0000DD, 0xFF0000EE, 0xFF0000FF};
    const TwinPicture t(dib.data(), 3, 2, 1.75);
    CHECK_EQ(t.twinWidth(), 5);
    CHECK_EQ(t.twinHeight(), 3);
    CHECK_EQ(at(t, 0, 0), 0xFF0000AAu);
    CHECK_EQ(at(t, 1, 0), 0xFF0000BBu);
    CHECK_EQ(at(t, 2, 0), 0xFF0000BBu);
    CHECK_EQ(at(t, 3, 0), 0u);
    CHECK_EQ(at(t, 0, 1), 0xFF0000DDu);
    CHECK_EQ(at(t, 0, 2), 0xFF0000DDu);
    CHECK_EQ(at(t, 4, 2), 0xFF0000FFu);
}

TEST(twin_picture_overwrite_erases_what_sync_keeps) {
    const std::vector<uint32_t> dib(16, 0xFF808080);
    TwinPicture t(dib.data(), 4, 4, 2.0);
    t.twin()[size_t(2 * t.twinWidth() + 2)] = 0xFF000000;  // a letter's pixel in game pixel (1, 1)
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 4, 4}), 0);      // the game's pixels did not change: sync keeps it
    CHECK_EQ(at(t, 2, 2), 0xFF000000u);
    t.overwrite(dib.data(), Rect{1, 1, 2, 2});              // a Clear of that pixel erases it
    CHECK_EQ(at(t, 2, 2), 0xFF808080u);
    t.overwrite(dib.data(), Rect{-5, -5, 50, 50});          // cut to the picture
    CHECK_EQ(at(t, 7, 7), 0xFF808080u);
}

TEST(twin_picture_sync_takes_changed_colours_only) {
    std::vector<uint32_t> dib(4, 0xFF808080);
    TwinPicture t(dib.data(), 2, 2, 2.0);
    dib[1] = 0xFF00FF00;  // a new colour
    dib[2] = 0x00808080;  // the alpha byte only: the game's own business before an upload
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 2, 2}), 1);
    CHECK_EQ(at(t, 2, 0), 0xFF00FF00u);
    CHECK_EQ(at(t, 3, 1), 0xFF00FF00u);
    CHECK_EQ(at(t, 0, 2), 0xFF808080u);
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 2, 2}), 0);  // the copy followed
}

TEST(twin_picture_absorb_keeps_the_twin) {
    std::vector<uint32_t> dib(4, 0xFF808080);
    TwinPicture t(dib.data(), 2, 2, 2.0);
    dib[0] = 0x00000000;  // the game's small letter
    t.absorb(dib.data(), Rect{0, 0, 2, 2});
    CHECK_EQ(at(t, 0, 0), 0xFF808080u);  // the twin has its own letters
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 2, 2}), 0);
}

TEST(twin_picture_copy_from_lays_the_child_over) {
    // A 3 x 2 child at (1, 1) of an 8 x 4 parent, K = 1.75: its magenta pixel keeps the parent, its red ones cover it,
    // and a half-covered letter edge on its transparent part blends.
    std::vector<uint32_t> parent(32, 0xFF0000FF);
    TwinPicture p(parent.data(), 8, 4, 1.75);
    const std::vector<uint32_t> kid = {kMagenta, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000};
    TwinPicture c(kid.data(), 3, 2, 1.75);
    c.twin()[0] = 0x80400000;  // twin pixel (0, 0): the block of the magenta game pixel
    for (int y = 1; y < 3; ++y)  // what the game's copy does to the parent's pixels
        for (int x = 1; x < 4; ++x) {
            const uint32_t v = kid[size_t(y - 1) * 3 + size_t(x - 1)];
            if (v != kMagenta) parent[size_t(y) * 8 + size_t(x)] = v;
        }
    p.copyFrom(parent.data(), c, 1, 1, 3, 2);
    // The child starts at the parent's block of game pixel 1 - twin (1, 1) - and ends before the block of 4, twin 7;
    // the parent's blocks need six columns there, the child has five: its last one repeats.
    CHECK_EQ(at(p, 1, 1), over(0x80400000, 0xFF0000FF));
    CHECK_EQ(at(p, 2, 1), 0xFFFF0000u);
    CHECK_EQ(at(p, 6, 2), 0xFFFF0000u);
    CHECK_EQ(at(p, 7, 1), 0xFF0000FFu);  // game pixel 4: past the copy
    CHECK_EQ(at(p, 0, 1), 0xFF0000FFu);
    CHECK_EQ(p.sync(parent.data(), Rect{0, 0, 8, 4}), 0);  // the copy followed the game
}

TEST(twin_picture_copy_from_a_child_at_another_scale_enlarges) {
    std::vector<uint32_t> parent(16, 0xFF0000FF);
    TwinPicture p(parent.data(), 4, 4, 1.75);
    const std::vector<uint32_t> kid(4, 0xFFFF0000);
    const TwinPicture c(kid.data(), 2, 2, 2.0);  // made before the scale changed
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) parent[size_t(y) * 4 + size_t(x)] = 0xFFFF0000;
    p.copyFrom(parent.data(), c, 0, 0, 2, 2);
    CHECK_EQ(at(p, 0, 0), 0xFFFF0000u);
    CHECK_EQ(at(p, 2, 2), 0xFFFF0000u);  // game pixel (1, 1): [1, 3) x [1, 3)
    CHECK_EQ(at(p, 3, 3), 0xFF0000FFu);
}

TEST(twin_picture_copy_from_region_is_the_game_clipping) {
    Rect r;
    CHECK(!copyFromRegion(-1, 0, 5, 5, 8, 4, r));  // negative: the game compares unsigned and copies nothing
    CHECK(!copyFromRegion(0, -5, 5, 5, 8, 4, r));
    CHECK(!copyFromRegion(8, 0, 5, 5, 8, 4, r));
    CHECK(!copyFromRegion(0, 0, 0, 3, 8, 4, r));
    CHECK(copyFromRegion(5, 2, 10, 10, 8, 4, r));
    CHECK(r == (Rect{5, 2, 8, 4}));
}

TEST(twin_picture_texels_cut_a_rect) {
    std::vector<uint32_t> dib(300 * 2);
    for (size_t i = 0; i < dib.size(); ++i) dib[i] = 0xFF000000u | uint32_t(i % 300);  // a colour per column
    const TwinPicture t(dib.data(), 300, 2, 1.75);
    const TwinSpan second = tileSpan(256, 44, t.twinWidth(), 1.75);  // the second tile: [448, 525)
    std::vector<uint32_t> texels;
    t.texels(Rect{second.first, 0, second.end, t.twinHeight()}, texels);
    CHECK_EQ(texels.size(), size_t(77 * 3));
    CHECK_EQ(texels[0], 0xFF000000u | 256u);  // twin column 448 starts game column 256
    CHECK_EQ(texels[76], 0xFF000000u | 299u);
    CHECK_EQ(texels[77 * 2], 0xFF000000u | 256u);
}

TEST(twin_picture_reports_the_twin_pixels_it_changed) {
    std::vector<uint32_t> dib(16, 0xFF808080);
    TwinPicture t(dib.data(), 4, 4, 1.75);  // a 7 x 7 twin: edges 0 1 3 5 7
    CHECK(t.takeChanged() == (Rect{0, 0, 7, 7}));  // a new twin is all new
    CHECK(t.takeChanged().empty());
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 4, 4}), 0);
    CHECK(t.takeChanged().empty());  // nothing changed, nothing to load
    dib[1 * 4 + 2] = 0xFF000000;     // game pixel (2, 1)
    t.sync(dib.data(), Rect{0, 0, 4, 4});
    CHECK(t.takeChanged() == (Rect{3, 1, 5, 3}));
    t.overwrite(dib.data(), Rect{0, 3, 1, 4});  // game pixel (0, 3)
    t.absorb(dib.data(), Rect{0, 0, 4, 4});     // the copy only
    CHECK(t.takeChanged() == (Rect{0, 5, 1, 7}));
    t.touch(Rect{-3, -3, 2, 2});  // cut to the twin
    CHECK(t.takeChanged() == (Rect{0, 0, 2, 2}));
}

TEST(twin_picture_bytes_for_matches_a_made_one) {
    const std::vector<uint32_t> dib(30 * 20, 0xFF000000);
    const TwinPicture t(dib.data(), 30, 20, 2.625);
    CHECK_EQ(TwinPicture::bytesFor(30, 20, 2.625), t.bytes());
}

TEST(twin_picture_copy_pixels_replaces_every_pixel_it_writes) {
    // The game's CopyPixels writes every pixel of its rectangle: a skin repainted over the edge of a crisp letter
    // erases it even where the small picture does not change (review 0.5.0 I1; sync keeps it).
    const std::vector<uint32_t> dib(8, 0xFF808080);  // 4 x 2
    TwinPicture t(dib.data(), 4, 2, 2.0);            // 8 x 4
    t.twin()[2] = 0xFF000000;                        // a letter's edge in game pixel (1, 0)
    const std::vector<uint32_t> skin(2, 0xFF808080);
    CHECK_EQ(t.sync(dib.data(), Rect{1, 0, 3, 1}), 0);  // the small pixels did not change
    t.copyPixels(dib.data(), 1, 0, 2, 1, skin.data(), false);
    CHECK_EQ(at(t, 2, 0), 0xFF808080u);
}

TEST(twin_picture_copy_pixels_key_skips_exact_magenta) {
    std::vector<uint32_t> dib(4, 0xFF808080);  // 2 x 2
    TwinPicture t(dib.data(), 2, 2, 2.0);      // 4 x 4
    t.twin()[0] = 0xFF000000;                  // details in game pixels (0, 0) and (1, 0)
    t.twin()[2] = 0xFF000000;
    const std::vector<uint32_t> src = {kMagenta, 0xFF808080};
    t.copyPixels(dib.data(), 0, 0, 2, 1, src.data(), true);
    CHECK_EQ(at(t, 0, 0), 0xFF000000u);  // skipped: the game kept its pixel, the twin keeps its detail
    CHECK_EQ(at(t, 2, 0), 0xFF808080u);
    dib[0] = kMagenta;  // without the key the game writes the magenta too
    t.copyPixels(dib.data(), 0, 0, 2, 1, src.data(), false);
    CHECK_EQ(at(t, 0, 0), 0u);
}

TEST(twin_picture_copy_pixels_stops_where_the_game_stops) {
    // The game stops at the first row whose first destination pixel is 0x00000000 (black GDI text).
    std::vector<uint32_t> dib(9, 0xFF808080);  // 3 x 3
    dib[2 * 3 + 1] = 0x00000000;               // the start of row 2 at x = 1
    CHECK_EQ(copyPixelsRows(dib.data(), 3, 3, 1, 0, 3), 2);
    CHECK_EQ(copyPixelsRows(dib.data(), 3, 3, 0, 0, 3), 3);
    CHECK_EQ(copyPixelsRows(dib.data(), 3, 3, 1, -5, 3), 3);  // starts outside the picture: the game reads elsewhere
    CHECK_EQ(copyPixelsRows(dib.data(), 3, 3, 1, 2, 1), 0);
    TwinPicture t(dib.data(), 3, 3, 2.0);
    t.twin()[size_t(4 * 6 + 2)] = 0xFF000000;  // a detail in game pixel (1, 2): a row the game does not write
    const std::vector<uint32_t> src(6, 0xFF808080);
    t.copyPixels(dib.data(), 1, 0, 2, copyPixelsRows(dib.data(), 3, 3, 1, 0, 3), src.data(), false);
    CHECK_EQ(at(t, 2, 4), 0xFF000000u);
}

TEST(twin_picture_copy_pixels_wraps_past_the_right_edge) {
    std::vector<uint32_t> dib(6, 0xFF808080);  // 3 x 2
    TwinPicture t(dib.data(), 3, 2, 2.0);      // 6 x 4
    const std::vector<uint32_t> src = {0xFF00FF00, 0xFF0000FF};
    dib[2] = src[0];  // what the game wrote: (2, 0) and, past the right edge, (0, 1)
    dib[3] = src[1];
    t.copyPixels(dib.data(), 2, 0, 2, 1, src.data(), false);
    CHECK_EQ(at(t, 4, 0), 0xFF00FF00u);
    CHECK_EQ(at(t, 0, 2), 0xFF0000FFu);
    CHECK_EQ(at(t, 2, 2), 0xFF808080u);
}
