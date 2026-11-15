#include <cstdlib>
#include <string>
#include <vector>

#include "core/png_write.hpp"
#include <miniz.h>
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

// The RGB rows of a PNG (its IDAT inflated, each row unfiltered by its own filter - PNG's five): width, height, pixels
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
    const size_t row = size_t(w) * 3;
    if (!raw || n != size_t(h) * (1 + row)) return false;
    const auto* r = static_cast<const uint8_t*>(raw);
    rgb.assign(size_t(h) * row, 0);
    for (int y = 0; y < h; ++y) {
        const uint8_t f = r[size_t(y) * (1 + row)];
        const uint8_t* in = r + size_t(y) * (1 + row) + 1;
        uint8_t* out = &rgb[size_t(y) * row];
        const uint8_t* up = y ? &rgb[size_t(y - 1) * row] : nullptr;
        for (size_t i = 0; i < row; ++i) {
            const int a = i >= 3 ? out[i - 3] : 0, b = up ? up[i] : 0, c = up && i >= 3 ? up[i - 3] : 0;
            int pred = 0;
            if (f == 1) pred = a;
            if (f == 2) pred = b;
            if (f == 3) pred = (a + b) / 2;
            if (f == 4) {
                const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                pred = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            }
            if (f > 4) return false;
            out[i] = uint8_t(in[i] + pred);
        }
    }
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

// 3.0.20-beta.19 (the owner: «такое впечатление что оно не один скрин отправляет а несколько» - a report's one
// screenshot sent twice, its original a 4K PNG of 16-20 MB: miniz's writer filters no row): each row with the filter
// that leaves its bytes smallest (as libpng chooses) and libdeflate's deflate - the same pixels, a far smaller file
TEST(png_write_filters_its_rows) {
    const int w = 200, h = 120;
    std::vector<uint8_t> bgra(size_t(w) * size_t(h) * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint8_t* p = &bgra[(size_t(y) * size_t(w) + size_t(x)) * 4];
            p[2] = uint8_t(x * 7 + y * 13), p[1] = uint8_t(x * 3 + y * 5), p[0] = uint8_t(x + y * 11), p[3] = 0;
        }
    const std::vector<uint8_t> png = pngOf(bgra.data(), w, h, size_t(w) * 4);
    int rw = 0, rh = 0;
    std::vector<uint8_t> back;
    CHECK(decode(png, rw, rh, back) && rw == w && rh == h);
    bool same = back.size() == size_t(w) * size_t(h) * 3;
    for (size_t i = 0; same && i < size_t(w) * size_t(h); ++i)
        same = back[i * 3] == bgra[i * 4 + 2] && back[i * 3 + 1] == bgra[i * 4 + 1] && back[i * 3 + 2] == bgra[i * 4];
    CHECK(same);
    // the writer before: no row filtered
    std::vector<uint8_t> rgb(size_t(w) * size_t(h) * 3);
    for (size_t i = 0; i < size_t(w) * size_t(h); ++i)
        rgb[i * 3] = bgra[i * 4 + 2], rgb[i * 3 + 1] = bgra[i * 4 + 1], rgb[i * 3 + 2] = bgra[i * 4];
    size_t before = 0;
    void* old = tdefl_write_image_to_png_file_in_memory_ex(rgb.data(), w, h, 3, &before, 6, MZ_FALSE);
    mz_free(old);
    CHECK(before > 0 && png.size() * 3 < before);
}
