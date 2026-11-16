#pragma once
#include <cstdint>
#include <vector>

#include "core/gnd.hpp"

namespace uagfx::rooms {

// 3.0.20-beta.57 (R-J3GKD9, the owner in payon_in01's 3D view: «тут другие комнаты видны»): a house's map keeps its
// rooms side by side - payon_in01 twelve, prt_in twenty-eight, morocc_in twenty-one - each its own patch of floor,
// no ground (or the makers' black filler) between them; from above one room fills the screen, from the eyes' level
// the others stand lit in the dark behind the room's open sides. A map's rooms: its cells with a top of no filler
// texture (outskirts::fillerTexture), joined side to side; under kLeastCells cells: none (a doorstep, a closet's
// corner). Numbered from 1, the largest first, kMost at most.
//  room: a cell's room (0: none); widened: the same widened kWiden cells - a wall's model stands on the cells beside
//  its room's floor; floor: a room's floor's height by its number (up positive: the middle one of its cells')
struct Map {
    int width = 0, height = 0;
    float zoom = 10.0f;
    int count = 0;
    std::vector<uint8_t> room, widened;
    std::vector<float> floor;
};
constexpr int kWiden = 2, kLeastCells = 12, kMost = 254;
Map build(const gnd::Ground& g);
// the room at a place of the map's file (its middle at 0, 0), a room's widened cells its own; 0: none
int roomAt(const Map& m, float x, float z);

}  // namespace uagfx::rooms
