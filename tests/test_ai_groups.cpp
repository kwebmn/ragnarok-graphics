#include <string>

#include "core/ai/groups.hpp"
#include "core/config.hpp"
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::ai;

TEST(ai_groups_names_go_both_ways) {
    for (int i = 0; i < kGroups; ++i) {
        Group g = Group::Water;
        CHECK(parseGroup(groupName(Group(i)), g) && g == Group(i));
    }
    for (int i = 0; i < kFamilies; ++i) {
        Family f = Family::Items;
        CHECK(parseFamily(familyName(Family(i)), f) && f == Family(i));
    }
    Group g = Group::Npcs;
    CHECK(!parseGroup("world", g) && g == Group::Npcs);  // 2.5.28: a family's word, no group's
    CHECK(parseGroup("monsters", g) && g == Group::Monsters);  // the family's word is its head's
    CHECK(std::string(groupName(Group::Login)) == "login" && std::string(groupName(Group(kGroups))).empty());
}

TEST(ai_groups_families_and_sections) {
    // 2.5.28: each group's family - the eight of before - its model when it has none of its own
    CHECK(familyOf(Group::GroundTown) == Family::World && familyOf(Group::Water) == Family::World);
    CHECK(familyOf(Group::Mounted) == Family::Players && familyOf(Group::Bosses) == Family::Monsters);
    CHECK(familyOf(Group::Npcs) == Family::Npcs && familyOf(Group::ItemsGround) == Family::Items);
    CHECK(familyOf(Group::ItemIcons) == Family::Icons && familyOf(Group::CardPictures) == Family::Screens);
    CHECK(familyOf(Group::SkillAnimations) == Family::Effects && familyOf(Group::Emotions) == Family::Icons);
    CHECK(familyOf(Group::Loading) == Family::Screens && familyOf(Group::Portraits) == Family::Screens);
    Group head = Group::Water;
    CHECK(headOf(Family::Monsters, head) && head == Group::Monsters);
    CHECK(!headOf(Family::World, head));
    CHECK(sectionOf(Group::GroundOther) == 0 && sectionOf(Group::BuildingsTown) == 1 && sectionOf(Group::Water) == 1);
    CHECK(sectionOf(Group::Bodies) == 2 && sectionOf(Group::Npcs) == 3 && sectionOf(Group::CardPictures) == 4);
    CHECK(sectionOf(Group::EffectSprites) == 5 && sectionOf(Group::Login) == kSections - 1);
}

TEST(ai_groups_models_and_their_work) {
    for (int i = 0; i < kModels; ++i) CHECK(validModel(modelToken(i)) && modelIndex(modelToken(i)) == i);
    CHECK(!validModel("realesrgan") && modelIndex("") == -1 && std::string(modelToken(kModels)).empty());
    CHECK(modelCost("anime") == 1 && modelCost("general") == 2 && modelCost("anime6b") > modelCost("general"));
}

TEST(ai_groups_take_game_ai) {
    // 2.5.27: Game AI 2.0 (ESRGAN, 23 blocks - some 3 times anime 6B's work), the RRDB engine's like anime 6B
    CHECK(validModel("gameai") && modelIndex("gameai") == kModels - 1);
    CHECK(modelCost("gameai") > 3 * modelCost("anime6b") - 1);
    CHECK(rrdbModel("gameai") && rrdbModel("anime6b") && !rrdbModel("anime") && !rrdbModel("general"));
    CHECK(std::string(modelFile("gameai")) == "4x-gameai-2.0");
    CHECK(std::string(modelFile("anime6b")) == "realesrgan-x4plus-anime-6b");
    CHECK(std::string(modelFile("anime")) == "realesr-animevideov3");
    CHECK(std::string(modelFile("general")) == "realesr-general-x4v3");
    AiSettings a = parseConfig("[ai]\nmodel_world = gameai\nsmall_model = gameai\nmodel = gameai\n").ai;
    CHECK(a.modelFor(Group::GroundField) == "gameai" && a.smallModel == "gameai" && a.model == "gameai");
}

TEST(ai_groups_of_the_sprites_folders) {
    using S = sprite_exe::Source;
    CHECK(groupOfSource(uint8_t(S::Player)) == Group::Bodies && groupOfSource(uint8_t(S::Npc)) == Group::Npcs);
    CHECK(groupOfSource(uint8_t(S::Item)) == Group::ItemsGround);
    CHECK(groupOfSource(uint8_t(S::Effect)) == Group::EffectSprites);
    CHECK(groupOfSource(uint8_t(S::Interface)) == Group::Cursor);
    CHECK(groupOfSource(uint8_t(S::Monster)) == Group::Monsters);
    CHECK(groupOfSource(uint8_t(S::Unknown)) == Group::Monsters);  // most sprites are monsters
    CHECK(groupOfKind(Kind::Ground) == Group::GroundOther && groupOfKind(Kind::Sprites) == Group::Monsters);
    CHECK(groupOfKind(Kind::Effects) == Group::SkillAnimations);
}
