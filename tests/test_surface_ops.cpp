#include <vector>

#include "core/surface_ops.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
const ColorKey kMagenta{true, 0xF81F, 0xF81F};  // the client's key in R5G6B5 (surfaces-present-caps.md §1)
}  // namespace

TEST(surfops_pixel_address_is_the_rect_origin) {
    Image img(10, 4, PixelFormat::R5G6B5);
    CHECK(pixelAddress(img, 3, 2) == img.data() + 2 * img.pitch() + 3 * 2);
}

TEST(surfops_argb_rect_with_and_without_key) {
    Image img(3, 1, PixelFormat::R5G6B5);
    img.setPixel(0, 0, 0xF81F);
    img.setPixel(1, 0, 0x07E0);
    img.setPixel(2, 0, 0xFFFF);
    std::vector<uint32_t> out;
    toArgbRect(img, Rect{0, 0, 3, 1}, ColorKey{}, out);
    CHECK_EQ(out.size(), size_t(3));
    CHECK_EQ(out[0], 0xFFFF00FFu);
    toArgbRect(img, Rect{0, 0, 3, 1}, kMagenta, out);
    CHECK_EQ(out[0], 0u);  // alpha 0: the shader discards it while COLORKEYENABLE is on
    CHECK_EQ(out[1], 0xFF00FF00u);
    CHECK_EQ(out[2], 0xFFFFFFFFu);
}

TEST(surfops_blit_same_size_copies_and_skips_the_key) {
    Image src(2, 1, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 0xF81F);
    src.setPixel(1, 0, 0x001F);
    Image dst(2, 1, PixelFormat::R5G6B5);
    dst.setPixel(0, 0, 0x1234);
    CHECK(blitCpu(dst, dst.bounds(), src, src.bounds(), kMagenta));
    CHECK_EQ(dst.pixel(0, 0), 0x1234u);
    CHECK_EQ(dst.pixel(1, 0), 0x001Fu);
}

TEST(surfops_blit_converts_formats) {
    Image src(1, 1, PixelFormat::A4R4G4B4);
    src.setPixel(0, 0, 0xF0F0);  // opaque green
    Image dst(1, 1, PixelFormat::A1R5G5B5);
    CHECK(blitCpu(dst, dst.bounds(), src, src.bounds(), ColorKey{}));
    CHECK_EQ(dst.pixel(0, 0), 0x83E0u);
}

TEST(surfops_blit_stretches_a_mip_level) {
    Image level0(4, 4, PixelFormat::A1R5G5B5);
    fill(level0, level0.bounds(), 0x801F);
    Image level1(2, 2, PixelFormat::A1R5G5B5);
    CHECK(blitCpu(level1, level1.bounds(), level0, level0.bounds(), ColorKey{}));
    CHECK_EQ(level1.pixel(0, 0), 0x801Fu);
    CHECK_EQ(level1.pixel(1, 1), 0x801Fu);
}

TEST(surfops_blit_rejects_a_source_outside_its_surface) {
    Image a(4, 4, PixelFormat::R5G6B5);
    Image b(4, 4, PixelFormat::R5G6B5);
    CHECK(!blitCpu(a, a.bounds(), b, Rect{2, 2, 6, 6}, ColorKey{}));
}

TEST(surfops_read_target_floors_into_16_bits) {
    const std::vector<uint32_t> target = {0xFFFDFDFD, 0xFFFFFFFF};
    Image dst(2, 1, PixelFormat::R5G6B5);
    CHECK(readTargetInto(dst, dst.bounds(), target, 2, 1, Rect{0, 0, 2, 1}));
    CHECK_EQ(dst.pixel(0, 0), uint32_t(30u << 11 | 62u << 5 | 30u));
    CHECK_EQ(dst.pixel(1, 0), 0xFFFFu);
}

TEST(surfops_read_target_stretches_into_a_surface_rounded_up_to_4) {
    // ROExt's 1600x863 window: the calibration readback surface is 1600x864, Blt(NULL, target, NULL) stretches.
    const std::vector<uint32_t> target = {0xFF000000, 0xFF0000FF, 0xFFFF0000};  // 1x3: black, blue, red
    Image dst(1, 4, PixelFormat::X8R8G8B8);
    CHECK(readTargetInto(dst, dst.bounds(), target, 1, 3, Rect{0, 0, 1, 3}));
    CHECK_EQ(dst.pixel(0, 0), 0x000000u);
    CHECK_EQ(dst.pixel(0, 1), 0x0000FFu);
    CHECK_EQ(dst.pixel(0, 2), 0x0000FFu);  // nearest: rows 0, 1, 1, 2
    CHECK_EQ(dst.pixel(0, 3), 0xFF0000u);
}

TEST(surfops_target_write_is_clipped_to_the_target) {
    Image src(4, 1, PixelFormat::R5G6B5);
    fill(src, src.bounds(), 0xFFFF);
    TargetWrite w;
    CHECK(targetWrite(src, src.bounds(), Rect{2, 0, 6, 1}, 4, 2, ColorKey{}, {}, w));
    CHECK(w.rect == (Rect{2, 0, 4, 1}));
    CHECK_EQ(w.argb.size(), size_t(2));
    CHECK_EQ(w.argb[0], 0xFFFFFFFFu);
}

TEST(surfops_target_write_with_key_keeps_the_target) {
    Image src(2, 1, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 0xF81F);
    src.setPixel(1, 0, 0x001F);
    const std::vector<uint32_t> current = {0xFF112233, 0xFF445566};
    TargetWrite w;
    CHECK(targetWrite(src, src.bounds(), Rect{0, 0, 2, 1}, 2, 1, kMagenta, current, w));
    CHECK_EQ(w.argb[0], 0xFF112233u);
    CHECK_EQ(w.argb[1], 0xFF0000FFu);
    CHECK(!targetWrite(src, src.bounds(), Rect{0, 0, 2, 1}, 2, 1, kMagenta, {}, w));  // a key needs the target
}

// Review I1: ReleaseDC writes back only what GDI drew (the screenshot caption), so at k > 1 the rest of the frame is
// not shrunk and stretched again before the screenshot reads it.
TEST(surfops_changed_rect_bounds_what_gdi_drew) {
    const std::vector<uint32_t> before(8 * 6, 0xFF102030u);
    std::vector<uint32_t> after = before;
    CHECK(changedRect(before.data(), after.data(), 8, 6).empty());  // nothing drawn
    after[2 * 8 + 3] = 0x00FFFFFFu;  // GDI text: white, the X byte 0
    after[4 * 8 + 5] = 0x00000000u;
    CHECK(changedRect(before.data(), after.data(), 8, 6) == (Rect{3, 2, 6, 5}));
    after = before;
    after[0] = 0x00102030u;  // only the X byte: GDI touched it, the colour is the same
    CHECK(changedRect(before.data(), after.data(), 8, 6).empty());
}
