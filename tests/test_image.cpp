#include "core/image.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
Image filled(int w, int h, PixelFormat f, uint32_t raw) {
    Image img(w, h, f);
    fill(img, img.bounds(), raw);
    return img;
}
}  // namespace

TEST(image_rows_are_dword_aligned) {
    const Image img(3, 2, PixelFormat::R5G6B5);
    CHECK_EQ(img.pitch(), 8);
    CHECK_EQ(img.sizeBytes(), size_t(16));
    const Image wide(5, 1, PixelFormat::A8R8G8B8);
    CHECK_EQ(wide.pitch(), 20);
}

TEST(image_clip_rect) {
    Rect r{-2, 1, 10, 3};
    CHECK(clipRect(Rect{0, 0, 4, 4}, r));
    CHECK(r == (Rect{0, 1, 4, 3}));
    Rect outside{5, 5, 9, 9};
    CHECK(!clipRect(Rect{0, 0, 4, 4}, outside));
}

TEST(image_fill_is_clipped) {
    Image img(4, 4, PixelFormat::A8R8G8B8);
    fill(img, Rect{2, 2, 10, 10}, 0x11223344);
    CHECK_EQ(img.pixel(2, 2), 0x11223344u);
    CHECK_EQ(img.pixel(3, 3), 0x11223344u);
    CHECK_EQ(img.pixel(1, 2), 0u);
    CHECK_EQ(img.pixel(2, 1), 0u);
}

TEST(image_fill_keeps_the_low_bytes_of_the_value) {
    Image img(1, 1, PixelFormat::A1R5G5B5);
    fill(img, img.bounds(), 0x12345678);  // the client may pass a screen-format colour to a 16-bit texture
    CHECK_EQ(img.pixel(0, 0), 0x5678u);
}

TEST(image_copy_clips_at_the_destination_edge) {
    Image src(2, 2, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 1);
    src.setPixel(1, 0, 2);
    src.setPixel(0, 1, 3);
    src.setPixel(1, 1, 4);
    Image dst(4, 4, PixelFormat::R5G6B5);
    CHECK(copy(dst, -1, 0, src, src.bounds()));
    CHECK_EQ(dst.pixel(0, 0), 2u);
    CHECK_EQ(dst.pixel(0, 1), 4u);
    CHECK_EQ(dst.pixel(1, 0), 0u);
}

TEST(image_copy_needs_the_same_format) {
    const Image src = filled(2, 2, PixelFormat::R5G6B5, 7);
    Image dst(2, 2, PixelFormat::A1R5G5B5);
    CHECK(!copy(dst, 0, 0, src, src.bounds()));
    CHECK_EQ(dst.pixel(0, 0), 0u);
}

TEST(image_copy_within_the_same_image_overlapping) {
    Image img(4, 1, PixelFormat::A8R8G8B8);
    for (int x = 0; x < 4; ++x) img.setPixel(x, 0, uint32_t(x + 1));
    CHECK(copy(img, 1, 0, img, Rect{0, 0, 3, 1}));
    CHECK_EQ(img.pixel(0, 0), 1u);
    CHECK_EQ(img.pixel(1, 0), 1u);
    CHECK_EQ(img.pixel(2, 0), 2u);
    CHECK_EQ(img.pixel(3, 0), 3u);
}

TEST(image_keyed_copy_skips_the_key_colour) {
    Image src(2, 1, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 0xF81F);  // magenta: the client's transparent colour
    src.setPixel(1, 0, 0x001F);
    Image dst = filled(2, 1, PixelFormat::R5G6B5, 0x07E0);
    CHECK(copyKeyed(dst, 0, 0, src, src.bounds(), 0xF81F, 0xF81F));
    CHECK_EQ(dst.pixel(0, 0), 0x07E0u);
    CHECK_EQ(dst.pixel(1, 0), 0x001Fu);
}

TEST(image_stretch_point_doubles_pixels) {
    Image src(2, 1, PixelFormat::A8R8G8B8);
    src.setPixel(0, 0, 0xFF0000FF);
    src.setPixel(1, 0, 0xFFFF0000);
    Image dst(4, 1, PixelFormat::A8R8G8B8);
    CHECK(stretch(dst, dst.bounds(), src, src.bounds(), StretchFilter::Point));
    CHECK_EQ(dst.pixel(0, 0), 0xFF0000FFu);
    CHECK_EQ(dst.pixel(1, 0), 0xFF0000FFu);
    CHECK_EQ(dst.pixel(2, 0), 0xFFFF0000u);
    CHECK_EQ(dst.pixel(3, 0), 0xFFFF0000u);
}

TEST(image_stretch_linear_halving_averages_four_pixels) {
    Image src(2, 2, PixelFormat::A8R8G8B8);
    src.setPixel(0, 0, 0xFF000000);
    src.setPixel(1, 0, 0xFF000064);  // blue 100
    src.setPixel(0, 1, 0xFF0000C8);  // blue 200
    src.setPixel(1, 1, 0xFF000064);
    Image dst(1, 1, PixelFormat::A8R8G8B8);
    CHECK(stretch(dst, dst.bounds(), src, src.bounds(), StretchFilter::Linear));
    CHECK_EQ(dst.pixel(0, 0), 0xFF000064u);  // (0 + 100 + 200 + 100) / 4
}

TEST(image_stretch_linear_one_to_one_is_a_copy) {
    Image src(3, 1, PixelFormat::A4R4G4B4);
    src.setPixel(0, 0, 0xF00F);
    src.setPixel(1, 0, 0x8421);
    src.setPixel(2, 0, 0x0FF0);
    Image dst(3, 1, PixelFormat::A4R4G4B4);
    CHECK(stretch(dst, dst.bounds(), src, src.bounds(), StretchFilter::Linear));
    for (int x = 0; x < 3; ++x) CHECK_EQ(dst.pixel(x, 0), src.pixel(x, 0));
}
