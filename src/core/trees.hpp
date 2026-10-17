#pragma once
#include <string>

namespace uagfx::trees {

// 1.7, the trees of the living world: a draw of leaves (core/foliage.hpp) is lit as a round crown (tree_volume), lit
// through on its sunward edge (leaf_light) and given a shade of its own and the season's colours (tree_tints) -
// shaders.cpp leafLight(), from these numbers.
enum class Season { Green, Autumn, Rime };

// The map's trees by the month (1-12): rime in Lutie all year, autumn colours from September to November where the
// trees are of the kind that turns (not the desert's palms, the jungle, the dead woods of Niflheim).
Season seasonOf(const std::string& map, int month);
int monthNow();  // the computer's calendar, 1-12
// 1.20: a map whose land has no seasons - the desert's, the jungle's, the dead woods', the tropics'
bool evergreen(const std::string& map);

struct Shade {
    float tint[3] = {1.0f, 1.0f, 1.0f};    // multiplies the leaves' colour: each tree its own
    float autumn[3] = {1.0f, 1.0f, 1.0f};  // the colour the leaves turn to (times their brightness) ...
    float autumnAmount = 0.0f;             // ... and how much, 0-1
    float rime = 0.0f;                     // white on the crown's top, 0-1
};

// The shade of the tree whose crown is about (x, z) of the world: smooth over the world (value noise about every 14
// units), so a crown keeps it as the camera turns, and trees apart differ.
Shade shadeAt(float x, float z, Season season);

// How strongly the leaves are lit through: low sun (long shadows) the most; none in the night (night 0-1).
float throughLight(float shadowLength, float night);

}  // namespace uagfx::trees
