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
