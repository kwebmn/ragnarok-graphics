#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/image.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
Image filled(int w, int h, PixelFormat f, uint32_t raw) {
    Image img(w, h, f);
    fill(img, img.bounds(), raw);
    return img;
}

// stretch as it was until 2.5.13 - each pixel's place worked out on its own, each channel through std::lround - the
// reference the faster one keeps to, bit for bit
uint32_t lerpArgbAsBefore(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const float ca = float((a >> shift) & 255);
        const float cb = float((b >> shift) & 255);
        out |= uint32_t(std::lround(ca + (cb - ca) * t)) << shift;
    }
    return out;
}

bool stretchAsBefore(Image& dst, Rect dr, const Image& src, Rect sr, StretchFilter filter) {
    if (dst.format() != src.format() || dst.format() == PixelFormat::Unknown) return false;
    if (dr.empty() || sr.empty() || sr.left < 0 || sr.top < 0 || sr.right > src.width() || sr.bottom > src.height())
        return false;
    const Rect full = dr;
    if (!clipRect(dst.bounds(), dr)) return true;
    const PixelFormat f = src.format();
    const float sx = float(sr.width()) / float(full.width());
    const float sy = float(sr.height()) / float(full.height());
    for (int y = dr.top; y < dr.bottom; ++y) {
        for (int x = dr.left; x < dr.right; ++x) {
            const float u = (float(x - full.left) + 0.5f) * sx;
            const float v = (float(y - full.top) + 0.5f) * sy;
            if (filter == StretchFilter::Point) {
                const int px = std::clamp(sr.left + int(u), sr.left, sr.right - 1);
                const int py = std::clamp(sr.top + int(v), sr.top, sr.bottom - 1);
                dst.setPixel(x, y, src.pixel(px, py));
                continue;
            }
            const float fu = u - 0.5f;
            const float fv = v - 0.5f;
            const int x0 = int(std::floor(fu));
            const int y0 = int(std::floor(fv));
            const float tx = fu - float(x0);
            const float ty = fv - float(y0);
            auto at = [&](int ix, int iy) {
                ix = std::clamp(sr.left + ix, sr.left, sr.right - 1);
                iy = std::clamp(sr.top + iy, sr.top, sr.bottom - 1);
                return toArgb8(f, src.pixel(ix, iy));
            };
            const uint32_t top = lerpArgbAsBefore(at(x0, y0), at(x0 + 1, y0), tx);
            const uint32_t bottom = lerpArgbAsBefore(at(x0, y0 + 1), at(x0 + 1, y0 + 1), tx);
            dst.setPixel(x, y, fromArgb8(f, lerpArgbAsBefore(top, bottom, ty)));
        }
    }
    return true;
}

uint32_t nextRandom(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
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

TEST(image_copies_reject_a_source_rect_outside_the_source) {
    const Image src = filled(2, 2, PixelFormat::A8R8G8B8, 7);
    Image dst(4, 4, PixelFormat::A8R8G8B8);
    CHECK(!copy(dst, 3, 0, src, Rect{-2, 0, 2, 2}));  // DirectDraw answers DDERR_INVALIDRECT
    CHECK(!copy(dst, 0, 0, src, Rect{0, 0, 3, 2}));
    CHECK(!copyKeyed(dst, 0, 0, src, Rect{0, 0, 2, 3}, 0, 0));
    CHECK(!copy(dst, 0, 0, src, Rect{1, 1, 1, 2}));  // empty
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) CHECK_EQ(dst.pixel(x, y), 0u);
}

TEST(image_stretch_rejects_a_source_rect_outside_the_source) {
    const Image src = filled(2, 2, PixelFormat::A8R8G8B8, 7);
    Image dst(4, 4, PixelFormat::A8R8G8B8);
    CHECK(!stretch(dst, dst.bounds(), src, Rect{0, -1, 2, 2}, StretchFilter::Linear));
    CHECK(!stretch(dst, dst.bounds(), src, Rect{0, 0, 3, 2}, StretchFilter::Point));
    CHECK_EQ(dst.pixel(0, 0), 0u);
}

TEST(image_stretch_stays_as_it_was_bit_for_bit) {
    // 2.5.14: the game builds its textures' mip levels by stretching Blts on its own thread (R-7NUW85: lroundf and
    // stretch were 40 % of it) - the faster stretch gives every pixel and every answer as before
    const PixelFormat formats[] = {PixelFormat::A1R5G5B5, PixelFormat::A4R4G4B4, PixelFormat::R5G6B5,
                                   PixelFormat::X1R5G5B5, PixelFormat::X8R8G8B8, PixelFormat::A8R8G8B8};
    struct Case {
        int srcW, srcH;
        Rect sr;
        int dstW, dstH;
        Rect dr;
    };
    const Case cases[] = {
        {64, 64, {0, 0, 64, 64}, 32, 32, {0, 0, 32, 32}},          // a mip level: half
        {64, 64, {0, 0, 64, 64}, 16, 16, {0, 0, 16, 16}},          // a quarter (each level from the first)
        {256, 128, {0, 0, 256, 128}, 8, 4, {0, 0, 8, 4}},          // an eighth and a 32nd
        {64, 64, {0, 0, 64, 64}, 1, 1, {0, 0, 1, 1}},              // the last level
        {48, 30, {0, 0, 48, 30}, 20, 13, {0, 0, 20, 13}},          // not a power of two
        {7, 5, {0, 0, 7, 5}, 23, 17, {0, 0, 23, 17}},              // larger
        {512, 347, {0, 0, 512, 347}, 80, 60, {0, 0, 80, 60}},      // a video frame made smaller
        {40, 30, {3, 2, 37, 29}, 24, 18, {-5, -3, 25, 20}},        // a part, clipped at the destination's edges
        {40, 30, {5, 5, 25, 15}, 30, 30, {10, 12, 40, 31}},        // past the right and bottom edges
        {9, 9, {0, 0, 9, 9}, 9, 9, {0, 0, 9, 9}},                  // one to one
        {1, 1, {0, 0, 1, 1}, 9, 9, {0, 0, 9, 9}},                  // one pixel over many
        {2, 1, {0, 0, 2, 1}, 1, 1, {0, 0, 1, 1}},
        {8, 8, {0, 0, 9, 8}, 4, 4, {0, 0, 4, 4}},                  // a source rect outside: false
        {8, 8, {0, 0, 8, 8}, 4, 4, {0, 0, 0, 4}},                  // an empty destination: false
        {8, 8, {0, 0, 8, 8}, 4, 4, {5, 5, 9, 9}},                  // wholly outside the destination: true, nothing
    };
    uint32_t seed = 12345;
    int checked = 0, differ = 0;
    for (const PixelFormat f : formats)
        for (const StretchFilter filter : {StretchFilter::Point, StretchFilter::Linear})
            for (const Case& c : cases) {
                Image src(c.srcW, c.srcH, f);
                for (int y = 0; y < c.srcH; ++y)
                    for (int x = 0; x < c.srcW; ++x) src.setPixel(x, y, nextRandom(seed));
                Image a(c.dstW, c.dstH, f), b(c.dstW, c.dstH, f);
                for (int y = 0; y < c.dstH; ++y)
                    for (int x = 0; x < c.dstW; ++x) {
                        const uint32_t raw = nextRandom(seed);
                        a.setPixel(x, y, raw);
                        b.setPixel(x, y, raw);
                    }
                const bool was = stretchAsBefore(a, c.dr, src, c.sr, filter);
                const bool now = stretch(b, c.dr, src, c.sr, filter);
                CHECK(was == now);
                for (int y = 0; y < c.dstH; ++y)
                    for (int x = 0; x < c.dstW; ++x) {
                        ++checked;
                        if (a.pixel(x, y) != b.pixel(x, y)) ++differ;
                    }
            }
    if (differ) std::printf("  %d of %d pixels differ\n", differ, checked);
    CHECK_EQ(differ, 0);
    CHECK(checked > 50000);
}

TEST(image_stretch_mixed_pixels_are_rounded_halves_up) {
    // the channels' halves go up, as std::lround's: (100 + 101) / 2 = 100.5 -> 101, 0.5 -> 1
    Image src(2, 1, PixelFormat::A8R8G8B8);
    src.setPixel(0, 0, 0x00016400);  // green 100, blue 0, red 1
    src.setPixel(1, 0, 0x01006501);  // green 101, blue 1, alpha 1
    Image dst(1, 1, PixelFormat::A8R8G8B8);
    CHECK(stretch(dst, dst.bounds(), src, src.bounds(), StretchFilter::Linear));
    CHECK_EQ(dst.pixel(0, 0), 0x01016501u);
}
