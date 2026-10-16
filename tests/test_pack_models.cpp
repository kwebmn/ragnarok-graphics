#include "core/ai/pack_models.hpp"
#include "core/config.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::ai;

TEST(groups_models_per_group_and_their_sets) {
    // [ai] model / sprite_model as before; model_<group> over them.
    AiSettings a = parseConfig("[ai]\nmodel = general\nsprite_model = anime\nmodel_players = anime6b\n"
                               "model_screens = nonsense\n").ai;
    CHECK(a.modelFor(Group::World) == "general" && a.modelFor(Group::Monsters) == "anime");
    CHECK(a.modelFor(Group::Players) == "anime6b" && a.modelFor(Group::Screens) == "anime");
    CHECK(parseAiArgument(aiArgument(a)) == a);  // to uaro-render64.exe and back
    CHECK(modelSetOf(a) == ModelSet::Own);
    a.models = modelsOfSet(ModelSet::Balanced);
    CHECK(modelSetOf(a) == ModelSet::Balanced && a.modelFor(Group::Screens) == "anime6b");
    a.models = modelsOfSet(ModelSet::Maximum);
    CHECK(a.modelFor(Group::World) == "general" && a.modelFor(Group::Effects) == "anime6b");
    CHECK(groupOfSource(2) == Group::Players && groupOfSource(0) == Group::Monsters);
    CHECK(groupOfKind(Kind::Icons) == Group::Screens && groupOfKind(Kind::Objects) == Group::World);
}

TEST(groups_a_packs_counts_by_group_and_model) {
    AiSettings a;  // general for the world, anime for the rest
    a.models[size_t(Group::Players)] = "anime6b";
    pack::Index index;
    index.pictures["world/256x256/a.webp"].kind = Kind::Ground;                // before 1.6: general
    index.pictures["ui/512x384/b.webp"].kind = Kind::Icons;                    // a big one: a loading screen
    index.pictures["ui/24x24/c.webp"].kind = Kind::Icons;                      // an icon
    index.pictures["sprites/32x64/d.webp"].kind = Kind::Sprites;              // before 1.6, no group
    pack::IndexEntry& e = index.pictures["sprites/32x64/e.webp"];
    e.kind = Kind::Sprites;
    e.group = "players";
    e.model = "anime";  // made before the players got anime 6B
    pack::IndexEntry& f = index.pictures["sprites/32x32/f.webp"];
    f.kind = Kind::Effects;
    f.group = "effects";
    f.model = "anime";
    const PackCounts c = countPack(index, a);
    CHECK_EQ(int(c.unknown), 1);
    CHECK_EQ(int(c.groups[size_t(Group::World)].pictures), 1);
    CHECK_EQ(int(c.groups[size_t(Group::World)].other), 0);
    CHECK_EQ(int(c.groups[size_t(Group::Screens)].pictures), 1);
    CHECK_EQ(int(c.groups[size_t(Group::Icons)].pictures), 1);
    CHECK_EQ(int(c.groups[size_t(Group::Players)].other), 1);
    CHECK_EQ(int(c.groups[size_t(Group::Players)].byModel[1]), 1);
    CHECK_EQ(int(c.groups[size_t(Group::Effects)].other), 0);
    // The builder's own groups.
    pack::Source sprite{pack::Source::Sprite, Kind::Sprites};
    CHECK(groupOfPicture("data\\sprite\\npc\\4_f_kafra1.spr", sprite, 37, 95) == Group::Npcs);
    pack::Source ui{pack::Source::Interface, Kind::Icons};
    CHECK(groupOfPicture("x.bmp", ui, 24, 24) == Group::Icons);
    CHECK(groupOfPicture("x.bmp", ui, 256, 192) == Group::Screens);
}

TEST(groups_small_pictures_by_their_own_model) {
    AiSettings a = parseConfig("[ai]\nsprite_model = anime6b\nsmall_pictures = 1\nsmall_side = 48\n"
                               "small_model = anime\n").ai;
    CHECK(a.modelForPicture(Group::Icons, 24, 24) == "anime");
    CHECK(a.modelForPicture(Group::Players, 40, 90) == "anime6b");  // tall: not small on both sides
    CHECK(parseAiArgument(aiArgument(a)) == a);
    pack::Index index;
    index.pictures["ui/24x24/c.webp"].model = "anime6b";  // made before the rule
    index.pictures["ui/24x24/c.webp"].group = "icons";
    CHECK_EQ(int(countPack(index, a).groups[size_t(Group::Icons)].other), 1);
    a.smallPictures = false;
    CHECK(a.modelForPicture(Group::Icons, 24, 24) == "anime6b");
    CHECK_EQ(int(countPack(index, a).groups[size_t(Group::Icons)].other), 0);
}
