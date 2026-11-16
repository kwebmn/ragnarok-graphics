#pragma once
#include <cstddef>
#include <vector>

#include "core/gnd.hpp"
#include "core/rooms.hpp"

// 3.0.20-beta.71 (the owner's check list, 18: «другие комнаты не видны, но есть стены чёрные, они там не продуманы.
// но можно же что-то придумать?», then his pick of the interiors' options, «ок, а»): in a house's map the makers
// raised blocks of their black filler (BLACK.BMP, BACKSIDE.BMP) beside the rooms. The side of a block towards the
// room carries the wall's picture; its top and its other sides are black, and some have no face at all - from above
// nobody saw them, from the eyes' level they stand in the view (payon_in01: 53 % of the rooms' edges adjoin such
// blocks, 24-40 over the floor; alberta_in, aldeba_in, izlude_in, in_orcs01 alike -
// /root/uagfx-tools/audit/ceiling/edges.cpp). The blocks beside a room are given that room's own wall picture: on
// their black sides and on the sides the makers left without a face, laid over the cells as the room's wall lays
// it; on their tops the picture's upper band alone (its rim - a wall with its windows laid flat is no wall's top;
// seen on payon_in01's offline pictures, /root/uagfx-tools/audit/blocks). Lit by the room's walls' lightmaps evened
// out (one wall's own carries its furniture's shadows). The ground file in memory is changed, before the own ground
// is built from it - the game's own ground stays as it is. Left alone: the room's open sides, where the filler lies
// lower than the floor (not asked for).
// For the 3D view alone: the own ground is one mesh for both views, so the dressed faces lie on copies of the wall's
// texture (the same name, textures of their own) and Done::fromAbove tells what each shows in the view from above -
// the makers' black as it was, or nothing where they left no face (through such a side the turned camera looks into
// the room: a face there, even a black one, would shut it).
namespace uagfx::roomblocks {

constexpr float kBlockRise = 5.0f;  // a filler cell this much over the room's floor beside it is a block
constexpr int kReach = 3;           // cells from a room's floor a block is dressed within
constexpr int kMostRun = 8;         // a picture runs over this many cells at most
constexpr float kCapShare = 0.10f;  // of the picture from its top edge: what lies on a block's top

constexpr int kAsItIs = -1, kNotDrawn = -2;
struct Done {
    size_t tops = 0, sides = 0;  // the makers' black faces given the picture
    size_t made = 0;             // sides made where they left none
    size_t rooms = 0;            // rooms with a block dressed
    // by the ground's textures after the dressing, what a texture's faces show in the view from above: kAsItIs (the
    // ground's own textures), the filler's texture the dressed faces had, kNotDrawn for the faces made
    std::vector<int> fromAbove;
};
// A room with no pictured wall of its own takes the map's commonest; a map with none is left as it is.
Done dress(gnd::Ground& g, const rooms::Map& rooms);

}  // namespace uagfx::roomblocks
