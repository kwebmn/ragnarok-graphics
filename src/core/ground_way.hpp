#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/game_camera.hpp"
#include "core/own_ground.hpp"

namespace uagfx::groundway {

// 3.0.11-beta.32 (the owner: «на самой локации нету линии по которой идти надо, только на карте»): the way followed
// drawn on the ground itself - its cells (core/walk_path, the minimap's) put into the game's world by the minimap's
// formula backwards and the map's .gat heights, a strip of quads a little over the ground, each corner placed on the
// screen as the game places its own vertices (ProjectVertex: own_ground::placeView) with the frame's camera; drawn
// against the depth with the world, before the interface. No Windows here.

// A map's .gat: "GRAT", two version bytes, its width and height (int32), then 20 bytes a cell from y 0 (south) - its
// corners' heights (south-west, south-east, north-west, north-east; the game's world y, up along -y) and its type
struct Heights {
    int w = 0, h = 0;
    std::vector<float> corners;  // 4 a cell
    // The height at a place of the map in cells (x east, y north; a cell's middle at +0.5): between its cell's
    // corners; false off the map
    bool at(float x, float y, float& out) const;
};
bool parseGat(const std::string& bytes, Heights& out);

// A map's cell's middle in the game's world (minimap::cellOf backwards: the width's and height's halves cut as the
// game cuts them)
inline void cellToWorld(int x, int y, int width, int height, int cellSize, float& wx, float& wz) {
    wx = (float(x) + 0.5f - float(width / 2)) * float(cellSize);
    wz = (float(y) + 0.5f - float(height / 2)) * float(cellSize);
}

struct Point {
    float x = 0, y = 0, z = 0;  // the game's world
};
// The way's cells as world points along the ground - each leg cut into steps of at most a cell (the heights followed
// up and down), `lift` world units over it (up: -y); `sign` -1 when the .gat's heights run the other way to the
// game's (the player's own height tells: win/ground_way)
std::vector<Point> along(const std::vector<std::pair<int, int>>& cells, const Heights& g, int cellSize, float lift,
                         float sign);

// The game's one-texture vertex (FVF_TL1, untextured here)
using Vertex = own_ground::Tl1;
// A strip `width` world units wide along the points, as triangles (6 vertices a step, the strip's sides joined at its
// bends), each corner where the game would put it on the screen through `view` (world -> view, as readGameCamera
// gives it) and `p`, each corner `bias` world units nearer along its view ray (its screen place kept: the strip lies on
// the ground and is not to sink under the ground's own bumps, the same at any distance); a step with a corner behind
// the near plane left out. `argb` its colour with its alpha
size_t strip(const std::vector<Point>& points, const float view[12], const camera::Projection& p, float width,
             uint32_t argb, float bias, std::vector<Vertex>& out);

}  // namespace uagfx::groundway
