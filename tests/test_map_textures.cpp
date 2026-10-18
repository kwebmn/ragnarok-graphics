#include <map>
#include <string>
#include <vector>

#include "core/map_textures.hpp"
#include "sample_files.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A ground file of version 1.7 naming these textures (80 bytes a name), cut after them.
std::vector<uint8_t> ground(const std::vector<std::string>& textures) {
    std::vector<uint8_t> b = {'G', 'R', 'G', 'N', 1, 7};
    const auto put = [&](const void* p, size_t n) {
        b.insert(b.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n);
    };
    const uint32_t width = 2, height = 2, count = uint32_t(textures.size()), length = 80;
    const float zoom = 10.0f;
    put(&width, 4), put(&height, 4), put(&zoom, 4), put(&count, 4), put(&length, 4);
    for (std::string t : textures) {
        t.resize(80, '\0');
        put(t.data(), t.size());
    }
    return b;
}

}  // namespace

TEST(map_textures_of_its_ground_file) {
    // 2.2.5: the textures a map's ground names - the texture pack's pictures of one map made again by another model,
    // or taken out, to test them
    const std::vector<uint8_t> g = ground({"fields\\Sand01.BMP", "BLACK.BMP"});
    const std::vector<std::string> t = map_textures::groundTextures(g.data(), g.size());
    CHECK(t.size() == 2 && t[0] == "fields\\Sand01.BMP" && t[1] == "BLACK.BMP");
    CHECK(map_textures::groundTextures(g.data(), 40).empty());  // cut short
    const std::vector<uint8_t> other = testfiles::rsw();
    CHECK(map_textures::groundTextures(other.data(), other.size()).empty());  // no ground file
}

TEST(map_textures_are_the_game_files_of_its_ground_and_models) {
    const std::map<std::string, std::vector<uint8_t>> files = {
        {"data\\beach.gnd", ground({"fields\\Sand01.BMP", "fields/sand01.bmp", "black.bmp"})},
        {"data\\beach.rsw", testfiles::rsw()},         // one model: tree.rsm
        {"data\\model\\tree.rsm", testfiles::rsm()}};  // bark.bmp, leaf.bmp
    const map_textures::Read read = [&](const std::string& name, std::vector<uint8_t>& out) {
        const auto it = files.find(name);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    };
    using Files = std::vector<std::string>;
    CHECK(map_textures::filesOf("beach", true, true, read) ==
          Files({"data\\texture\\bark.bmp", "data\\texture\\black.bmp", "data\\texture\\fields\\sand01.bmp",
                 "data\\texture\\leaf.bmp"}));
    CHECK(map_textures::filesOf("BEACH.rsw", true, false, read) ==
          Files({"data\\texture\\black.bmp", "data\\texture\\fields\\sand01.bmp"}));
    CHECK(map_textures::filesOf("data\\beach.gnd", false, true, read) ==
          Files({"data\\texture\\bark.bmp", "data\\texture\\leaf.bmp"}));
    CHECK(map_textures::filesOf("nowhere", true, true, read).empty());
    CHECK(map_textures::mapName(" Cmd_Fild02.gnd ") == "cmd_fild02");
    CHECK(map_textures::mapName("data/prontera.rsw") == "prontera");
    CHECK(map_textures::mapName("..\\x").empty());  // a map's name only
}

TEST(ground_uses_of_its_surfaces) {
    // 2.3, the studio: how a ground texture lies on a map - the rectangles of it its surfaces take, and whether
    // neighbouring cells go on across their edge (a texture spread over cells) or not (tiles of an atlas)
    testfiles::GndCell a1, a2, b;
    a1.texture = 0;
    for (int i = 0; i < 4; ++i) a1.u[i] *= 0.25f, a1.v[i] *= 0.25f;  // the first quarter's tile
    a2.texture = 0;
    for (int i = 0; i < 4; ++i) a2.u[i] = 0.5f + a1.u[i], a2.v[i] = 0.25f + a1.v[i];
    b.texture = 1;
    const std::vector<uint8_t> g = testfiles::gnd({"Fields\\A.BMP", "b.bmp"}, 2, 2, {a1, a2, b, {}});
    const auto uses = map_textures::groundUses(g.data(), g.size());
    CHECK_EQ(uses.size(), size_t(2));
    const map_textures::GroundUse& a = uses.at("data\\texture\\fields\\a.bmp");
    CHECK_EQ(a.rects.size(), size_t(2));
    CHECK(a.rects[0].u0 == 0.0f && a.rects[0].u1 == 0.25f && a.rects[0].v1 == 0.25f);
    CHECK(a.rects[1].u0 == 0.5f && a.rects[1].v0 == 0.25f);
    CHECK(a.pairs == 1 && a.joined == 0);  // side by side, but another tile than the next of the texture
    CHECK_EQ(uses.at("data\\texture\\b.bmp").rects.size(), size_t(1));
    // spread over two cells: the second takes up where the first ends
    testfiles::GndCell s1, s2;
    s1.texture = s2.texture = 0;
    for (int i = 0; i < 4; ++i) s1.u[i] *= 0.5f, s2.u[i] = 0.5f + s1.u[i];
    const std::vector<uint8_t> spread = testfiles::gnd({"c.bmp"}, 2, 1, {s1, s2});
    const auto spreadUses = map_textures::groundUses(spread.data(), spread.size());
    const map_textures::GroundUse& c = spreadUses.at("data\\texture\\c.bmp");
    CHECK(c.pairs == 1 && c.joined == 1);
}

TEST(ground_uses_cut_short_is_empty) {
    testfiles::GndCell a;
    a.texture = 0;
    const std::vector<uint8_t> g = testfiles::gnd({"a.bmp"}, 2, 2, {a, a, a, a});
    CHECK(!map_textures::groundUses(g.data(), g.size()).empty());
    for (size_t n = 0; n < g.size(); ++n) CHECK(map_textures::groundUses(g.data(), n).empty());
}

TEST(ground_patch_lies_where_the_texture_is) {
    // the studio's "as the map lays it": the cells of a map about where the texture is laid most
    std::vector<testfiles::GndCell> cells(36);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 6; ++x) {
            testfiles::GndCell& c = cells[size_t(y * 6 + x)];
            c.texture = x >= 3 && y >= 3 ? 0 : 1;
            for (int i = 0; i < 4; ++i) c.u[i] = 0.25f * float(x % 4) + 0.25f * c.u[i];
        }
    const std::vector<uint8_t> g = testfiles::gnd({"a.bmp", "b.bmp"}, 6, 6, cells);
    const std::vector<map_textures::PatchCell> p =
        map_textures::groundPatch(g.data(), g.size(), "data\\texture\\a.bmp", 4);
    CHECK_EQ(p.size(), size_t(9));
    for (const map_textures::PatchCell& c : p) {
        CHECK(c.x >= 0 && c.x < 4 && c.y >= 0 && c.y < 4);
        CHECK(c.u[1] - c.u[0] == 0.25f);
    }
    CHECK(map_textures::groundPatch(g.data(), g.size(), "data\\texture\\none.bmp", 4).empty());
}
