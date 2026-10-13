#include <cmath>

#include "core/d3d7.hpp"
#include "core/raster.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
RefTexture checker() {  // calibration.md §1: 4x4, 2x2 blocks, black-white over white-black
    const uint32_t B = 0x00000000, W = 0xFFFFFFFF;
    return RefTexture{4, 4, {B, B, W, W, B, B, W, W, W, W, B, B, W, W, B, B}};
}
TlVertex tl(float x, float y, float u, float v) {
    return TlVertex{x, y, 1e-5f, 0.99999f, 0xFFFFFFFF, 0xFFFFFFFF, u, v, 0.0f, 0.0f};
}
void quad(RefRaster& r, float x0, float y0, float u0, float v0, float u1, float v1, uint32_t address) {
    const TlVertex v[4] = {tl(x0, y0, u0, v0), tl(x0, y0 + 4, u0, v1), tl(x0 + 4, y0, u1, v0), tl(x0 + 4, y0 + 4, u1, v1)};
    r.strip(v, 4, checker(), true, address);
}
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
// A solid white rectangle of game pixels [x0, x1) x [y0, y1) drawn into r on a grey background.
void rect(RefRaster& r, float x0, float y0, float x1, float y1) {
    const RefTexture white{1, 1, {0xFFFFFFFF}};
    const TlVertex v[4] = {tl(x0, y0, 0, 0), tl(x0, y1, 0, 1), tl(x1, y0, 1, 0), tl(x1, y1, 1, 1)};
    r.clear(Rgb{0.25, 0.25, 0.25});
    r.strip(v, 4, white, false, d3d7::TADDRESS_CLAMP);
}
int covered(const RefRaster& r) {
    int n = 0;
    for (int y = 0; y < r.height(); ++y)
        for (int x = 0; x < r.width(); ++x)
            if (!near(r.at(x, y).r, 0.25)) ++n;
    return n;
}
}  // namespace

TEST(raster_ndc_puts_d3d7_pixel_centres_on_d3d12_centres) {
    float x = 0, y = 0;
    tlToNdc(0.0f, 0.0f, 4, 4, x, y);  // D3D7 pixel 0 -> D3D12 centre 0.5 -> -0.75
    CHECK(std::fabs(x - -0.75f) < 1e-6f);
    CHECK(std::fabs(y - 0.75f) < 1e-6f);
    tlToNdc(3.0f, 3.0f, 4, 4, x, y);
    CHECK(std::fabs(x - 0.75f) < 1e-6f);
    CHECK(std::fabs(y - -0.75f) < 1e-6f);
}

TEST(raster_quad_covers_exactly_its_16_pixels) {
    RefRaster r(8, 8, PixelCenters::D3D7);
    r.clear(Rgb{0.25, 0.25, 0.25});
    quad(r, 2, 2, 0.125f, 0.125f, 1.125f, 1.125f, d3d7::TADDRESS_CLAMP);  // offset 0.5 texel: exact texels
    int covered = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (!near(r.at(x, y).r, 0.25)) ++covered;
    CHECK_EQ(covered, 16);
    CHECK(near(r.at(1, 2).r, 0.25));  // left of the quad
    CHECK(near(r.at(6, 2).r, 0.25));  // right edge x = 6 is not covered (top-left rule)
}

TEST(raster_half_texel_offset_samples_texel_centres) {
    RefRaster r(4, 4, PixelCenters::D3D7);
    quad(r, 0, 0, 0.125f, 0.125f, 1.125f, 1.125f, d3d7::TADDRESS_CLAMP);
    CHECK(near(r.at(0, 0).g, 0.0));
    CHECK(near(r.at(2, 0).g, 1.0));
    CHECK(near(r.at(0, 2).g, 1.0));
    CHECK(near(r.at(3, 3).g, 0.0));
}

TEST(raster_zero_offset_blurs_half_and_half) {
    RefRaster r(4, 4, PixelCenters::D3D7);
    quad(r, 0, 0, 0.0f, 0.0f, 1.0f, 1.0f, d3d7::TADDRESS_CLAMP);  // calibration.md §5, idx 0x0000
    CHECK(near(r.at(0, 0).g, 0.0));
    CHECK(near(r.at(1, 0).g, 0.0));
    CHECK(near(r.at(2, 0).g, 0.5));
    CHECK(near(r.at(3, 0).g, 1.0));
    CHECK(near(r.at(0, 2).g, 0.5));
    CHECK(near(r.at(2, 2).g, 0.5));
}

TEST(raster_ours_at_scale_1_matches_d3d7) {
    RefRaster a(8, 8, PixelCenters::D3D7);
    RefRaster b(8, 8, PixelCenters::Ours, 1.0);
    quad(a, 2, 2, 0.0f, 0.0f, 1.0f, 1.0f, d3d7::TADDRESS_CLAMP);
    quad(b, 2, 2, 0.0f, 0.0f, 1.0f, 1.0f, d3d7::TADDRESS_CLAMP);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) CHECK(near(a.at(x, y).g, b.at(x, y).g));
}

TEST(raster_full_screen_quad_at_1_75_covers_every_pixel) {
    // An 8x8 game drawn 1.75 times larger: 14x14. The client's full-screen quad has its vertices on the screen's
    // edges (0 and 8); every target pixel must be covered ((x + 0.5) k would leave column 0 and row 0 empty).
    RefRaster r(14, 14, PixelCenters::Ours, 1.75);
    rect(r, 0, 0, 8, 8);
    CHECK_EQ(covered(r), 14 * 14);
}

TEST(raster_one_game_pixel_becomes_k_by_k_pixels) {
    RefRaster r(16, 16, PixelCenters::Ours, 2.0);
    rect(r, 3, 3, 4, 4);
    CHECK_EQ(covered(r), 4);
    CHECK(!near(r.at(6, 6).r, 0.25));
    CHECK(!near(r.at(7, 7).r, 0.25));
    CHECK(near(r.at(8, 8).r, 0.25));
}
