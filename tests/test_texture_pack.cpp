#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/texture_pack.hpp"
#include "core/webp_image.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::string tempPath() {
    char name[] = "/tmp/uagfx-pack-XXXXXX";
    const int fd = mkstemp(name);
    if (fd >= 0) close(fd);
    std::remove(name);
    return name;
}

std::vector<uint8_t> bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

bool add(pack::Writer& w, const std::string& name, const std::string& data) {
    std::string error;
    return w.add(name, reinterpret_cast<const uint8_t*>(data.data()), data.size(), error);
}

}  // namespace

TEST(texture_pack_names_its_pictures) {
    // 0.42: by what they are made for and the game texture's key, with the ZIP's slashes.
    CHECK(pack::pictureName(pack::Group::World, "256x256\\0123456789abcdef") == "world/256x256/0123456789abcdef.webp");
    CHECK(pack::pictureName(pack::Group::Sprites, "40x80\\00000000000000ff") == "sprites/40x80/00000000000000ff.webp");
    CHECK(pack::pictureName(pack::Group::Ui, "24x24\\1") == "ui/24x24/1.webp");
}

TEST(texture_pack_knows_the_key_of_a_picture) {
    // 0.44: the index's names back to the game texture's key - the driver takes a map's textures from the pack as the
    // game loads them, before they are drawn.
    pack::Group g = pack::Group::Sprites;
    std::string key;
    CHECK(pack::keyOfPicture("world/256x256/0123456789abcdef.webp", g, key));
    CHECK(g == pack::Group::World && key == "256x256\\0123456789abcdef");
    CHECK(pack::keyOfPicture("ui/24x24/00000000000000aa.webp", g, key));
    CHECK(g == pack::Group::Ui && key == "24x24\\00000000000000aa");
    CHECK(pack::keyOfPicture("sprites/64x128/fedcba9876543210.webp", g, key));
    CHECK(g == pack::Group::Sprites && key == "64x128\\fedcba9876543210");
    for (const char* bad : {"index.txt", "world/256x256.webp", "other/8x8/1.webp", "world/8x8/1.png", "world//1.webp",
                            "world/8x8/.webp", "world/8x8/a/b.webp"})
        CHECK(!pack::keyOfPicture(bad, g, key));
}

TEST(texture_pack_index_holds_the_lower_details_keys) {
    // 0.45: a texture at a lower detail (the game's TEXTUREMODE) has a key of its own; the index sends it to the
    // picture of the whole texture - "=<key>\t<picture>" lines, which the pictures' lines leave alone.
    const std::map<std::string, ai::Kind> kinds = {{"world/256x256/00000000000000aa.webp", ai::Kind::Objects}};
    const std::map<std::string, std::string> aliases = {
        {"128x128\\00000000000000bb", "world/256x256/00000000000000aa.webp"},
        {"64x64\\00000000000000cc", "world/256x256/00000000000000aa.webp"}};
    const std::string text = pack::indexText(kinds, aliases);
    CHECK_EQ(text, std::string("world/256x256/00000000000000aa.webp\tobjects\n"
                               "=128x128\\00000000000000bb\tworld/256x256/00000000000000aa.webp\n"
                               "=64x64\\00000000000000cc\tworld/256x256/00000000000000aa.webp\n"));
    const auto back = pack::parseIndex(text);
    CHECK(back.size() == 1 && back.at("world/256x256/00000000000000aa.webp") == ai::Kind::Objects);
    const auto alias = pack::parseAliases(text + "=broken\n=\tworld/1x1/1.webp\n");
    CHECK(alias == aliases);
}

TEST(texture_pack_reads_back_what_it_wrote) {
    const std::string path = tempPath();
    std::string error;
    {
        pack::Writer w;
        CHECK(w.open(path, false, error));
        CHECK(add(w, "world/2x2/a.webp", "first"));
        CHECK(add(w, "ui/1x1/b.webp", std::string(1000, 'x')));
        CHECK(add(w, "world/2x2/a.webp", "again"));  // there already: kept as it was
        CHECK_EQ(int(w.count()), 2);
        CHECK(w.close("world/2x2/a.webp\tground\tprontera\n", error));
    }
    pack::Reader r;
    CHECK(r.open(path, error));
    CHECK_EQ(int(r.count()), 3);  // and the index
    std::vector<uint8_t> out;
    CHECK(r.read("world/2x2/a.webp", out) && out == bytes("first"));
    CHECK(r.read("ui/1x1/b.webp", out) && out.size() == 1000 && out[999] == 'x');
    CHECK(r.read(pack::kIndexName, out) && out == bytes("world/2x2/a.webp\tground\tprontera\n"));
    CHECK(!r.has("world/2x2/c.webp") && !r.read("world/2x2/c.webp", out));
    r.close();
    std::remove(path.c_str());
}

TEST(texture_pack_goes_on_where_it_stopped) {
    // A pause: the pictures stay, new ones go after them, the index is written anew.
    const std::string path = tempPath();
    std::string error;
    {
        pack::Writer w;
        CHECK(w.open(path, false, error));
        CHECK(add(w, "world/1x1/a.webp", "aaa"));
        CHECK(w.close("old index\n", error));
    }
    {
        pack::Writer w;
        CHECK(w.open(path, true, error));
        CHECK(w.has("world/1x1/a.webp"));
        CHECK_EQ(int(w.count()), 1);
        CHECK(add(w, "world/1x1/b.webp", "bbbb"));
        CHECK(w.close("new index\n", error));
    }
    pack::Reader r;
    CHECK(r.open(path, error));
    CHECK_EQ(int(r.count()), 3);
    std::vector<uint8_t> out;
    CHECK(r.read("world/1x1/a.webp", out) && out == bytes("aaa"));
    CHECK(r.read("world/1x1/b.webp", out) && out == bytes("bbbb"));
    CHECK(r.read(pack::kIndexName, out) && out == bytes("new index\n"));
    r.close();
    std::remove(path.c_str());
}

TEST(texture_pack_keeps_the_pictures_of_a_run_that_broke_off) {
    // The builder stopped (the power went) after a picture but before its central directory: the pictures written
    // are found again by their local headers.
    const std::string path = tempPath();
    std::string error;
    {
        pack::Writer w;
        CHECK(w.open(path, false, error));
        CHECK(add(w, "world/1x1/a.webp", "aaa"));
        CHECK(add(w, "world/1x1/b.webp", "bbb"));
        CHECK(w.checkpoint("index\n", error));
        CHECK(add(w, "world/1x1/c.webp", std::string(300, 'c')));  // over the index and the directory
    }  // no close: broken off
    // 0.48: the game reads such a pack too, by the same headers - without its index.
    pack::Reader broken;
    CHECK(broken.open(path, error));
    CHECK(broken.recovered());
    CHECK_EQ(int(broken.count()), 3);
    std::vector<uint8_t> got;
    CHECK(broken.read("world/1x1/c.webp", got) && got.size() == 300);
    CHECK(!broken.has(pack::kIndexName));
    broken.close();
    {
        pack::Writer w;
        CHECK(w.open(path, true, error));
        CHECK_EQ(int(w.count()), 3);
        CHECK(w.has("world/1x1/c.webp"));
        CHECK(w.close("", error));
    }
    pack::Reader r;
    CHECK(r.open(path, error));
    CHECK(!r.recovered());
    std::vector<uint8_t> out;
    CHECK(r.read("world/1x1/c.webp", out) && out.size() == 300);
    CHECK_EQ(r.sizeOf("world/1x1/c.webp"), uint64_t(300));
    CHECK_EQ(r.sizeOf("world/1x1/none.webp"), uint64_t(0));
    r.close();
    std::remove(path.c_str());
}

TEST(texture_pack_index_holds_the_maps_of_its_pictures) {
    // 0.48: the game's own making notes the maps a picture was shown on ("name\tkind\tmap,map"); the builder's
    // pictures have none. The lower details' keys go with them; a line it cannot read is left out.
    pack::Index x;
    x.pictures["world/256x256/00000000000000aa.webp"] = pack::IndexEntry{ai::Kind::Ground, {"prontera", "geffen"}};
    x.pictures["sprites/64x64/00000000000000bb.webp"] = pack::IndexEntry{ai::Kind::Sprites, {}};
    x.aliases["128x128\\00000000000000cc"] = "world/256x256/00000000000000aa.webp";
    const std::string text = pack::indexText(x);
    CHECK_EQ(text, std::string("sprites/64x64/00000000000000bb.webp\tsprites\n"
                               "world/256x256/00000000000000aa.webp\tground\tgeffen,prontera\n"
                               "=128x128\\00000000000000cc\tworld/256x256/00000000000000aa.webp\n"));
    const pack::Index back = pack::parseIndexFile(text + "no tab\nworld/1x1/1.webp\tnonsense\t,,payon,\n");
    CHECK_EQ(back.pictures.size(), size_t(3));
    const std::string aa = "world/256x256/00000000000000aa.webp";
    CHECK(back.pictures.at(aa) == x.pictures.at(aa));
    CHECK(back.pictures.at("sprites/64x64/00000000000000bb.webp").maps.empty());
    const pack::IndexEntry& odd = back.pictures.at("world/1x1/1.webp");
    CHECK(odd.kind == ai::Kind::None && odd.maps == std::set<std::string>{"payon"});
    CHECK(back.aliases == x.aliases);
    // 0.49: the likeness of a picture of a .jpg, on a line of its own.
    pack::Index withNear = x;
    withNear.alike["ui/1024x1024/0000000000000001.webp"] = "1024\t1024\t800\t600\tab";
    const pack::Index nearBack = pack::parseIndexFile(pack::indexText(withNear));
    CHECK(nearBack.alike == withNear.alike);
    CHECK(nearBack.pictures.size() == 2 && !nearBack.pictures.count("~ui/1024x1024/0000000000000001.webp"));
    // The kinds alone, as before (0.43): the maps after the kind do not change it.
    CHECK(pack::parseIndex(text).at("world/256x256/00000000000000aa.webp") == ai::Kind::Ground);
}

TEST(texture_pack_rewrites_itself_without_some_pictures) {
    // 0.48: the Ready textures page deletes pictures - the pack again without them, the index with what stays (a
    // lower detail's key to a picture gone goes too).
    const std::string path = tempPath(), out = tempPath();
    std::string error;
    {
        pack::Writer w;
        CHECK(w.open(path, false, error));
        CHECK(add(w, "world/1x1/a.webp", "aaa"));
        CHECK(add(w, "world/1x1/b.webp", "bbbb"));
        CHECK(add(w, "ui/1x1/c.webp", "cc"));
        pack::Index x;
        x.pictures["world/1x1/a.webp"] = pack::IndexEntry{ai::Kind::Ground, {"prontera"}};
        x.pictures["world/1x1/b.webp"] = pack::IndexEntry{ai::Kind::Objects, {}};
        x.pictures["ui/1x1/c.webp"] = pack::IndexEntry{ai::Kind::Icons, {}};
        x.aliases["1x1\\1"] = "world/1x1/a.webp";
        x.aliases["1x1\\2"] = "world/1x1/b.webp";
        CHECK(w.close(pack::indexText(x), error));
    }
    size_t kept = 0;
    CHECK(pack::rewrite(path, out, [](const std::string& n) { return n != "world/1x1/b.webp"; }, kept, error));
    CHECK_EQ(kept, size_t(2));
    pack::Reader r;
    CHECK(r.open(out, error));
    CHECK_EQ(int(r.count()), 3);  // two pictures and the index
    std::vector<uint8_t> data;
    CHECK(r.read("world/1x1/a.webp", data) && data == bytes("aaa"));
    CHECK(r.read("ui/1x1/c.webp", data) && data == bytes("cc"));
    CHECK(!r.has("world/1x1/b.webp"));
    CHECK(r.read(pack::kIndexName, data));
    const pack::Index x = pack::parseIndexFile(std::string(data.begin(), data.end()));
    CHECK_EQ(x.pictures.size(), size_t(2));
    CHECK(x.pictures.at("world/1x1/a.webp").maps == std::set<std::string>{"prontera"});
    CHECK(x.aliases.size() == 1 && x.aliases.count("1x1\\1"));
    r.close();
    CHECK(!pack::rewrite(path + ".none", out, [](const std::string&) { return true; }, kept, error));
    std::remove(path.c_str());
    std::remove(out.c_str());
}

TEST(texture_pack_holds_more_than_a_plain_zip_does) {
    // ZIP64: past 65535 entries (a whole game's sprite frames are some 130 thousand).
    const std::string path = tempPath();
    std::string error;
    {
        pack::Writer w;
        CHECK(w.open(path, false, error));
        bool ok = true;
        for (int i = 0; i < 70000 && ok; ++i) ok = add(w, "sprites/1x1/" + std::to_string(i) + ".webp", "p");
        CHECK(ok);
        CHECK(w.close("", error));
    }
    pack::Reader r;
    CHECK(r.open(path, error));
    CHECK_EQ(int(r.count()), 70000);
    std::vector<uint8_t> out;
    CHECK(r.read("sprites/1x1/69999.webp", out) && out == bytes("p"));
    r.close();
    std::remove(path.c_str());
}

TEST(webp_keeps_a_picture_and_its_transparency) {
    // 0.42: a red square on nothing - lossless as it was; lossy close to it, the nothing still nothing.
    const int w = 32, h = 32;
    std::vector<uint32_t> argb(size_t(w * h), 0x00000000u);
    for (int y = 8; y < 24; ++y)
        for (int x = 8; x < 24; ++x) argb[size_t(y * w + x)] = 0xFFC02010u;
    std::vector<uint32_t> back;
    int bw = 0, bh = 0;
    const std::vector<uint8_t> exact = webp::encode(argb.data(), w, h, 100);
    CHECK(!exact.empty());
    CHECK(webp::decode(exact.data(), exact.size(), back, bw, bh));
    CHECK(bw == w && bh == h && back == argb);
    const std::vector<uint8_t> lossy = webp::encode(argb.data(), w, h, 90);
    CHECK(!lossy.empty());
    CHECK(webp::decode(lossy.data(), lossy.size(), back, bw, bh));
    CHECK(bw == w && bh == h);
    CHECK_EQ(back[0] >> 24, 0u);  // a corner: transparent
    const uint32_t mid = back[size_t(16 * w + 16)];
    CHECK(mid >> 24 == 255u && ((mid >> 16) & 255) > 170 && ((mid >> 8) & 255) < 60);
    CHECK(!webp::decode(exact.data(), exact.size() / 2, back, bw, bh));  // cut short: nothing
}

TEST(pack_writer_replaces_a_picture_and_the_index_keeps_its_model) {
    const std::string path = tempPath();
    std::string error;
    {
        uagfx::pack::Writer w;
        CHECK(w.open(path, false, error));
        const uint8_t a[3] = {1, 2, 3}, b[4] = {9, 8, 7, 6};
        CHECK(w.add("sprites/32x32/aa.webp", a, 3, error));
        CHECK(w.add("sprites/32x32/aa.webp", b, 4, error));  // kept as it was
        CHECK(w.add("sprites/32x32/aa.webp", b, 4, error, true));  // 1.6: replaced
        CHECK_EQ(int(w.count()), 1);
        uagfx::pack::Index index;
        index.pictures["sprites/32x32/aa.webp"].kind = uagfx::ai::Kind::Sprites;
        index.pictures["sprites/32x32/aa.webp"].model = "anime6b";
        index.pictures["sprites/32x32/aa.webp"].group = "players";
        CHECK(w.close(uagfx::pack::indexText(index), error));
    }
    uagfx::pack::Reader r;
    CHECK(r.open(path, error));
    std::vector<uint8_t> data;
    CHECK(r.read("sprites/32x32/aa.webp", data) && data.size() == 4 && data[0] == 9);
    CHECK(r.read(uagfx::pack::kIndexName, data));
    const uagfx::pack::Index back = uagfx::pack::parseIndexFile(std::string(data.begin(), data.end()));
    const auto& e = back.pictures.at("sprites/32x32/aa.webp");
    CHECK(e.kind == uagfx::ai::Kind::Sprites && e.model == "anime6b" && e.group == "players");
    CHECK_EQ(int(back.pictures.size()), 1);
}
