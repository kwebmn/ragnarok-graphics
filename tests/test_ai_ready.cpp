#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/ai/ready.hpp"
#include "core/near_picture.hpp"
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
    return s;
}

// A pack of these entries and this index.
void addPack(ReadyPictures& r, const std::set<std::string>& names, const std::string& index) {
    r.addPack([names](const std::string& n) { return names.count(n) != 0; }, index);
}

}  // namespace

TEST(ai_ready_texture_from_the_pack) {
    // 0.47 (Direct3D 11), 0.48 (upscaled.grf gone): the pack's picture of the key, the world's group for the world.
    ReadyPictures r(settings());
    const std::string packed = pack::pictureName(pack::Group::World, kKey);
    addPack(r, {packed}, "");
    std::string name;
    CHECK(r.texture(kKey, Kind::Ground, name));
    CHECK(name == packed);
    CHECK(r.texture(kKey, Kind::Objects, name));
    CHECK(r.texture(kKey, Kind::Sprites, name));  // a world texture drawn as a sprite: the same picture
    CHECK(r.texture(kKey, Kind::Effects, name));
    CHECK(!r.texture("64x64\\0000000000000001", Kind::Ground, name));  // none made
    ReadyPictures empty(settings());  // no pack at all
    CHECK(!empty.texture(kKey, Kind::Ground, name));
}

TEST(ai_ready_texture_of_a_kind_that_is_off_has_none) {
    AiSettings a = settings();
    a.objects = false;
    a.monsters = a.players = a.npcs = a.items = false;
    ReadyPictures r(a);
    addPack(r, {pack::pictureName(pack::Group::World, kKey)}, "");
    std::string name;
    CHECK(r.texture(kKey, Kind::Ground, name));
    CHECK(!r.texture(kKey, Kind::Objects, name));
    CHECK(!r.texture(kKey, Kind::Sprites, name));
    CHECK(!r.texture(kKey, Kind::Lightmap, name));
    CHECK(!r.texture(kKey, Kind::None, name));
}

TEST(ai_ready_texture_at_a_lower_detail_takes_its_whole_textures_picture) {
    // 0.45: the index's "=<key>\t<picture>" lines - the world's only, and only to a picture the pack has.
    ReadyPictures r(settings());
    const std::string whole = pack::pictureName(pack::Group::World, kKey);
    const std::string index =
        pack::indexText({{whole, Kind::Ground}}, {{kLow, whole}, {"32x32\\00000000000000bb", "world/missing.webp"}});
    addPack(r, {whole, pack::kIndexName}, index);
    std::string name;
    CHECK(r.texture(kLow, Kind::Ground, name));
    CHECK(name == whole);
    CHECK(!r.texture("32x32\\00000000000000bb", Kind::Ground, name));
    CHECK(!r.texture(kLow, Kind::Icons, name));  // the interface's pictures have none
    CHECK(r.aliases() == 1);
}

TEST(ai_ready_interface_pictures_icons_and_sprite_frames) {
    ReadyPictures r(settings());
    const std::string ui = pack::pictureName(pack::Group::Ui, kKey);
    const std::string frame = pack::pictureName(pack::Group::Sprites, "40x80\\0000000000000040");
    addPack(r, {ui, frame}, "");
    std::string name;
    CHECK(r.texture(kKey, Kind::Icons, name));  // a big picture of the interface (the loading screen)
    CHECK(name == ui);
    CHECK(r.icon(kKey, name));  // a window's picture
    CHECK(name == ui);
    CHECK(r.cell("40x80\\0000000000000040", name));  // a frame of the sprite cache
    CHECK(name == frame);
    CHECK(!r.cell(kKey, name));
    CHECK(!r.icon("24x24\\0000000000000024", name));
}

TEST(ai_ready_sprite_frames_need_a_kind_of_theirs) {
    AiSettings a = settings();
    a.monsters = a.players = a.npcs = a.items = false;
    a.effects = a.icons = false;
    ReadyPictures r(a);
    addPack(r, {pack::pictureName(pack::Group::Sprites, kKey), pack::pictureName(pack::Group::Ui, kKey)}, "");
    std::string name;
    CHECK(!r.anyCells());
    CHECK(!r.cell(kKey, name));
    CHECK(!r.icon(kKey, name));
    a.icons = true;  // the cursor comes from the sprite cache too
    ReadyPictures r2(a);
    addPack(r2, {pack::pictureName(pack::Group::Sprites, kKey)}, "");
    CHECK(r2.anyCells());
    CHECK(r2.cell(kKey, name));
}

TEST(ai_ready_pictures_the_game_made_and_kinds_it_saw) {
    // 0.48: what the game's own making (Direct3D 12, a debugging aid) wrote into the pack meanwhile is found as the
    // pack's; a kind seen in a draw is known ahead from then on.
    ReadyPictures r(settings());
    addPack(r, {}, "");
    const std::string made = pack::pictureName(pack::Group::World, kKey);
    std::string name;
    CHECK(!r.texture(kKey, Kind::Ground, name));
    r.addMade(made);
    CHECK(r.texture(kKey, Kind::Ground, name));
    CHECK(name == made);
    Kind k = Kind::None;
    CHECK(!r.knownKind(kLow, k));
    r.noteKind(kLow, Kind::Effects);
    CHECK(r.knownKind(kLow, k));
    CHECK(k == Kind::Effects);
    r.noteKind(kLow, Kind::Ground);  // the first kind stays
    CHECK(r.knownKind(kLow, k) && k == Kind::Effects);
}

TEST(ai_ready_kinds_known_ahead) {
    // The pack's index (the world's and the interface's pictures; a sprite frame is no texture of its own). Nothing
    // ahead with [ai] ahead = 0.
    ReadyPictures r(settings());
    const std::string world = pack::pictureName(pack::Group::World, kKey);
    const std::string sprite = pack::pictureName(pack::Group::Sprites, "40x80\\00000000000000ff");
    const std::string index = pack::indexText({{world, Kind::Objects}, {sprite, Kind::Sprites}}, {{kLow, world}});
    addPack(r, {world, sprite}, index);
    Kind k = Kind::None;
    CHECK(r.knownKind(kKey, k));
    CHECK(k == Kind::Objects);
    CHECK(r.knownKind(kLow, k));
    CHECK(k == Kind::Objects);
    CHECK(!r.knownKind("40x80\\00000000000000ff", k));
    CHECK(r.ahead() == 2);
    AiSettings a = settings();
    a.ahead = false;
    ReadyPictures off(a);
    addPack(off, {world}, index);
    CHECK(!off.knownKind(kKey, k));
}

TEST(ai_ready_jpg_pictures_found_by_their_likeness) {
    // 0.49: a loading screen decoded by the game itself (ijl15.dll) has another key than the builder's: the pack's
    // index keeps its likeness ("~" lines) and the game takes the picture most like its texture - of its group only.
    std::vector<uint32_t> screen(256 * 256, 0);
    for (int y = 0; y < 150; ++y)
        for (int x = 0; x < 200; ++x)
            screen[size_t(y) * 256 + size_t(x)] = 0xFF000000u | uint32_t(x) << 16 | uint32_t(y);
    const std::string ui = "ui/256x256/00000000000000aa.webp";
    pack::Index x;
    x.pictures[ui] = pack::IndexEntry{Kind::Icons, {}};
    x.alike[ui] = pack::likenessText(pack::likenessOf(screen.data(), 256, 256, 200, 150));
    ReadyPictures r(settings());
    addPack(r, {ui}, pack::indexText(x));
    CHECK(r.hasAlike(256, 256));
    CHECK(!r.hasAlike(128, 128));
    std::vector<uint32_t> game = screen;
    for (uint32_t& p : game) p = p ? (p & 0xFFF8F8F8u) : 0xFF336699u;  // 16 bits; the rest of the texture anything
    std::string name;
    CHECK(r.alike(game.data(), 256, 256, Kind::Icons, name));
    CHECK(name == ui);
    CHECK(!r.alike(game.data(), 256, 256, Kind::Ground, name));  // a picture of the interface: not for the world
    AiSettings off = settings();
    off.icons = false;
    ReadyPictures r2(off);
    addPack(r2, {ui}, pack::indexText(x));
    CHECK(!r2.alike(game.data(), 256, 256, Kind::Icons, name));
}

TEST(ai_ready_takes_no_studio_line_for_a_picture) {
    // 2.3: the studio's lines ('+', '!', '#') name no picture of the pack
    ReadyPictures r(settings());
    const std::string packed = pack::pictureName(pack::Group::World, kKey);
    pack::Index x;
    x.pictures[packed] = pack::IndexEntry{Kind::Ground, {}};
    x.made[packed] = pack::Provenance{"a.bmp", "data.grf", 1, "general", "general x4"};
    x.materials[kKey] = pack::MaterialScales{};
    addPack(r, {packed}, pack::indexText(x));
    std::string name;
    CHECK(r.texture(kKey, Kind::Ground, name) && name == packed);
    CHECK_EQ(r.ahead(), size_t(1));  // the picture's kind alone
}

TEST(ready_pictures_leave_marked_keys) {
    // 2.3, the studio's "the game's own always" ('!' lines): no picture of the pack for that key, whatever the kind
    ReadyPictures r(settings());
    const std::string world = pack::pictureName(pack::Group::World, kKey);
    const std::string sprite = pack::pictureName(pack::Group::Sprites, kKey);
    const std::string ui = pack::pictureName(pack::Group::Ui, kKey);
    pack::Index x;
    x.pictures[world] = pack::IndexEntry{Kind::Ground, {}};
    x.aliases[kLow] = world;
    x.ownAlways.insert(kKey);
    x.ownAlways.insert(kLow);
    addPack(r, {world, sprite, ui}, pack::indexText(x));
    std::string name;
    CHECK(r.marked(kKey));
    CHECK(!r.texture(kKey, Kind::Ground, name));
    CHECK(!r.texture(kLow, Kind::Ground, name));  // a lower detail's key marked too
    CHECK(!r.cell(kKey, name));
    CHECK(!r.icon(kKey, name));
    ReadyPictures plain(settings());  // without the mark: the pictures as before
    x.ownAlways.clear();
    addPack(plain, {world, sprite, ui}, pack::indexText(x));
    CHECK(!plain.marked(kKey));
    CHECK(plain.texture(kKey, Kind::Ground, name) && plain.cell(kKey, name) && plain.icon(kKey, name));
}
