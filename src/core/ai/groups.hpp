#pragma once
#include <cstdint>
#include <string>

#include "core/ai/kinds.hpp"

namespace uagfx::ai {

// 1.6: the groups of pictures a model is chosen for - the AI page's rows, the pack index's word for each picture. The
// world is one (the pack builder cannot tell the ground's textures from the buildings' by their files); the sprites
// go by their files' folders (sprite_exe::Source); the interface's icons and pieces are one, its big pictures (the
// loading screens, the login's backgrounds) another.
enum class Group : uint8_t { World, Monsters, Players, Npcs, Items, Effects, Icons, Screens };
constexpr int kGroups = 8;

const char* groupName(Group g);  // "world", "monsters" ... "screens": the ini's model_<name>, the index's word
bool parseGroup(const std::string& name, Group& out);

// The models a group can be made by, in the page's order: Real-ESRGAN's general, its compact anime, the RRDB anime 6B.
constexpr int kModels = 3;
const char* modelToken(int i);  // "general", "anime", "anime6b"
bool validModel(const std::string& token);
int modelIndex(const std::string& token);  // -1 for none of them
// How much work a picture is for each, against anime (the page shows it).
int modelCost(const std::string& token);

// The group of a world texture by the kind it is drawn as (the driver), and of a sprite cache's frame by its file's
// folder (sprite_exe::Source as a number; 0 unknown - a monster, as most sprites are).
Group groupOfKind(Kind k);
Group groupOfSource(uint8_t source);

}  // namespace uagfx::ai
