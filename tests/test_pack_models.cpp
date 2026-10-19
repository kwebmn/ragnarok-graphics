#include "core/ai/pack_models.hpp"
#include "core/config.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::ai;

TEST(groups_models_per_group_and_their_sets) {
    // [ai] model / sprite_model as before; model_<family> (the eight groups of before 2.5.28) over them; 2.5.28:
    // model_<group> over those
    AiSettings a = parseConfig("[ai]\nmodel = general\nsprite_model = anime\nmodel_players = anime6b\n"
                               "model_screens = nonsense\nmodel_water = gameai\nmodel_monsters = anime6b\n"
                               "model_bosses = gameai\n").ai;
    CHECK(a.modelFor(Group::GroundTown) == "general" && a.modelFor(Group::Water) == "gameai");
    CHECK(a.modelFor(Group::Bodies) == "anime6b" && a.modelFor(Group::Garments) == "anime6b");  // the family's
    CHECK(a.modelFor(Group::Loading) == "anime");  // nonsense: as sprite_model
    CHECK(a.modelFor(Group::Monsters) == "anime6b" && a.modelFor(Group::Homunculi) == "anime6b");  // the head's
    CHECK(a.modelFor(Group::Bosses) == "gameai");
    CHECK(parseAiArgument(aiArgument(a)) == a);  // to uaro-render64.exe and back
    CHECK(modelSetOf(a) == ModelSet::Own);
    a.models = modelsOfSet(ModelSet::Balanced);
    CHECK(modelSetOf(a) == ModelSet::Balanced && a.modelFor(Group::Loading) == "anime6b");
    CHECK(a.modelFor(Group::Portraits) == "anime6b" && a.modelFor(Group::Windows) == "anime");
    a.models = modelsOfSet(ModelSet::Maximum);
    CHECK(a.modelFor(Group::Trees) == "general" && a.modelFor(Group::SkillAnimations) == "anime6b");
    CHECK(groupOfSource(2) == Group::Bodies && groupOfSource(0) == Group::Monsters);
    CHECK(groupOfKind(Kind::Icons) == Group::Loading);
    CHECK(groupOfKind(Kind::Objects, MapKind::Indoor) == Group::Furniture);
    CHECK(groupOfKind(Kind::Ground, MapKind::Dungeon) == Group::GroundDungeon);
}

TEST(groups_a_packs_counts_by_group_and_model) {
    AiSettings a;  // general for the world, anime for the rest
    a.models[size_t(Group::Bodies)] = "anime6b";
    pack::Index index;
    index.pictures["world/256x256/a.webp"].kind = Kind::Ground;  // before 1.6: no word
    pack::IndexEntry& old = index.pictures["sprites/32x64/o.webp"];
    old.group = "players";  // a word of before 2.5.28: its group comes with the builder's next run
    old.model = "anime";
    pack::IndexEntry& e = index.pictures["sprites/32x64/e.webp"];
    e.kind = Kind::Sprites;
    e.group = "bodies";
    e.model = "anime";  // made before the bodies got anime 6B
    pack::IndexEntry& f = index.pictures["world/128x128/f.webp"];
    f.kind = Kind::Ground;
    f.group = "ground_field";
    f.model = "general";
    const PackCounts c = countPack(index, a);
    CHECK_EQ(int(c.unknown), 2);
    CHECK_EQ(int(c.groups[size_t(Group::Bodies)].pictures), 1);
    CHECK_EQ(int(c.groups[size_t(Group::Bodies)].other), 1);
    CHECK_EQ(int(c.groups[size_t(Group::Bodies)].byModel[1]), 1);
    CHECK_EQ(int(c.groups[size_t(Group::GroundField)].pictures), 1);
    CHECK_EQ(int(c.groups[size_t(Group::GroundField)].other), 0);
}

TEST(groups_small_pictures_by_their_own_model) {
    AiSettings a = parseConfig("[ai]\nsprite_model = anime6b\nsmall_pictures = 1\nsmall_side = 48\n"
                               "small_model = anime\n").ai;
    CHECK(a.modelForPicture(Group::ItemIcons, 24, 24) == "anime");
    CHECK(a.modelForPicture(Group::Bodies, 40, 90) == "anime6b");  // tall: not small on both sides
    CHECK(parseAiArgument(aiArgument(a)) == a);
    pack::Index index;
    index.pictures["ui/24x24/c.webp"].model = "anime6b";  // made before the rule
    index.pictures["ui/24x24/c.webp"].group = "item_icons";
    CHECK_EQ(int(countPack(index, a).groups[size_t(Group::ItemIcons)].other), 1);
    a.smallPictures = false;
    CHECK(a.modelForPicture(Group::ItemIcons, 24, 24) == "anime6b");
    CHECK_EQ(int(countPack(index, a).groups[size_t(Group::ItemIcons)].other), 0);
}

TEST(groups_shown_by_the_kinds_boxes) {
    AiSettings a;
    a.ground = true;
    a.objects = false;
    CHECK(groupOn(a, Group::GroundTown) && !groupOn(a, Group::BuildingsTown) && !groupOn(a, Group::Water));
    a.icons = false;
    CHECK(!groupOn(a, Group::Emotions) && !groupOn(a, Group::ItemIcons) && !groupOn(a, Group::Loading));
    a.players = true;
    CHECK(groupOn(a, Group::Mounted) && groupOn(a, Group::Garments));
}
