#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include "core/pack_download.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::pack;

namespace {

std::vector<uint8_t> fileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// A pack as the site has it: pictures by the builder's Writer, its index.
std::string sitePack(const std::vector<std::pair<std::string, std::string>>& pictures, const std::string& index) {
    char name[] = "/tmp/uagfx-site-XXXXXX";
    const std::string dir = mkdtemp(name) ? name : "/tmp";
    const std::string path = dir + "/uaro-textures.zip";
    Writer w;
    std::string error;
    w.open(path, false, error);
    for (const auto& p : pictures)
        w.add(p.first, reinterpret_cast<const uint8_t*>(p.second.data()), p.second.size(), error);
    w.close(index, error);
    return path;
}

}  // namespace

TEST(pack_download_reads_the_sites_directory_and_fetches_what_is_missing) {
    // 0.53: the remote pack's central directory read through ranges, the missing pictures fetched in spans, each
    // picture's bytes found in its span and checked.
    const std::vector<std::pair<std::string, std::string>> pics = {
        {"world/256x256/0000000000000001.webp", std::string(5000, 'a')},
        {"world/256x256/0000000000000002.webp", std::string(7000, 'b')},
        {"sprites/32x64/0000000000000003.webp", std::string(300, 'c')},
    };
    const std::string path = sitePack(pics, "world/256x256/0000000000000001.webp\tground\t\n");
    const std::vector<uint8_t> site = fileBytes(path);
    int reads = 0;
    const ReadAt read = [&](uint64_t at, uint8_t* out, size_t n) {
        ++reads;
        if (at + n > site.size()) return false;
        std::copy(site.begin() + long(at), site.begin() + long(at + n), out);
        return true;
    };
    std::vector<ZipEntry> remote;
    std::string error;
    CHECK(centralDirectoryOf(site.size(), read, remote, error));
    CHECK(remote.size() == 4);  // and the index
    CHECK(reads >= 2);
    uint64_t end = 0;
    for (const ZipEntry& e : remote) end = std::max(end, e.header + 30 + e.name.size() + e.compressed);
    const std::set<std::string> local = {"world/256x256/0000000000000001.webp"};
    const auto spans = downloadSpans(remote, [&](const std::string& n) { return local.count(n) != 0; }, end);
    CHECK(spans.size() == 1);  // the two missing pictures are near: one request
    CHECK(spans[0].entries.size() == 2);
    std::vector<uint8_t> got(site.begin() + long(spans[0].from), site.begin() + long(spans[0].to));
    for (size_t i : spans[0].entries) {
        const uint8_t* data = nullptr;
        CHECK(entryData(got.data(), got.size(), spans[0].from, remote[i], data, error));
        const std::string text(reinterpret_cast<const char*>(data), size_t(remote[i].compressed));
        CHECK(text == (remote[i].name.find("0002") != std::string::npos ? pics[1].second : pics[2].second));
    }
    got[size_t(remote[spans[0].entries[0]].header - spans[0].from) + 40] ^= 1;  // a byte gone wrong on the way
    const uint8_t* data = nullptr;
    CHECK(!entryData(got.data(), got.size(), spans[0].from, remote[spans[0].entries[0]], data, error));
    CHECK(!entryData(got.data(), 20, spans[0].from, remote[spans[0].entries[1]], data, error));  // cut short
    // Spans apart past the gap, and a span no bigger than asked.
    const auto apart = downloadSpans(remote, [](const std::string&) { return false; }, end, 0, 1);
    CHECK(apart.size() == 3);
    CHECK(downloadSpans(remote, [](const std::string&) { return true; }, end).empty());
    std::remove(path.c_str());
}

TEST(pack_download_merges_the_indexes) {
    // The site's pictures and aliases, and the player's own (made by the game) it has not.
    const std::string site = "world/a.webp\tground\t\n=64x64\\00000000000000aa\tworld/a.webp\n";
    const std::string mine = "world/a.webp\tobjects\tprontera\nworld/mine.webp\teffects\t\n";
    const Index x = parseIndexFile(mergedIndex(mine, site));
    CHECK(x.pictures.size() == 2);
    CHECK(x.pictures.at("world/a.webp").kind == ai::Kind::Ground);  // the site's line wins
    CHECK(x.pictures.at("world/mine.webp").kind == ai::Kind::Effects);
    CHECK(x.aliases.size() == 1);
}
