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

TEST(grf_copy_keeps_the_chosen_entries_and_drops_old_tables) {
    // The settings app deletes pictures of upscaled.grf: a new archive with the rest, their data as stored; the file
    // tables every append left behind go too. Empty entries (0.12's tags) survive both.
    const std::string path = tempPath(), copy = tempPath();
    std::string error;
    {
        Archive a;
        CHECK(a.open(path, true, error));
        for (int i = 0; i < 6; ++i)
            CHECK(a.append({{"ai\\p" + std::to_string(i), bytes(20000, uint8_t(i))},
                            {"ai\\@t" + std::to_string(i), {}}},
                           error));
    }
    uint64_t before = 0;
    {
        Archive a;
        CHECK(a.open(path, false, error));
        before = a.bytes();
        CHECK(a.find("ai\\@t3") && a.find("ai\\@t3")->size == 0);
        std::FILE* out = std::fopen(copy.c_str(), "wb");
        CHECK(a.copyTo(out, [](const std::string& n) { return n != "ai\\p1" && n != "ai\\p4"; }, error));
        std::fclose(out);
    }
    Archive b;
    CHECK(b.open(std::fopen(copy.c_str(), "rb"), false, error));
    CHECK_EQ(b.count(), size_t(10));
    CHECK(!b.find("ai\\p1") && !b.find("ai\\p4"));
    std::vector<uint8_t> out;
    for (int i : {0, 2, 3, 5})
        CHECK(b.read("ai\\p" + std::to_string(i), out, error) && out == bytes(20000, uint8_t(i)));
    CHECK(b.find("ai\\@t1") && b.find("ai\\@t1")->size == 0);
    CHECK(b.bytes() < before);
    b.close();
    // Nothing kept: still a valid, empty archive.
    {
        Archive a;
        CHECK(a.open(path, false, error));
        std::FILE* o = std::fopen(copy.c_str(), "wb");
        CHECK(a.copyTo(o, [](const std::string&) { return false; }, error));
        std::fclose(o);
    }
    Archive e;
    CHECK(e.open(copy, false, error));
    CHECK_EQ(e.count(), size_t(0));
    CHECK(!b.open(static_cast<std::FILE*>(nullptr), false, error));
    std::remove(path.c_str());
    std::remove(copy.c_str());
}
