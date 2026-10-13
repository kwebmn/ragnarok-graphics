#include <cmath>
#include <cstdint>

#include "core/d3d7.hpp"
#include "core/pixel.hpp"
#include "core/raster.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// How a read-back pixel reaches the client's bright/dark sums.
enum class Readback {
    Round565,  // reference hardware model: float -> 565, round to nearest
    Floor565,  // float -> 565, floor
    Ours565,   // our driver: GPU float -> 8 bits (round), then targetToClient() floors to 565
    Xrgb8888,  // a 32-bit screen: the client masks every pixel with 0xFFFF
};

const bool kWhite[16] = {false, false, true, true, false, false, true, true,
                         true,  true,  false, false, true, true,  false, false};

int channelSum(const Rgb& p, Readback mode) {
    auto r8 = [](double v) { return uint32_t(std::lround(v * 255.0)); };
    switch (mode) {
        case Readback::Round565: {
            const int r = int(std::lround(p.r * 31)), g = int(std::lround(p.g * 63)), b = int(std::lround(p.b * 31));
            return (r << 3) + (g << 2) + (b << 3);
        }
        case Readback::Floor565: {
            const int r = int(std::floor(p.r * 31 + 1e-9)), g = int(std::floor(p.g * 63 + 1e-9)),
                      b = int(std::floor(p.b * 31 + 1e-9));
            return (r << 3) + (g << 2) + (b << 3);
        }
        case Readback::Ours565: {
            const uint32_t v = targetToClient(PixelFormat::R5G6B5, 0xFF000000u | r8(p.r) << 16 | r8(p.g) << 8 | r8(p.b));
            return int(((v >> 11) & 31) << 3) + int(((v >> 5) & 63) << 2) + int((v & 31) << 3);
        }
        case Readback::Xrgb8888: {
            const uint32_t v = (r8(p.r) << 16 | r8(p.g) << 8 | r8(p.b)) & 0xFFFF;  // red is lost
            return int((v >> 8) & 255) + int(v & 255);
        }
    }
    return 0;
}

struct Calibration {
    uint32_t best = 0x8080;  // the client's starting index: (0.5, 0.5)
    int accepted = 0;
};

Calibration calibrate(int w, int h, PixelCenters centers, Readback mode, uint32_t address) {
    const uint32_t B = 0x00000000, W = 0xFFFFFFFF;
    const RefTexture tex{4, 4, {B, B, W, W, B, B, W, W, W, W, B, B, W, W, B, B}};
    const uint32_t cols = uint32_t(w) >> 2, rows = uint32_t(h) >> 2, perPass = cols * rows;
    Calibration out;
    int best = -1024;
    for (uint32_t base = 0; base < 0x10000; base += perPass) {
        RefRaster r(w, h, centers);
        for (uint32_t i = 0; i < perPass && base + i <= 0xFFFF; ++i) {
            const uint32_t idx = base + i, lo = idx & 0xFF, hi = idx >> 8;
            const float x0 = float(4 * (i % cols)), y0 = float(4 * (i / cols));
            const float du = float(lo) * (1 / 256.f), dv = float(hi) * (1 / 256.f);
            const float u0 = du / 4, u1 = (du + 4.0f) / 4, v0 = dv / 4, v1 = (dv + 4.0f) / 4;
            const TlVertex v[4] = {{x0, y0, 1e-5f, 0.99999f, ~0u, ~0u, u0, v0, 0, 0},
                                   {x0, y0 + 4, 1e-5f, 0.99999f, ~0u, ~0u, u0, v1, 0, 0},
                                   {x0 + 4, y0, 1e-5f, 0.99999f, ~0u, ~0u, u1, v0, 0, 0},
                                   {x0 + 4, y0 + 4, 1e-5f, 0.99999f, ~0u, ~0u, u1, v1, 0, 0}};
            r.strip(v, 4, tex, true, address);
        }
        for (uint32_t i = 0; i < perPass && base + i <= 0xFFFF; ++i) {
            int bright = 0, dark = 0;
            for (int k = 0; k < 16; ++k) {
                const int x = int(4 * (i % cols)) + (k & 3), y = int(4 * (i / cols)) + (k >> 2);
                (kWhite[k] ? bright : dark) += channelSum(r.at(x, y), mode);
            }
            if (bright >= 0x1680 && dark <= 0x180) {
                ++out.accepted;
                if (bright - dark > best) {
                    best = bright - dark;
                    out.best = base + i;
                }
            }
        }
    }
    return out;
}

}  // namespace

TEST(calibration_32bit_screen_never_accepts_and_keeps_half) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Xrgb8888, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.accepted, 0);
    CHECK_EQ(c.best, 0x8080u);
}

TEST(calibration_our_16bit_readback_settles_on_half) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Ours565, d3d7::TADDRESS_CLAMP);
    CHECK(c.accepted > 0);
    CHECK_EQ(c.best, 0x8080u);
}

TEST(calibration_floor_model_settles_on_half) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Floor565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x8080u);
}

TEST(calibration_round_model_matches_the_research) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Round565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x7E80u);  // (0.5, 0.4921875)
}

TEST(calibration_uncorrected_d3d10_centres_pick_zero) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D10, Readback::Round565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x0000u);  // what a translation without the +0.5 shift would make the client choose
}

TEST(calibration_address_mode_changes_the_count) {
    const Calibration clamp = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Round565, d3d7::TADDRESS_CLAMP);
    const Calibration wrap = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Round565, d3d7::TADDRESS_WRAP);
    CHECK(clamp.accepted != wrap.accepted);
}

TEST(calibration_runs_in_passes_on_a_small_screen) {
    const Calibration c = calibrate(800, 600, PixelCenters::D3D7, Readback::Ours565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x8080u);
}
