#include "core/ai/groups.hpp"

#include "core/sprite_exe.hpp"

namespace uagfx::ai {
namespace {

const char* const kNames[kGroups] = {
    "ground_town", "ground_field", "ground_dungeon", "ground_indoor", "ground_other",
    "buildings_town", "models_field", "models_dungeon", "furniture", "models_other", "trees", "plants", "water",
    "bodies", "mounted", "heads", "weapons", "shields", "headgear", "garments",
    "monsters", "bosses", "homunculi", "npcs",
    "items_ground", "item_icons", "item_pictures", "card_pictures",
    "skill_animations", "effect_sprites",
    "emotions", "cursor", "windows", "menu_icons", "portraits", "maps", "loading", "login"};
const char* const kFamilyNames[kFamilies] = {"world", "monsters", "players", "npcs", "items", "effects", "icons",
                                             "screens"};
const char* const kModelTokens[kModels] = {"general", "anime", "anime6b", "gameai"};

}  // namespace

const char* groupName(Group g) { return int(g) < kGroups ? kNames[int(g)] : ""; }

bool parseGroup(const std::string& name, Group& out) {
    for (int i = 0; i < kGroups; ++i)
        if (name == kNames[i]) {
            out = Group(i);
            return true;
        }
    return false;
}

Family familyOf(Group g) {
    const int i = int(g);
    if (i <= int(Group::Water)) return Family::World;
    if (i <= int(Group::Garments)) return Family::Players;
    if (i <= int(Group::Homunculi)) return Family::Monsters;
    switch (g) {
        case Group::Npcs: return Family::Npcs;
        case Group::ItemsGround: return Family::Items;
        case Group::ItemIcons:
        case Group::ItemPictures: return Family::Icons;  // small pictures of the interface, as before
        case Group::CardPictures: return Family::Screens;  // a card's picture is a big one
        case Group::SkillAnimations:
        case Group::EffectSprites: return Family::Effects;
        case Group::Emotions:
        case Group::Cursor:
        case Group::Windows:
        case Group::MenuIcons: return Family::Icons;
        default: return Family::Screens;  // portraits, maps, loading and login screens
    }
}

const char* familyName(Family f) { return int(f) < kFamilies ? kFamilyNames[int(f)] : ""; }

bool parseFamily(const std::string& name, Family& out) {
    for (int i = 0; i < kFamilies; ++i)
        if (name == kFamilyNames[i]) {
            out = Family(i);
            return true;
        }
    return false;
}

bool headOf(Family f, Group& out) {
    if (f == Family::Monsters) out = Group::Monsters;
    else if (f == Family::Npcs) out = Group::Npcs;
    else return false;
    return true;
}

int sectionOf(Group g) {
    const int i = int(g);
    if (i <= int(Group::GroundOther)) return 0;
    if (i <= int(Group::Water)) return 1;
    if (i <= int(Group::Garments)) return 2;
    if (i <= int(Group::Npcs)) return 3;
    if (i <= int(Group::CardPictures)) return 4;
    if (i <= int(Group::EffectSprites)) return 5;
    return 6;
}

const char* modelToken(int i) { return i >= 0 && i < kModels ? kModelTokens[i] : ""; }
bool validModel(const std::string& token) { return modelIndex(token) >= 0; }

int modelIndex(const std::string& token) {
    for (int i = 0; i < kModels; ++i)
        if (token == kModelTokens[i]) return i;
    return -1;
}

int modelCost(const std::string& token) {
    if (token == "general") return 2;   // 32 convolutions against anime's 16
    if (token == "anime6b") return 14;  // about 5.7 million multiply-adds a pixel and the tiles' margins (1.5)
    if (token == "gameai") return 44;   // 2.5.27: 23 blocks for 6, about 17.9 million a pixel
    return 1;
}

const char* modelFile(const std::string& token) {
    if (token == "anime6b") return "realesrgan-x4plus-anime-6b";  // 1.5
    if (token == "gameai") return "4x-gameai-2.0";                // 2.5.27
    return token == "anime" ? "realesr-animevideov3" : "realesr-general-x4v3";
}

bool rrdbModel(const std::string& token) { return token == "anime6b" || token == "gameai"; }

Group groupOfKind(Kind k, MapKind map) {
    switch (k) {
        case Kind::Sprites: return Group::Monsters;  // a world texture drawn facing the screen
        case Kind::Effects: return Group::SkillAnimations;
        case Kind::Icons: return Group::Loading;      // through the world's way only the big pictures come
        case Kind::Objects: {
            const Group models[] = {Group::BuildingsTown, Group::ModelsField, Group::ModelsDungeon, Group::Furniture,
                                    Group::ModelsOther};
            return models[int(map) < 5 ? int(map) : 4];
        }
        default: return Group(int(Group::GroundTown) + (int(map) < 5 ? int(map) : 4));
    }
}

Group groupOfSource(uint8_t source) {
    switch (sprite_exe::Source(source)) {
        case sprite_exe::Source::Player: return Group::Bodies;
        case sprite_exe::Source::Npc: return Group::Npcs;
        case sprite_exe::Source::Item: return Group::ItemsGround;
        case sprite_exe::Source::Effect: return Group::EffectSprites;
        case sprite_exe::Source::Interface: return Group::Cursor;
        case sprite_exe::Source::Monster:
        case sprite_exe::Source::Unknown: break;
    }
    return Group::Monsters;
}

}  // namespace uagfx::ai
