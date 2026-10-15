#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/ai/ready.hpp"
#include "core/ai/texture_cache.hpp"
#include "core/texture_pack.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::ai;

namespace {

const std::string kKey = "256x256\\0123456789abcdef";
const std::string kLow = "128x128\\00000000000000aa";  // the same texture at half the texture detail

AiSettings settings() {
    AiSettings s;
    s.enabled = true;
    s.ground = s.objects = true;
    s.monsters = s.players = s.npcs = s.items = true;
    s.effects = s.icons = true;
    return s;  // general (denoise 25) for the world, anime for the rest, 4 times
}

// A pack of these entries and this index.
void addPack(ReadyPictures& r, const std::set<std::string>& names, const std::string& index) {
    r.addPack([names](const std::string& n) { return names.count(n) != 0; }, index);
}

}  // namespace

TEST(ai_ready_texture_from_the_pack_first) {
    // 0.47 (Direct3D 11): the pack's picture before upscaled.grf's, as Direct3D 12 looks them up.
    ReadyPictures r(settings());
    const std::string packed = pack::pictureName(pack::Group::World, kKey);
    const std::string grf = entryName(netFolder("general", 25, 4), kKey);
    addPack(r, {packed}, "");
    r.addGrf({grf});
    ReadyPictures::Source s;
    CHECK(r.texture(kKey, Kind::Ground, s));
    CHECK(s.name == packed);
    CHECK(s.pack);
    CHECK(r.texture(kKey, Kind::Objects, s));
    CHECK(s.name == packed);
}

TEST(ai_ready_texture_from_upscaled_grf_by_its_kinds_folder) {
    ReadyPictures r(settings());
    const std::string ground = entryName(netFolder("general", 25, 4), kKey);
    const std::string sprite = entryName(netFolder("anime", 100, 4), kKey);
    r.addGrf({ground, sprite});
    ReadyPictures::Source s;
    CHECK(r.texture(kKey, Kind::Ground, s));
    CHECK(s.name == ground);
    CHECK(!s.pack);
    // A world texture drawn as a sprite or an effect: the sprites' network's folder.
    CHECK(r.texture(kKey, Kind::Sprites, s));
    CHECK(s.name == sprite);
    CHECK(r.texture(kKey, Kind::Effects, s));
    CHECK(s.name == sprite);
    CHECK(!r.texture("64x64\\0000000000000001", Kind::Ground, s));  // none made
    CHECK(r.grfPictures() == 2);
}

TEST(ai_ready_texture_of_a_kind_that_is_off_has_none) {
    AiSettings a = settings();
    a.objects = false;
    a.monsters = a.players = a.npcs = a.items = false;
    ReadyPictures r(a);
    addPack(r, {pack::pictureName(pack::Group::World, kKey)}, "");
    ReadyPictures::Source s;
    CHECK(r.texture(kKey, Kind::Ground, s));
    CHECK(!r.texture(kKey, Kind::Objects, s));
    CHECK(!r.texture(kKey, Kind::Sprites, s));
    CHECK(!r.texture(kKey, Kind::Lightmap, s));
    CHECK(!r.texture(kKey, Kind::None, s));
}

TEST(ai_ready_texture_at_a_lower_detail_takes_its_whole_textures_picture) {
    // 0.45: the index's "=<key>\t<picture>" lines - the world's only, and only to a picture the pack has.
    ReadyPictures r(settings());
    const std::string whole = pack::pictureName(pack::Group::World, kKey);
    const std::string index =
        pack::indexText({{whole, Kind::Ground}}, {{kLow, whole}, {"32x32\\00000000000000bb", "world/missing.webp"}});
    addPack(r, {whole, pack::kIndexName}, index);
    ReadyPictures::Source s;
    CHECK(r.texture(kLow, Kind::Ground, s));
    CHECK(s.name == whole);
    CHECK(s.pack);
    CHECK(!r.texture("32x32\\00000000000000bb", Kind::Ground, s));
    CHECK(!r.texture(kLow, Kind::Icons, s));  // the interface's pictures have none
    CHECK(r.aliases() == 1);
}

TEST(ai_ready_interface_pictures_icons_and_sprite_frames) {
    ReadyPictures r(settings());
    const std::string ui = pack::pictureName(pack::Group::Ui, kKey);
    const std::string cell = entryName(netFolder("anime", 100, 4), "40x80\\00000000000000ff");
    const std::string icon = entryName(kindFolder(netFolder("anime", 100, 4), Kind::Icons), "24x24\\0000000000000024");
    addPack(r, {ui, pack::pictureName(pack::Group::Sprites, "40x80\\0000000000000040")}, "");
    r.addGrf({cell, icon});
    ReadyPictures::Source s;
    // A big picture of the interface (the loading screen): the pack's "ui" group.
    CHECK(r.texture(kKey, Kind::Icons, s));
    CHECK(s.name == ui);
    CHECK(r.icon(kKey, s));
    CHECK(s.name == ui);
    // A window's icon from upscaled.grf: the "-ui" folder.
    CHECK(r.icon("24x24\\0000000000000024", s));
    CHECK(s.name == icon);
    CHECK(!s.pack);
    // A frame of the sprite cache: the pack's "sprites" group, else the sprites' network's folder.
    CHECK(r.cell("40x80\\0000000000000040", s));
    CHECK(s.name == pack::pictureName(pack::Group::Sprites, "40x80\\0000000000000040"));
    CHECK(s.pack);
    CHECK(r.cell("40x80\\00000000000000ff", s));
    CHECK(s.name == cell);
    CHECK(!s.pack);
    CHECK(!r.cell(kKey, s));
}

TEST(ai_ready_sprite_frames_need_a_kind_of_theirs) {
    AiSettings a = settings();
    a.monsters = a.players = a.npcs = a.items = false;
    a.effects = a.icons = false;
    ReadyPictures r(a);
    addPack(r, {pack::pictureName(pack::Group::Sprites, kKey), pack::pictureName(pack::Group::Ui, kKey)}, "");
    ReadyPictures::Source s;
    CHECK(!r.anyCells());
    CHECK(!r.cell(kKey, s));
    CHECK(!r.icon(kKey, s));
    a.icons = true;  // the cursor comes from the sprite cache too
    ReadyPictures r2(a);
    addPack(r2, {pack::pictureName(pack::Group::Sprites, kKey)}, "");
    CHECK(r2.anyCells());
    CHECK(r2.cell(kKey, s));
}

TEST(ai_ready_pictures_twice_the_size_have_their_own_folder) {
    AiSettings a = settings();
    a.scale = 2;
    ReadyPictures r(a);
    const std::string x4 = entryName(netFolder("general", 25, 4), kKey);
    const std::string x2 = entryName(netFolder("general", 25, 2), kKey);
    r.addGrf({x4});
    ReadyPictures::Source s;
    CHECK(!r.texture(kKey, Kind::Ground, s));
    CHECK(r.otherPictures() == 1);
    ReadyPictures r2(a);
    r2.addGrf({x2});
    CHECK(r2.texture(kKey, Kind::Ground, s));
    CHECK(s.name == x2);
}

TEST(ai_ready_kinds_known_ahead) {
    // The pack's index (the world's and the interface's pictures; a sprite frame is no texture of its own) and
    // upscaled.grf's tags; the pack's count first. Nothing ahead with [ai] ahead = 0.
    ReadyPictures r(settings());
    const std::string world = pack::pictureName(pack::Group::World, kKey);
    const std::string sprite = pack::pictureName(pack::Group::Sprites, "40x80\\00000000000000ff");
    const std::string index = pack::indexText({{world, Kind::Objects}, {sprite, Kind::Sprites}}, {{kLow, world}});
    addPack(r, {world, sprite}, index);
    r.addGrf({kindTag(Kind::Ground, kKey), kindTag(Kind::Effects, "64x64\\0000000000000064")});
    Kind k = Kind::None;
    CHECK(r.knownKind(kKey, k));
    CHECK(k == Kind::Objects);
    CHECK(r.knownKind(kLow, k));
    CHECK(k == Kind::Objects);
    CHECK(r.knownKind("64x64\\0000000000000064", k));
    CHECK(k == Kind::Effects);
    CHECK(!r.knownKind("40x80\\00000000000000ff", k));
    CHECK(r.ahead() == 3);
    AiSettings a = settings();
    a.ahead = false;
    ReadyPictures off(a);
    addPack(off, {world}, index);
    CHECK(!off.knownKind(kKey, k));
}
