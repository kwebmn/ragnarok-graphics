#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "core/ai/groups.hpp"
#include "core/config.hpp"
#include "core/pack_build.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::ai {

// 1.6: a picture's group and model as the pack builder makes it, and as a pack's index tells them.

// The group of a picture the builder makes of a game file (its name, its part in the pack, the picture's size): a
// sprite by its folder, an interface picture of 128 a side and more (a loading screen, a background) apart from the
// icons, an effect's texture, the rest of the world.
Group groupOfPicture(const std::string& fileName, const pack::Source& s, int width, int height);

// A picture's group by the index: its own word (1.6), else what its name and kind tell - nothing for a sprite's frame
// of before 1.6 (the builder's next run writes its group).
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
    uint32_t unknown = 0;  // sprites' frames of before 1.6: their group is not written yet
};
PackCounts countPack(const pack::Index& index, const AiSettings& a);

// Whether a group's pictures are on: the world's by the ground's or the objects' box, the interface's big pictures
// with its icons.
bool groupOn(const AiSettings& a, Group g);

// The AI page's sets of models: fast - anime everywhere; balanced - general for the world, anime for the rest, anime
// 6B for the loading screens; maximum - general for the world, anime 6B for the rest.
enum class ModelSet : uint8_t { Fast, Balanced, Maximum, Own };
std::array<std::string, kGroups> modelsOfSet(ModelSet s);  // Own: none
ModelSet modelSetOf(const AiSettings& a);

}  // namespace uagfx::ai
