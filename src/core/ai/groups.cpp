#include "core/ai/groups.hpp"

#include "core/sprite_exe.hpp"

namespace uagfx::ai {
namespace {

const char* const kNames[kGroups] = {"world", "monsters", "players", "npcs", "items", "effects", "icons", "screens"};
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

Group groupOfKind(Kind k) {
    switch (k) {
        case Kind::Sprites: return Group::Monsters;  // a world texture drawn facing the screen
        case Kind::Effects: return Group::Effects;
        case Kind::Icons: return Group::Screens;      // through the world's way only the big pictures come
        default: return Group::World;
    }
}

Group groupOfSource(uint8_t source) {
    switch (sprite_exe::Source(source)) {
        case sprite_exe::Source::Player: return Group::Players;
        case sprite_exe::Source::Npc: return Group::Npcs;
        case sprite_exe::Source::Item: return Group::Items;
        case sprite_exe::Source::Effect: return Group::Effects;
        case sprite_exe::Source::Interface: return Group::Icons;  // the cursor
        case sprite_exe::Source::Monster:
        case sprite_exe::Source::Unknown: break;
    }
    return Group::Monsters;
}

}  // namespace uagfx::ai
