#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <map>
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

TEST(pack_build_passes_its_choice_as_words) {
    // 0.43: the settings app starts the builder with the kinds ticked on its page, as words.
    pack::Choice c;
    CHECK_EQ(pack::choiceText(c), std::string("world,effects,monsters,players,npcs,items,interface"));
    pack::Choice some = pack::parseChoice("monsters,interface,nonsense,,world");
    CHECK(some.world && some.monsters && some.interface);
    CHECK(!some.effects && !some.players && !some.npcs && !some.items);
    CHECK_EQ(pack::choiceText(some), std::string("world,monsters,interface"));
    const pack::Choice none = pack::parseChoice("");
    CHECK(!none.world && !none.effects && !none.monsters && !none.players && !none.npcs && !none.items &&
          !none.interface);
    CHECK_EQ(pack::choiceText(none), std::string(""));
    CHECK(pack::anyChosen(some) && !pack::anyChosen(none));
}

TEST(pack_build_writes_and_reads_its_status) {
    // What the builder does, in a file the settings app reads twice a second: a line a value.
    pack::Status s;
    s.state = pack::Status::Building;
    s.filesDone = 120;
    s.filesTotal = 11253;
    s.made = 5000;
    s.toMake = 80000;
    s.inPack = 7000;
    s.bytes = 123456789012ull;
    s.rate = 41.5;
    s.eta = 1807;
    s.pid = 4242;
    s.current = "data\\sprite\\poring.spr\nsecond line";  // never two lines
    s.message = "";
    const std::string text = pack::statusText(s);
    pack::Status r;
    CHECK(pack::parseStatus(text, r));
    CHECK(r.state == pack::Status::Building);
    CHECK(r.filesDone == 120 && r.filesTotal == 11253 && r.made == 5000 && r.toMake == 80000 && r.inPack == 7000);
    CHECK(r.bytes == 123456789012ull && r.eta == 1807 && r.pid == 4242);
    CHECK(r.rate > 41.4 && r.rate < 41.6);
    CHECK_EQ(r.current, std::string("data\\sprite\\poring.spr second line"));
    for (int st = 0; st <= int(pack::Status::Failed); ++st) {
        pack::Status a;
        a.state = pack::Status::State(st);
        a.message = "why";
        pack::Status b;
        CHECK(pack::parseStatus(pack::statusText(a), b) && b.state == a.state && b.message == "why");
    }
    pack::Status bad;
    CHECK(!pack::parseStatus("", bad));
    CHECK(!pack::parseStatus("state=nonsense\n", bad));
}

TEST(pack_build_writes_and_reads_the_packs_index) {
    // index.txt: a line a picture - its name and its kind - so that a pack goes on with its kinds known.
    std::map<std::string, ai::Kind> kinds = {{"world/256x256/0123456789abcdef.webp", ai::Kind::Ground},
                                             {"sprites/64x128/fedcba9876543210.webp", ai::Kind::Sprites},
                                             {"ui/24x24/00000000000000aa.webp", ai::Kind::Icons}};
    const std::string text = pack::indexText(kinds);
    CHECK_EQ(text, std::string("sprites/64x128/fedcba9876543210.webp\tsprites\n"
                               "ui/24x24/00000000000000aa.webp\ticons\n"
                               "world/256x256/0123456789abcdef.webp\tground\n"));
    const auto back = pack::parseIndex(text + "broken line\nworld/32x32/1111111111111111.webp\tnonsense\n");
    CHECK_EQ(back.size(), size_t(4));
    CHECK(back.at("world/256x256/0123456789abcdef.webp") == ai::Kind::Ground);
    CHECK(back.at("ui/24x24/00000000000000aa.webp") == ai::Kind::Icons);
    CHECK(back.at("world/32x32/1111111111111111.webp") == ai::Kind::None);  // a kind it does not know
}
