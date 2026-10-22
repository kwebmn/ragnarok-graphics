#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "miniz.h"

// Zip archives for the tests (test_zip, the installer's): what the archivers write, stored or deflated.
namespace fixture {

inline void put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}
inline void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, uint16_t(v));
    put16(b, uint16_t(v >> 16));
}

struct Item {
    std::string name;
    std::vector<uint8_t> data;
    bool deflate;
};

// A zip of the items as the archivers write it: local headers and data, the central directory, its end record.
inline std::vector<uint8_t> makeZip(const std::vector<Item>& items) {
    std::vector<uint8_t> b, dir;
    for (const Item& it : items) {
        std::vector<uint8_t> stored = it.data;
        if (it.deflate) {
            size_t n = 0;
            void* p = tdefl_compress_mem_to_heap(it.data.data(), it.data.size(), &n, 128);  // raw deflate
            stored.assign(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + n);
            mz_free(p);
        }
        const uint32_t at = uint32_t(b.size());
        const uint32_t crc = uint32_t(mz_crc32(MZ_CRC32_INIT, it.data.data(), it.data.size()));
        put32(b, 0x04034b50);
        put16(b, 20);
        put16(b, 0);
        put16(b, it.deflate ? 8 : 0);
        put32(b, 0);
        put32(b, crc);
        put32(b, uint32_t(stored.size()));
        put32(b, uint32_t(it.data.size()));
        put16(b, uint16_t(it.name.size()));
        put16(b, 3);  // an extra field the reader skips
        b.insert(b.end(), it.name.begin(), it.name.end());
        b.push_back(1);
        b.push_back(2);
        b.push_back(3);
        b.insert(b.end(), stored.begin(), stored.end());
        put32(dir, 0x02014b50);
        put16(dir, 20);
        put16(dir, 20);
        put16(dir, 0);
        put16(dir, it.deflate ? 8 : 0);
        put32(dir, 0);
        put32(dir, crc);
        put32(dir, uint32_t(stored.size()));
        put32(dir, uint32_t(it.data.size()));
        put16(dir, uint16_t(it.name.size()));
        put16(dir, 0);
        put16(dir, 0);
        put16(dir, 0);
        put16(dir, 0);
        put32(dir, 0);
        put32(dir, at);
        dir.insert(dir.end(), it.name.begin(), it.name.end());
    }
    const uint32_t dirAt = uint32_t(b.size());
    b.insert(b.end(), dir.begin(), dir.end());
    put32(b, 0x06054b50);
    put16(b, 0);
    put16(b, 0);
    put16(b, uint16_t(items.size()));
    put16(b, uint16_t(items.size()));
    put32(b, uint32_t(dir.size()));
    put32(b, dirAt);
    put16(b, 0);
    return b;
}

// A zip of the items as a string of bytes (what readFile gives the settings app).
inline std::string zipBytes(const std::vector<Item>& items) {
    const std::vector<uint8_t> b = makeZip(items);
    return std::string(b.begin(), b.end());
}

inline Item item(const std::string& name, const std::string& data, bool deflate = false) {
    return Item{name, std::vector<uint8_t>(data.begin(), data.end()), deflate};
}

}  // namespace fixture
