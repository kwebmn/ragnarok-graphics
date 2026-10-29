#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uagfx::walkgrid {

// 3.0.11-beta.8 (the owner: «сделай по максимум»; the concept's routes need what one can walk): the server's maps
// cell by cell (tools/walk_grids.py from its .gat files, uaro-graphics\walkable.bin) - which cells are walked on and
// which parts of a map are reachable on foot from each other. A map's warp is a way only from the part it stands in:
// the Geffen tower's floors, que_ng's two guilds (amatsu's ninjas, einbroch's gunslingers - not one room) and the
// like are parts of one map.

constexpr uint8_t kWall = 255;  // not walked on (or outside)

struct Grid {
    int w = 0, h = 0;
    std::vector<uint32_t> rows;                      // h + 1 offsets into runs
    std::vector<std::pair<uint16_t, uint8_t>> runs;  // a run's length and its part (kWall)
    int partCount = 0;                               // the highest part + 1

    uint8_t at(int x, int y) const;  // the cell's part, kWall outside the map
    // The parts within the nearest square ring around (x, y) holding a walkable cell, up to `most` cells away (none:
    // nothing walkable that near)
    std::vector<uint8_t> around(int x, int y, int most) const;
};
using Grids = std::unordered_map<std::string, Grid>;

// The file's bytes (gzip of the payload walk_grids.py describes, or the payload) into the maps; false when it is
// not one
bool read(const std::string& file, Grids& out);
// A map labelled from its cells (walkable or not, row by row from y 0): its parts side by side (4-connected) - the
// tool's rule, for the tests and for a map the file lacks
Grid fromCells(int w, int h, const std::vector<uint8_t>& walkable);
// The payload of maps (gzip is the tool's): the tests' way to make the file
std::string payload(const Grids& maps);

}  // namespace uagfx::walkgrid
