#include "core/png_write.hpp"

#include <libdeflate.h>
#include <miniz.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace uagfx {

namespace {

void be32(std::vector<uint8_t>& out, uint32_t v) {
    for (int k = 3; k >= 0; --k) out.push_back(uint8_t(v >> (8 * k)));
}

void chunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, size_t size) {
    be32(out, uint32_t(size));
    const size_t from = out.size();
    out.insert(out.end(), type, type + 4);
    if (size) out.insert(out.end(), data, data + size);
    be32(out, uint32_t(mz_crc32(MZ_CRC32_INIT, &out[from], out.size() - from)));
}

}  // namespace

// 3.0.20-beta.19 (the owner: «такое впечатление что оно не один скрин отправляет а несколько» - a report's original, a
// 4K PNG, was 16-20 MB: miniz's writer filtered no row): each row with the filter that leaves its bytes smallest (the
// sum of their distances from 0, as libpng chooses), all of it deflated by libdeflate at its level 1 - R-U7QZPV's 4K
// original 17.3 MB in 1066 ms before, 12.5 MB in about 400 ms now (level 6: 12.3 MB, 600 ms; 12: 11.7 MB, 4.3 s)
std::vector<uint8_t> pngOf(const uint8_t* bgra, int width, int height, size_t pitch) {
    std::vector<uint8_t> out;
    if (!bgra || width <= 0 || height <= 0) return out;
    const size_t row = size_t(width) * 3;
    std::vector<uint8_t> prev(row, 0), cur(row), raw(size_t(height) * (row + 1)), tried[5];
    for (std::vector<uint8_t>& t : tried) t.resize(row);
    for (int y = 0; y < height; ++y) {
        const uint8_t* p = bgra + size_t(y) * pitch;
        for (int x = 0; x < width; ++x) {
            cur[size_t(x) * 3] = p[size_t(x) * 4 + 2];
            cur[size_t(x) * 3 + 1] = p[size_t(x) * 4 + 1];
            cur[size_t(x) * 3 + 2] = p[size_t(x) * 4];
        }
        uint64_t best = UINT64_MAX;
        int chosen = 0;
        for (int f = 0; f < 5; ++f) {
            uint64_t sum = 0;
            for (size_t i = 0; i < row; ++i) {
                const int a = i >= 3 ? cur[i - 3] : 0, b = prev[i], c = i >= 3 ? prev[i - 3] : 0;
                int pred = 0;
                if (f == 1) pred = a;
                if (f == 2) pred = b;
                if (f == 3) pred = (a + b) / 2;
                if (f == 4) {
                    const int q = a + b - c, pa = std::abs(q - a), pb = std::abs(q - b), pc = std::abs(q - c);
                    pred = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
                }
                const uint8_t v = uint8_t(cur[i] - pred);
                tried[f][i] = v;
                sum += v < 128 ? v : 256u - v;
            }
            if (sum < best) best = sum, chosen = f;
        }
        uint8_t* to = &raw[size_t(y) * (row + 1)];
        to[0] = uint8_t(chosen);
        std::memcpy(to + 1, tried[chosen].data(), row);
        std::swap(prev, cur);
    }
    libdeflate_compressor* c = libdeflate_alloc_compressor(1);
    if (!c) return out;
    std::vector<uint8_t> idat(libdeflate_zlib_compress_bound(c, raw.size()));
    const size_t packed = libdeflate_zlib_compress(c, raw.data(), raw.size(), idat.data(), idat.size());
    libdeflate_free_compressor(c);
    if (!packed) return out;
    static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    out.assign(kSignature, kSignature + 8);
    std::vector<uint8_t> head;
    be32(head, uint32_t(width));
    be32(head, uint32_t(height));
    head.insert(head.end(), {8, 2, 0, 0, 0});  // 8 bits, RGB, deflate, the filters' set, not interlaced
    chunk(out, "IHDR", head.data(), head.size());
    chunk(out, "IDAT", idat.data(), packed);
    chunk(out, "IEND", nullptr, 0);
    return out;
}

bool writePng(const std::string& path, const uint8_t* bgra, int width, int height, size_t pitch) {
    const std::vector<uint8_t> png = pngOf(bgra, width, height, pitch);
    if (png.empty()) return false;
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    return std::fclose(f) == 0 && ok;
}

}  // namespace uagfx
