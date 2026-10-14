#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/zip.hpp"
#include "miniz.h"
#include "test.hpp"

using namespace uagfx;

namespace {

std::string zipPath() {
    char name[] = "/tmp/uagfx-zip-XXXXXX";
    const int fd = mkstemp(name);
    if (fd >= 0) close(fd);
    return name;
}

void put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, uint16_t(v));
    put16(b, uint16_t(v >> 16));
}

struct Item {
    std::string name;
    std::vector<uint8_t> data;
    bool deflate;
};

// A zip of the items as the archivers write it: local headers and data, the central directory, its end record.
std::vector<uint8_t> makeZip(const std::vector<Item>& items) {
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

}  // namespace

TEST(zip_reads_the_entries_it_keeps) {
    // 0.25: the client's data.zip holds its maps (data/<map>.rsw): read by name, stored or deflated.
    std::vector<uint8_t> big(5000);
    for (size_t i = 0; i < big.size(); ++i) big[i] = uint8_t(i % 7);
    const std::vector<Item> items = {{"data/Prontera.rsw", {1, 2, 3, 4}, false},
                                     {"data/comodo.rsw", big, true},
                                     {"texture/x.bmp", {9}, false}};
    const std::vector<uint8_t> bytes = makeZip(items);
    const std::string path = zipPath();
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    zip::Index z;
    std::string error;
    const auto maps = [](const std::string& n) { return n.size() > 4 && n.compare(n.size() - 4, 4, ".rsw") == 0; };
    CHECK(z.open(path, maps, error));
    CHECK_EQ(z.count(), size_t(2));
    CHECK(z.has("data\\prontera.rsw"));  // names in lower case with backslashes
    CHECK(!z.has("texture\\x.bmp"));     // not kept
    std::vector<uint8_t> out;
    CHECK(z.read("data\\prontera.rsw", out, error));
    CHECK(out == items[0].data);
    CHECK(z.read("data\\comodo.rsw", out, error));
    CHECK(out == big);
    CHECK(!z.read("data\\nowhere.rsw", out, error));
    std::remove(path.c_str());
    CHECK(zip::normalName("Data/Sub/Map.RSW") == "data\\sub\\map.rsw");
}

TEST(zip_refuses_what_is_not_a_zip) {
    const std::string path = zipPath();
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fputs("not a zip at all", f);
    std::fclose(f);
    zip::Index z;
    std::string error;
    CHECK(!z.open(path, [](const std::string&) { return true; }, error));
    CHECK(!error.empty());
    std::remove(path.c_str());
}
