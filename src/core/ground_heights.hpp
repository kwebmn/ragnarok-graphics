#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/map_axes.hpp"

namespace uagfx::ground_heights {

// 2.6.18: a map's ground as its .gnd shapes it - where its walls stand between a fire and a character (R-QPYCS9: "a
// fire behind the wall, its shadow goes through the wall"). Each cell's four corner heights (up along -y), `zoom`
// units a cell, the map's middle at 0 - the .rsw's places: cell (x, z) = (x / zoom + width / 2, z / zoom + height /
// 2), as the maps' torches stand by their walls.
struct Ground {
    int width = 0, height = 0;
    float zoom = 10.0f;
    std::vector<float> corners;  // 4 a cell, row by row: (x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)
    std::vector<uint8_t> laid;   // 2.7.57: 1 a cell in a patch laid by hand (laidPatches), row by row
};

// 2.7.57 (R-8YJSCR: Prontera's mats torn at their edges, «в версии первоначальной там они прямоугольные»; R-NSHM8K:
// straight lines across its paving - squares of 4 x 4 of eight textures): the cells of the patches a map's makers
// laid by hand - one texture filling a whole rectangle of cells (a square of paving, a mat, a bed), and every cell of
// a texture at least half of whose cells lie so - where the ground's seams (core/seams.hpp) keep the game's straight
// edge. texture: each cell's top's texture, row by row, -1 none; the fields' grass and roads lie irregular.
std::vector<uint8_t> laidPatches(int width, int height, const std::vector<int>& texture);
// Whether the cell at the map file's (x, z) is laid by hand; false off the map or none known.
bool laidAt(const Ground& g, float x, float z);

// A .gnd (1.7: textures, lightmaps, surfaces, then 28 bytes a cell) read to its cells' heights; false, with why.
bool parse(const uint8_t* data, size_t size, Ground& out, std::string& error);
// The ground's height at the map file's (x, z) - its cell's top across its corners; false off the map.
bool heightAt(const Ground& g, float x, float z, float& y);
// Whether light from `from` reaches `to` (the file's places): nowhere between them does the ground stand more than
// `slack` above the straight line - but within `skipFrom` of `from` along the ground (a torch hangs on its wall) and
// `skipTo` of `to`; off the map, and with no ground known, nothing stands in the way.
bool clear(const Ground& g, const map::Point& from, const map::Point& to, float skipFrom, float skipTo, float slack);

}  // namespace uagfx::ground_heights
