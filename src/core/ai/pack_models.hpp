#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "core/ai/file_groups.hpp"
#include "core/ai/groups.hpp"
#include "core/config.hpp"
#include "core/pack_build.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::ai {

// 1.6: a picture's group and model as the pack builder makes it, and as a pack's index tells them.

// The group of a picture the builder makes of a game file (its name, its part in the pack, the picture's size) -
// 2.5.28: core/ai/file_groups.hpp's: a sprite by its folders, an interface picture by its, a world texture by the maps
// that take it (uses: the builder's reading of every map; none - by its folder).
Group groupOfPicture(const std::string& fileName, const pack::Source& s, int width, int height,
                     const WorldUses* uses = nullptr);

// A picture's group by the index: its own word - 2.5.28: one of the 38; none for a word of before (a family's) or
// none at all: the builder's next run writes its group.
std::optional<Group> groupOfEntry(const std::string& name, const pack::IndexEntry& e);
// The model that made it: its own word (1.6), else the one the settings had for such pictures before 1.6 - [ai]
// model for the world, sprite_model for the rest.
std::string modelOfEntry(const pack::IndexEntry& e, Group g, const AiSettings& a);

// What a pack holds, by group: the pictures, and how many of them another model made than the group's now.
struct GroupCount {
    uint32_t pictures = 0;
    uint32_t other = 0;  // made by another model than modelFor(group)
    std::array<uint32_t, kModels> byModel{};
};
struct PackCounts {
    std::array<GroupCount, kGroups> groups{};
    uint32_t unknown = 0;  // pictures whose group is not written yet (made before 2.5.28's groups, or 1.6)
};
PackCounts countPack(const pack::Index& index, const AiSettings& a);

// Whether a group's pictures are on: the world's ground by the ground's box, its models by the objects', the
// interface's (item icons and pictures too) by the icons'.
bool groupOn(const AiSettings& a, Group g);

// The AI page's sets of models: fast - anime everywhere; balanced - general for the world, anime for the rest, anime
// 6B for the big pictures (the screens family: portraits, maps, cards, loading and login); maximum - general for the
// world, anime 6B for the rest.
enum class ModelSet : uint8_t { Fast, Balanced, Maximum, Own };
std::array<std::string, kGroups> modelsOfSet(ModelSet s);  // Own: none
ModelSet modelSetOf(const AiSettings& a);

}  // namespace uagfx::ai
