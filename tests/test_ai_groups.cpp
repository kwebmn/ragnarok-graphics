#include <string>

#include "core/ai/groups.hpp"
#include "core/config.hpp"
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::ai;

TEST(ai_groups_names_go_both_ways) {
    for (int i = 0; i < kGroups; ++i) {
        Group g = Group::World;
        CHECK(parseGroup(groupName(Group(i)), g) && g == Group(i));
    }
    Group g = Group::Items;
    CHECK(!parseGroup("sprites", g) && g == Group::Items);  // not a group: left as it was
    CHECK(std::string(groupName(Group::Screens)) == "screens" && std::string(groupName(Group(kGroups))).empty());
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
    CHECK(a.modelFor(Group::World) == "gameai" && a.smallModel == "gameai" && a.model == "gameai");
}

TEST(ai_groups_of_the_sprites_folders) {
    using S = sprite_exe::Source;
    CHECK(groupOfSource(uint8_t(S::Player)) == Group::Players && groupOfSource(uint8_t(S::Npc)) == Group::Npcs);
    CHECK(groupOfSource(uint8_t(S::Item)) == Group::Items && groupOfSource(uint8_t(S::Effect)) == Group::Effects);
    CHECK(groupOfSource(uint8_t(S::Interface)) == Group::Icons);  // the cursor
    CHECK(groupOfSource(uint8_t(S::Monster)) == Group::Monsters);
    CHECK(groupOfSource(uint8_t(S::Unknown)) == Group::Monsters);  // most sprites are monsters
    CHECK(groupOfKind(Kind::Ground) == Group::World && groupOfKind(Kind::Sprites) == Group::Monsters);
    CHECK(groupOfKind(Kind::Effects) == Group::Effects);
}
