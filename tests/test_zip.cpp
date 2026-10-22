#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/zip.hpp"
#include "test.hpp"
#include "zip_fixture.hpp"

using namespace uagfx;

namespace {

std::string zipPath() {
    char name[] = "/tmp/uagfx-zip-XXXXXX";
    const int fd = mkstemp(name);
    if (fd >= 0) close(fd);
    return name;
}

using fixture::Item;
using fixture::makeZip;

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

// 2.10: an archive in memory - the setup's appended package, the installer's downloads read with readFile.
TEST(zip_reads_an_archive_in_memory) {
    zip::Index z;
    std::string error;
    CHECK(z.openBytes(fixture::zipBytes({fixture::item("uaro-graphics/ddraw.dll", "stored bytes"),
                                         fixture::item("uaro-graphics/version.txt", std::string(3000, 'v'), true)}),
                      [](const std::string&) { return true; }, error));
    CHECK_EQ(z.count(), size_t(2));
    std::vector<uint8_t> out;
    CHECK(z.read("uaro-graphics\\ddraw.dll", out, error));
    CHECK(std::string(out.begin(), out.end()) == "stored bytes");
    CHECK(z.read("uaro-graphics\\version.txt", out, error));
    CHECK(std::string(out.begin(), out.end()) == std::string(3000, 'v'));
    const std::string whole = fixture::zipBytes({fixture::item("a.txt", "a")});
    zip::Index cut;
    CHECK(!cut.openBytes(whole.substr(0, whole.size() - 10), [](const std::string&) { return true; }, error));
}
