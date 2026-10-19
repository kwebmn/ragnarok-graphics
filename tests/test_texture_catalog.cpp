#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/ai/groups.hpp"
#include "core/ai/texture_cache.hpp"
#include "core/pack_build.hpp"
#include "core/pack_source.hpp"
#include "core/texture_catalog.hpp"
#include "sample_files.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

const std::string kHuman = "\xC0\xCE\xB0\xA3\xC1\xB7", kHead = "\xB8\xD3\xB8\xAE\xC5\xEB", kHair = "\xB8\xD3\xB8\xAE",
                  kMale = "\xB3\xB2";
const std::string kSprite = "data\\sprite\\" + kHuman + "\\" + kHead + "\\" + kMale + "\\12_" + kMale + ".spr";

std::string hairPalette(int colour) {
    return "data\\palette\\" + kHair + "\\" + kHair + "12_" + kMale + "_" + std::to_string(colour) + ".pal";
}

// A 64 x 64 texture of 16 cells, `seed` making it its own.
std::vector<uint32_t> cells(uint32_t seed) {
    std::vector<uint32_t> p(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            p[size_t(y * 64 + x)] = (seed * 40503u + uint32_t((y / 16) * 4 + x / 16) * 9001u) & 0xF8F8F8u;
    return p;
}

// The game's files in two archives: uaro.grf (0) before data.grf (1).
struct Game {
    std::map<std::string, std::vector<uint8_t>> files;
    std::vector<catalog::ArchiveStamp> archives{{"uaro.grf", 100, 1000}, {"data.grf", 200, 2000}};
    std::vector<std::pair<std::string, std::vector<uint16_t>>> names;
    Game() {
        add("data\\texture\\a.bmp", testfiles::bmp24(64, 64, cells(1)), {0, 1});
        add("data\\texture\\bark.bmp", testfiles::bmp24(64, 64, cells(2)), {1});
        add("data\\texture\\leaf.bmp", testfiles::bmp24(64, 64, cells(3)), {1});
        add(kSprite, testfiles::spr(16, 16, testfiles::checker(16, 16), testfiles::palette(10, 20, 30)), {1});
        for (int c = 0; c <= 24; ++c) add(hairPalette(c), testfiles::palette(uint8_t(40 + c * 8), 60, 70), {1});
        std::vector<testfiles::GndCell> ground(4);
        for (int i = 0; i < 4; ++i) {  // each cell another quarter tile of a.bmp, never the next one
            testfiles::GndCell& c = ground[size_t(i)];
            c.texture = 0;
            const float u0 = 0.25f * float((i * 2) % 4), v0 = 0.25f * float(i);
            for (int k = 0; k < 4; ++k) c.u[k] = u0 + 0.25f * c.u[k], c.v[k] = v0 + 0.25f * c.v[k];
        }
        files["data\\t.gnd"] = testfiles::gnd({"A.BMP"}, 2, 2, ground);
        files["data\\t.rsw"] = testfiles::rsw();
        files["data\\model\\tree.rsm"] = testfiles::rsm();
    }
    void add(const std::string& name, std::vector<uint8_t> data, std::vector<uint16_t> in) {
        files[name] = std::move(data);
        names.emplace_back(name, std::move(in));
    }
    catalog::Read read() const {
        return [this](const std::string& n, std::vector<uint8_t>& out) {
            const auto it = files.find(n);
            if (it == files.end()) return false;
            out = it->second;
            return true;
        };
    }
    std::vector<std::string> allNames() const {
        std::vector<std::string> out;
        for (const auto& n : names) out.push_back(n.first);
        std::sort(out.begin(), out.end());
        return out;
    }
    catalog::Catalog build(const catalog::Catalog* old = nullptr, std::vector<size_t>* decoded = nullptr) const {
        std::vector<size_t> todo;
        catalog::Catalog c = catalog::start(archives, names, pack::Choice(), old, todo);
        for (size_t i : todo) CHECK(catalog::decode(c.files[i], allNames(), read(), pack::Choice(), nullptr));
        catalog::mapsOf(c, {"t"}, read(), nullptr);
        if (decoded) *decoded = todo;
        return c;
    }
};

const catalog::FileEntry* fileOf(const catalog::Catalog& c, const std::string& name) {
    for (const catalog::FileEntry& f : c.files)
        if (f.name == name) return &f;
    return nullptr;
}

}  // namespace

TEST(catalog_lists_every_texture) {
    // 2.3, the studio (T1): every texture of the game - the archive that gives it and those it shadows, its keys
    const Game g;
    const catalog::Catalog c = g.build();
    CHECK_EQ(c.files.size(), size_t(4));  // the palettes are no textures of their own
    const catalog::FileEntry* a = fileOf(c, "data\\texture\\a.bmp");
    CHECK(a && a->archives == (std::vector<uint16_t>{0, 1}));
    CHECK(a && a->pictures.size() == 1);
    pack::Picture p;
    const std::vector<uint8_t>& bytes = g.files.at("data\\texture\\a.bmp");
    CHECK(pack::worldTexture(bytes.data(), bytes.size(), false, p));
    CHECK(a && a->pictures[0].key == p.key && a->pictures[0].width == 64 && a->pictures[0].group == 0);
    CHECK(a && a->pictures[0].lowKeys.size() == 2);  // at the half and the quarter texture detail
    CHECK(a && a->type == uint8_t(pack::Source::Texture));
}

TEST(catalog_takes_the_server_colours) {
    // a head in its own palette and the hair colours the server lets be worn (0-20 of 0-24)
    const Game g;
    const catalog::Catalog c = g.build();
    const catalog::FileEntry* head = fileOf(c, kSprite);
    CHECK(head && head->palettes.size() == 21);
    CHECK(head && head->pictures.size() == 22);
    std::set<std::string> used;
    if (head)
        for (const catalog::PictureOf& p : head->pictures) used.insert(p.palette);
    CHECK(used.count("") && used.count(hairPalette(20)) && !used.count(hairPalette(21)));
}

TEST(catalog_names_the_maps_and_grid) {
    const Game g;
    const catalog::Catalog c = g.build();
    CHECK(c.maps == std::vector<std::string>{"t"});
    const catalog::FileEntry* a = fileOf(c, "data\\texture\\a.bmp");
    CHECK(a && a->maps == std::vector<uint32_t>{0});
    CHECK(a && a->grid.cols == 4 && a->grid.rows == 4);  // its quarters laid side by side out of order: an atlas
    const catalog::FileEntry* bark = fileOf(c, "data\\texture\\bark.bmp");
    CHECK(bark && bark->maps == std::vector<uint32_t>{0} && bark->grid.cols == 1);  // a model's
    CHECK(fileOf(c, kSprite)->maps.empty());
}

TEST(catalog_groups_the_world_by_its_maps) {
    // 2.5.28: a world texture's AI group by the maps that take it - map "t" is none of the towns, fields, dungeons
    // or houses: its ground's are the other maps' ground, its models' the other maps' models; a head by its folder
    const Game g;
    const catalog::Catalog c = g.build();
    CHECK(fileOf(c, "data\\texture\\a.bmp")->pictures.at(0).aiGroup == uint8_t(ai::Group::GroundOther));
    CHECK(fileOf(c, "data\\texture\\bark.bmp")->pictures.at(0).aiGroup == uint8_t(ai::Group::ModelsOther));
    CHECK(fileOf(c, kSprite)->pictures.at(0).aiGroup == uint8_t(ai::Group::Heads));
}

TEST(catalog_keeps_what_did_not_change) {
    // N2: the archives that did not change are not read again - only data.grf's files after it changed
    Game g;
    const catalog::Catalog old = g.build();
    std::vector<size_t> again;
    g.build(&old, &again);
    CHECK(again.empty());
    g.archives[1].time = 3000;
    const catalog::Catalog now = g.build(&old, &again);
    CHECK_EQ(again.size(), size_t(4));  // a.bmp is in data.grf too (shadowed); the rest come from it
    g = Game();
    g.archives[0].size = 101;  // uaro.grf changed: a.bmp alone
    g.build(&old, &again);
    CHECK_EQ(again.size(), size_t(1));
    pack::Choice smaller;  // other sides of the world's textures: other pictures, every file again
    smaller.minSide = 32;
    std::vector<size_t> all;
    catalog::start(Game().archives, Game().names, smaller, &old, all);
    CHECK_EQ(all.size(), size_t(4));
    CHECK(fileOf(now, "data\\texture\\a.bmp")->pictures.size() == 1);
}

TEST(catalog_joins_the_pack) {
    const Game g;
    const catalog::Catalog c = g.build();
    const catalog::PictureOf& a = fileOf(c, "data\\texture\\a.bmp")->pictures[0];
    const catalog::PictureOf& bark = fileOf(c, "data\\texture\\bark.bmp")->pictures[0];
    const catalog::PictureOf& leaf = fileOf(c, "data\\texture\\leaf.bmp")->pictures[0];
    catalog::PackView v;
    v.pictures[pack::pictureName(pack::Group::World, a.key)] = 1760000000;
    v.index.ownAlways.insert(bark.key);
    CHECK(catalog::stateOf(a, v) == catalog::State::InPack);
    CHECK(catalog::stateOf(bark, v) == catalog::State::OwnAlways);
    CHECK(catalog::stateOf(leaf, v) == catalog::State::Missing);
    v.index.aliases[leaf.key] = pack::pictureName(pack::Group::World, a.key);  // its key another's picture
    CHECK(catalog::stateOf(leaf, v) == catalog::State::InPack);
    const auto keys = catalog::byKey(c);
    CHECK(keys.count(a.key) == 1 && keys.count(a.lowKeys[0]) == 1);
}

TEST(catalog_finds_orphans) {
    const Game g;
    const catalog::Catalog c = g.build();
    catalog::PackView v;
    const std::string a = pack::pictureName(pack::Group::World, fileOf(c, "data\\texture\\a.bmp")->pictures[0].key);
    v.pictures[a] = 1;
    v.pictures["world/64x64/00000000000000ff.webp"] = 1;  // a texture the game has no more
    v.pictures[pack::kIndexName] = 1;
    CHECK(catalog::orphans(c, v) == std::vector<std::string>{"world/64x64/00000000000000ff.webp"});
}

TEST(catalog_cache_round_trips) {
    const Game g;
    const catalog::Catalog c = g.build();
    catalog::Catalog back;
    CHECK(catalog::load(catalog::save(c), back));
    CHECK(catalog::save(back) == catalog::save(c));
    CHECK(back.files.size() == c.files.size() && back.maps == c.maps && back.archives.size() == 2);
    CHECK(fileOf(back, kSprite)->palettes.size() == 21);
}

TEST(catalog_cache_refuses_bad_bytes) {
    // an older studio's, one cut short, one broken: refused (and built anew), never a crash or a huge allocation
    const Game g;
    const std::string bytes = catalog::save(g.build());
    catalog::Catalog out;
    for (size_t n = 0; n < bytes.size(); n += 7) CHECK(!catalog::load(bytes.substr(0, n), out));
    std::string other = bytes;
    other[4] = char(99);  // another version
    CHECK(!catalog::load(other, out));
    for (size_t at = 8; at < bytes.size(); at += 13) {  // a count or a length gone huge
        std::string broken = bytes;
        broken[at] = char(0xFF);
        broken[at + 1 < broken.size() ? at + 1 : at] = char(0xFF);
        catalog::load(broken, out);  // whatever it says, it returns
    }
    CHECK(!catalog::load("UCAT", out));
    CHECK(!catalog::load("", out));
}
