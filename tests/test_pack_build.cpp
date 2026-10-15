#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/grf/grf.hpp"
#include "core/pack_build.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::string tempDir() {
    char name[] = "/tmp/uagfx-game-XXXXXX";
    return mkdtemp(name) ? name : "";
}

void writeFile(const std::string& path, const std::string& text) {
    if (std::FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
    }
}

void grfWith(const std::string& path, const std::vector<std::pair<std::string, std::string>>& files) {
    grf::Archive a;
    std::string error;
    a.open(path, true, error);
    std::vector<std::pair<std::string, std::vector<uint8_t>>> data;
    for (const auto& f : files) data.emplace_back(f.first, std::vector<uint8_t>(f.second.begin(), f.second.end()));
    a.append(data, error);
}

}  // namespace

TEST(pack_build_reads_data_ini_in_the_games_order) {
    // 0.43: [Data] by the numbers, whatever the lines' order; the other sections and comments are not archives.
    const auto a = pack::dataArchives("[Other]\n0=no.grf\n[Data]\n; a comment\n2=data.zip\n1=uaro.grf\n0=data.grf\r\n");
    CHECK(a.size() == 3 && a[0] == "data.grf" && a[1] == "uaro.grf" && a[2] == "data.zip");
}

TEST(pack_build_sorts_the_games_files) {
    const pack::Choice all;
    using S = pack::Source;
    CHECK(pack::sourceOf("data\\texture\\prontera\\wall.bmp", all).type == S::Texture);
    CHECK(pack::sourceOf("data\\texture\\prontera\\wall.bmp", all).kind == ai::Kind::Ground);
    CHECK(pack::sourceOf("data\\texture\\effect\\ring.tga", all).type == S::Tga);
    CHECK(pack::sourceOf("data\\texture\\effect\\ring.tga", all).kind == ai::Kind::Effects);
    const std::string ui = "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\apple.bmp";
    CHECK(pack::sourceOf(ui, all).type == S::Interface && pack::sourceOf(ui, all).kind == ai::Kind::Icons);
    const std::string monster = "data\\sprite\\\xB8\xF3\xBD\xBA\xC5\xCD\\poring.spr";
    CHECK(pack::sourceOf(monster, all).type == S::Sprite && pack::sourceOf(monster, all).kind == ai::Kind::Sprites);
    CHECK(pack::sourceOf("data\\model\\tree.rsm", all).type == S::None);
    pack::Choice some;
    some.monsters = false;
    some.world = false;
    CHECK(pack::sourceOf(monster, some).type == S::None);
    CHECK(pack::sourceOf("data\\texture\\prontera\\wall.bmp", some).type == S::None);
    CHECK(pack::sourceOf("data\\texture\\effect\\ring.tga", some).type == S::Tga);  // effects still chosen
}

TEST(pack_build_takes_a_file_from_the_first_archive) {
    const std::string dir = tempDir();
    grfWith(dir + "/uaro.grf", {{"data\\texture\\a.bmp", "new"}, {"data\\texture\\b.bmp", "only here"}});
    grfWith(dir + "/data.grf", {{"DATA\\TEXTURE\\A.BMP", "old"}, {"data\\model\\x.rsm", "not a picture"}});
    writeFile(dir + "/DATA.INI", "[Data]\n0=uaro.grf\n1=data.grf\n");
    pack::GameFiles files;
    std::string error;
    CHECK(files.open(dir, error));
    CHECK(files.names() == (std::vector<std::string>{"data\\texture\\a.bmp", "data\\texture\\b.bmp"}));
    std::vector<uint8_t> out;
    CHECK(files.read("data\\texture\\a.bmp", out) && std::string(out.begin(), out.end()) == "new");
    CHECK(files.read("data\\texture\\b.bmp", out) && std::string(out.begin(), out.end()) == "only here");
    CHECK(!files.read("data\\texture\\c.bmp", out));
    std::remove((dir + "/uaro.grf").c_str());
    std::remove((dir + "/data.grf").c_str());
    std::remove((dir + "/DATA.INI").c_str());
    rmdir(dir.c_str());
}

TEST(pack_build_checks_its_keys_against_upscaled_grf) {
    // What the game showed (upscaled.grf: pictures and their kind tags) against the keys the builder made.
    const std::vector<std::string> cache = {
        "ai\\realesr-animevideov3-r2\\256x256\\0000000000000001.tex", "ai\\@kind\\ground\\256x256\\0000000000000001",
        "ai\\realesr-animevideov3-r2\\64x64\\0000000000000002.tex", "ai\\@kind\\sprites\\64x64\\0000000000000002",
        "ai\\realesr-animevideov3-r2\\32x32\\0000000000000003.tex", "ai\\@kind\\sprites\\32x32\\0000000000000003",
        "ai\\@map\\prontera\\256x256\\0000000000000001"};
    const std::unordered_set<std::string> made = {"256x256\\0000000000000001", "64x64\\0000000000000002"};
    const pack::KeyCheck k = pack::checkKeys(cache, made);
    CHECK(k.seen[int(ai::Kind::Ground)] == 1 && k.found[int(ai::Kind::Ground)] == 1);
    CHECK(k.seen[int(ai::Kind::Sprites)] == 2 && k.found[int(ai::Kind::Sprites)] == 1);
    CHECK(k.missing.size() == 1 && k.missing[0] == "sprites 32x32\\0000000000000003");
    CHECK(pack::keyCheckText(k).find("sprites 1 of 2") != std::string::npos);
}
