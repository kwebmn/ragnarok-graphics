#include <string>

#include "core/ai/groups.hpp"
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
