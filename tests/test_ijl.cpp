// 3.0.1-beta.6: our ijl15.dll's core (core/ijl) - Intel JPEG Library 1.5's ijlRead and ijlWrite as uaRO.exe calls
// them: its loader (0x42BF50) reads a JPEG from memory into a BGR DIB, rows padded to 4 bytes, the height above 0
// for the top row first; its Print Screen (0x411F72) writes one.
#include <cstring>
#include <vector>

#include "core/ijl.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

struct Bgr {
    uint8_t b, g, r;
};

// A DIB of w x h, its rows w * 3 bytes and the padding to 4, every byte `fill`
std::vector<uint8_t> dibBytes(int w, int h, uint8_t fill) {
    const int stride = (w * 3 + 3) & ~3;
    return std::vector<uint8_t>(size_t(stride) * size_t(h), fill);
}

ijl::Dib dib(std::vector<uint8_t>& bytes, int w, int h, int color = ijl::kBgr) {
    ijl::Dib d;
    d.bytes = bytes.data();
    d.width = w;
    d.height = h;
    d.padBytes = ((w * 3 + 3) & ~3) - w * 3;
    d.channels = 3;
    d.color = color;
    return d;
}

Bgr at(const std::vector<uint8_t>& bytes, int w, int x, int row) {
    const size_t i = size_t(row) * size_t((w * 3 + 3) & ~3) + size_t(x) * 3;
    return {bytes[i], bytes[i + 1], bytes[i + 2]};
}

bool red(Bgr p) { return p.r > 200 && p.g < 60 && p.b < 60; }
bool blue(Bgr p) { return p.b > 200 && p.g < 60 && p.r < 60; }
bool green(Bgr p) { return p.g > 200 && p.r < 60 && p.b < 60; }

// A 15 x 16 picture, written: the top half red and blue (left, right), the bottom half green
std::vector<uint8_t> picture() {
    const int w = 15, h = 16;
    std::vector<uint8_t> bytes = dibBytes(w, h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint8_t* p = &bytes[size_t(y) * size_t((w * 3 + 3) & ~3) + size_t(x) * 3];
            const Bgr c = y >= 8 ? Bgr{0, 255, 0} : x < 8 ? Bgr{0, 0, 255} : Bgr{255, 0, 0};
            p[0] = c.b, p[1] = c.g, p[2] = c.r;
        }
    std::vector<uint8_t> jpg;
    CHECK_EQ(ijl::writeImage(dib(bytes, w, h), 95, jpg), ijl::kOk);
    return jpg;
}

}  // namespace

TEST(ijl_writes_and_reads_a_bgr_dib) {
    const std::vector<uint8_t> jpg = picture();
    CHECK(jpg.size() > 100 && jpg[0] == 0xFF && jpg[1] == 0xD8);
    ijl::Params p;
    CHECK_EQ(ijl::readParams(jpg.data(), jpg.size(), p), ijl::kOk);
    CHECK(p.width == 15 && p.height == 16 && p.channels == 3);
    std::vector<uint8_t> bytes = dibBytes(15, 16, 0xEE);
    CHECK_EQ(ijl::readImage(jpg.data(), jpg.size(), dib(bytes, 15, 16)), ijl::kOk);
    CHECK(red(at(bytes, 15, 1, 1)) && blue(at(bytes, 15, 13, 1)) && green(at(bytes, 15, 7, 14)));
    const size_t stride = size_t((15 * 3 + 3) & ~3);
    CHECK(bytes[stride - 1] == 0xEE && bytes[stride * 16 - 3] == 0xEE);  // the rows' padding left as it was
}

// IJL's height: above 0 the top row first, below 0 the bottom row first (a Windows DIB)
TEST(ijl_reads_bottom_up_when_the_height_is_negative) {
    const std::vector<uint8_t> jpg = picture();
    std::vector<uint8_t> bytes = dibBytes(15, 16, 0);
    CHECK_EQ(ijl::readImage(jpg.data(), jpg.size(), dib(bytes, 15, -16)), ijl::kOk);
    CHECK(green(at(bytes, 15, 7, 1)) && red(at(bytes, 15, 1, 14)));
}

TEST(ijl_reads_rgb_when_asked) {
    const std::vector<uint8_t> jpg = picture();
    std::vector<uint8_t> bytes = dibBytes(15, 16, 0);
    CHECK_EQ(ijl::readImage(jpg.data(), jpg.size(), dib(bytes, 15, 16, ijl::kRgb)), ijl::kOk);
    const Bgr p = at(bytes, 15, 1, 1);  // red's bytes in R, G, B order
    CHECK(p.b > 200 && p.r < 60);
}

TEST(ijl_refuses_what_it_cannot_do) {
    const char garbage[] = "no JPEG here";
    ijl::Params p;
    CHECK_EQ(ijl::readParams(reinterpret_cast<const uint8_t*>(garbage), sizeof garbage, p), ijl::kNotJpeg);
    const std::vector<uint8_t> jpg = picture();
    std::vector<uint8_t> bytes = dibBytes(15, 16, 0);
    CHECK_EQ(ijl::readImage(jpg.data(), jpg.size(), dib(bytes, 14, 16)), ijl::kInvalidProperties);  // another size
    ijl::Dib none = dib(bytes, 15, 16);
    none.bytes = nullptr;
    CHECK_EQ(ijl::readImage(jpg.data(), jpg.size(), none), ijl::kInvalidProperties);
    std::vector<uint8_t> out;
    CHECK_EQ(ijl::writeImage(none, 75, out), ijl::kInvalidProperties);
    CHECK(std::strlen(ijl::errorText(ijl::kNotJpeg)) > 0);
}
