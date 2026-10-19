#pragma once
#include <cstdint>
#include <string>

#include "core/ai/kinds.hpp"

namespace uagfx::ai {

// 1.6: the groups of pictures a model is chosen for - the AI page's rows, the pack index's word for each picture.
// 2.5.28: 38 of them (the user: «а еще более разбито»), by the game's files (core/ai/file_groups.hpp): the world's
// ground and models by the kind of map that has them, the characters' parts by their folders, the bosses by the
// server's monster database, the interface by its folders.
enum class Group : uint8_t {
    GroundTown, GroundField, GroundDungeon, GroundIndoor, GroundOther,
    BuildingsTown, ModelsField, ModelsDungeon, Furniture, ModelsOther, Trees, Plants, Water,
    Bodies, Mounted, Heads, Weapons, Shields, Headgear, Garments,
    Monsters, Bosses, Homunculi, Npcs,
    ItemsGround, ItemIcons, ItemPictures, CardPictures,
    SkillAnimations, EffectSprites,
    Emotions, Cursor, Windows, MenuIcons, Portraits, Maps, Loading, Login
};
constexpr int kGroups = 38;

const char* groupName(Group g);  // "ground_town" ... "login": the ini's model_<name>, the index's word
bool parseGroup(const std::string& name, Group& out);

// The eight groups of before 2.5.28, each group's family now: its model when the group has none of its own
// ([ai] model_world ...), the kinds' boxes that show it, an older index's word (its picture counted as of no group
// until the builder's next run writes its group). Monsters and npcs share their word with the group of that name.
enum class Family : uint8_t { World, Monsters, Players, Npcs, Items, Effects, Icons, Screens };
constexpr int kFamilies = 8;
Family familyOf(Group g);
const char* familyName(Family f);  // "world", "monsters" ... "screens"
bool parseFamily(const std::string& name, Family& out);
// The group whose model is the family's (its word is the family's): monsters, npcs; false for the other families.
bool headOf(Family f, Group& out);
// The heading the AI page shows a group under: 0 the world's ground, 1 its models, 2 the characters, 3 monsters and
// NPCs, 4 items, 5 effects, 6 the interface.
int sectionOf(Group g);
constexpr int kSections = 7;

// The models a group can be made by, in the page's order: Real-ESRGAN's general, its compact anime, the RRDB anime 6B;
// 2.5.27: Game AI 2.0 (ESRGAN of 23 blocks, by Tal, WTFPL - game textures; its file comes apart, get-models.bat).
constexpr int kModels = 4;
const char* modelToken(int i);  // "general", "anime", "anime6b", "gameai"
bool validModel(const std::string& token);
int modelIndex(const std::string& token);  // -1 for none of them
// How much work a picture is for each, against anime (the page shows it).
int modelCost(const std::string& token);
// 2.5.27: its file in uaro-graphics\\ai, without ".bin"; whether the RRDB engine runs it (else the compact one)
const char* modelFile(const std::string& token);
bool rrdbModel(const std::string& token);

// The game's own making of a picture (the driver, [ai] generate): a world texture by the kind it is drawn as and the
// kind of map shown, a sprite cache's frame by its file's folder (sprite_exe::Source as a number; 0 unknown - a
// monster, as most sprites are).
enum class MapKind : uint8_t { Town, Field, Dungeon, Indoor, Other };
Group groupOfKind(Kind k, MapKind map = MapKind::Other);
Group groupOfSource(uint8_t source);

}  // namespace uagfx::ai
