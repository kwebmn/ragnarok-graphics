#pragma once
#include <string>

namespace uagfx::homunai {

// 3.0.11-beta.290 (the owner's feature 22): our own AI for the homunculus and the mercenary (src/core/ai_lua, Lua 5.0
// as uaRO.exe runs it from AI\USER_AI after /hoai and /merai): follows the owner, defends it, attacks by itself if
// chosen, heals the owner (Lif) and strikes with its skill (Filir, Vanilmirth). The driver writes its files where none
// is or ours is (the first line kMarker), never over another AI; its choices in uaro_config.lua.
constexpr const char* kMarker = "-- uaRO Graphics AI";
const char* aiLua();   // AI.lua
const char* aiMLua();  // AI_M.lua
struct Choices {
    bool aggressive = false, defend = true, skills = true;
    int follow = 3, healAt = 50, skillLevel = 1;
};
std::string config(const Choices& c);  // uaro_config.lua's text
// May a file with these first bytes be written over: none there, or ours
bool ours(const std::string& existing);

}  // namespace uagfx::homunai
