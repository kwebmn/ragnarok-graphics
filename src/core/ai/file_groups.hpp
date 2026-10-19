#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/ai/groups.hpp"
#include "core/map_textures.hpp"

namespace uagfx::ai {

// 2.5.28: the group of a game's file (core/ai/groups.hpp) - its folders, its name and, for the world's textures, the
// maps that take them. Names as the game's archives have them (CP949), any case, either slash.

// A map's kind by its name: a town of the list; "fild" a field; "dun", Glast Heim, the towers, mazes, the abyss ... a
// dungeon; "_in" a house's inside; the rest - guild castles, arenas, events, the airships - other. An instance
// ("1@tower") as the map it copies.
MapKind mapKindOf(const std::string& map);

// A sprite's group: a body (a mount's by its name: riding, Peco Peco, gryphon, wolf, dragon, Mado Gear ...), a head, a
// job's weapon, headgear, a shield, a garment; a monster (a boss's by the server's database), a homunculus, an NPC;
// an item on the ground; an effect, the emotions, the cursor. One of no folder of these: a monster.
Group spriteGroupOf(const std::string& path);

// An interface picture's group (data\texture\유저인터페이스\...): item icons, item and card pictures, NPC portraits,
// the world map and minimaps, the menu's icons, the login's and character select's screens, the loading screens;
// the rest the windows'.
Group interfaceGroupOf(const std::string& path, int width, int height);

// Which maps take each of the world's textures: their indexes in `kinds` (a map's kind), on their ground (.gnd) and
// through their models (.rsw's .rsm); a texture of a model of the trees' folder as such too.
struct WorldUses {
    std::vector<MapKind> kinds;
    struct Use {
        std::vector<uint32_t> ground, models;
        bool plantModel = false;
    };
    std::map<std::string, Use> textures;  // normal names: "data\texture\..."
};
// Every map's, a model read once whatever maps place it (the builder's start: some 1,250 maps).
WorldUses worldUsesOf(const std::vector<std::string>& maps, const map_textures::Read& read);
// Whether a model's file (data\model\...) is of the trees' and plants' folder.
bool plantModelFile(const std::string& file);

// A world texture's group: water's folder; the trees' and plants' folder (or a model there) by its name - a tree's
// or grass, a bush, a flower; an effect's; else the kind of map that takes it most - on its ground, or through its
// models when more maps take it so (a town's are buildings, a house's inside furniture); one no map takes by its
// folder (the field's floors, the inside's things, the outside's), else other maps' model.
Group worldGroupOf(const std::string& path, const WorldUses* uses);

}  // namespace uagfx::ai
