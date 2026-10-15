#include <string>
#include <vector>

#include "core/png_write.hpp"
#include <miniz.h>
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

// The RGB rows of a PNG miniz wrote (its IDAT inflated; every row has filter 0): width, height and the pixels.
bool decode(const std::vector<uint8_t>& png, int& w, int& h, std::vector<uint8_t>& rgb) {
    std::vector<uint8_t> idat;
    for (size_t at = 8; at + 12 <= png.size();) {
        const uint32_t len = be32(&png[at]);
        const std::string type(reinterpret_cast<const char*>(&png[at + 4]), 4);
        if (type == "IHDR") {
            w = int(be32(&png[at + 8]));
            h = int(be32(&png[at + 12]));
        } else if (type == "IDAT") {
            idat.insert(idat.end(), png.begin() + long(at + 8), png.begin() + long(at + 8 + len));
        }
        at += 12 + len;
    }
    size_t n = 0;
    void* raw = tinfl_decompress_mem_to_heap(idat.data(), idat.size(), &n, TINFL_FLAG_PARSE_ZLIB_HEADER);
    if (!raw || n != size_t(h) * (1 + size_t(w) * 3)) return false;
    const auto* r = static_cast<const uint8_t*>(raw);
    rgb.clear();
    for (int y = 0; y < h; ++y) rgb.insert(rgb.end(), r + y * (1 + w * 3) + 1, r + (y + 1) * (1 + w * 3));
    mz_free(raw);
    return true;
}

}  // namespace

TEST(png_write_keeps_the_pictures_colours) {
    // 0.54: the screenshot - B8G8R8A8 rows with padding in, an RGB PNG out (read back by stb_image).
    const int w = 3, h = 2;
    const size_t pitch = 16;
    std::vector<uint8_t> bgra(pitch * h, 0xEE);
    const uint8_t colours[6][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {10, 20, 30}, {200, 100, 50}, {1, 2, 3}};
    for (int i = 0; i < 6; ++i) {
        uint8_t* p = &bgra[size_t(i / 3) * pitch + size_t(i % 3) * 4];
        p[0] = colours[i][2];
        p[1] = colours[i][1];
        p[2] = colours[i][0];
        p[3] = 0;  // the back buffer's alpha means nothing
    }
    const std::vector<uint8_t> png = pngOf(bgra.data(), w, h, pitch);
    CHECK(png.size() > 8 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
    int rw = 0, rh = 0;
    std::vector<uint8_t> back;
    CHECK(decode(png, rw, rh, back) && rw == w && rh == h);
    for (int i = 0; i < 6 && back.size() == 18; ++i)
        for (int c = 0; c < 3; ++c) CHECK(back[size_t(i) * 3 + size_t(c)] == colours[i][c]);
    CHECK(pngOf(nullptr, 1, 1, 4).empty());
}
