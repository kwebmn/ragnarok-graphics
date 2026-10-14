#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/grf/grf.hpp"
#include "test.hpp"

using uagfx::grf::Archive;

namespace {

std::string tempPath() {
    char name[] = "/tmp/uagfx-grf-XXXXXX";
    const int fd = mkstemp(name);
    if (fd >= 0) close(fd);
    std::remove(name);  // the archive is created by open()
    return name;
}

std::vector<uint8_t> bytes(size_t n, uint8_t seed) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = uint8_t(seed + i * 7 + (i >> 5));
    return v;
}

}  // namespace

TEST(grf_archive_keeps_what_was_added_across_opens) {
    const std::string path = tempPath();
    std::string error;
    {
        Archive a;
        CHECK(a.open(path, true, error));
        CHECK_EQ(a.count(), size_t(0));
        CHECK(a.append({{"ai\\one", bytes(1000, 1)}, {"ai\\two", bytes(70000, 2)}}, error));
    }
    {
        Archive a;
        CHECK(a.open(path, false, error));
        CHECK_EQ(a.count(), size_t(2));
        std::vector<uint8_t> out;
        CHECK(a.read("ai\\two", out, error) && out == bytes(70000, 2));
        CHECK(a.append({{"ai\\three", bytes(5, 3)}, {"ai\\one", bytes(9, 4)}}, error));  // a name again: the new data
    }
    {
        Archive a;
        CHECK(a.open(path, false, error));
        CHECK_EQ(a.count(), size_t(3));
        std::vector<uint8_t> out;
        CHECK(a.read("ai\\one", out, error) && out == bytes(9, 4));
        CHECK(a.read("ai\\two", out, error) && out == bytes(70000, 2));
        CHECK(a.read("ai\\three", out, error) && out == bytes(5, 3));
        CHECK(!a.read("ai\\four", out, error));
    }
    std::remove(path.c_str());
}

TEST(grf_archive_survives_data_written_without_its_table) {
    // A crash after new files' data but before the new table and header: the archive opens as it was.
    const std::string path = tempPath();
    std::string error;
    {
        Archive a;
        CHECK(a.open(path, true, error));
        CHECK(a.append({{"ai\\kept", bytes(300, 5)}}, error));
    }
    FILE* f = std::fopen(path.c_str(), "ab");
    const std::vector<uint8_t> junk = bytes(4096, 9);
    std::fwrite(junk.data(), 1, junk.size(), f);
    std::fclose(f);
    Archive a;
    CHECK(a.open(path, false, error));
    std::vector<uint8_t> out;
    CHECK(a.read("ai\\kept", out, error) && out == bytes(300, 5));
    std::remove(path.c_str());
}

TEST(grf_archive_refuses_what_is_not_one) {
    const std::string path = tempPath();
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fputs("not a grf at all, just text that is long enough to fill a header", f);
    std::fclose(f);
    Archive a;
    std::string error;
    CHECK(!a.open(path, true, error));
    CHECK(!error.empty());
    std::remove(path.c_str());
}
