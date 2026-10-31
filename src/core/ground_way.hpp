#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/game_camera.hpp"
#include "core/own_ground.hpp"

namespace uagfx::groundway {

// 3.0.11-beta.31 (the owner: «на самой локации нету линии по которой идти надо, только на карте»): the way followed
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

// 3.0.11-beta.33 (the owner: «полоска с маршрутом может лучше бы была выполнена такими типа следами типа шажков, как
// сделано в новых версиях ро клиента»): the way as footprints - left and right in turn a stride apart, each to its
// side of the way, its toes along it
struct Print {
    float x = 0, z = 0;    // its middle, the game's world
    float fx = 1, fz = 0;  // its toes' way along the ground (a unit)
    bool left = false;     // on the way's left (x east, z north)
    float at = 0;          // how far along the way (world units)
    float alpha = 1;       // 3.0.11-beta.34: its colour's alpha times this (faded in and out)
};
// The prints along the way's points (as along() gives them): `stride` world units apart, `apart` to their sides.
// 3.0.11-beta.34 (the owner: «каждый раз перестраивается линия, можно так чтобы она не очень дергалась»): counted back
// from the way's end - the way kept as the player walks it and cut at them, each print stays where it is on the ground
// (they were counted from the player's feet: all of them moved at each step)
std::vector<Print> prints(const std::vector<Point>& points, float stride, float apart);
// The way's points from `at` along it on (the first one put there; the ground's distances, as prints() counts them)
std::vector<Point> cut(const std::vector<Point>& points, float at);
// The prints the player at `at` along the way sees: from `skip` ahead of them (under their feet none) to `most` ahead,
// each faded in over `fade` at both ends (a print they come to fades out under them, a far one comes in softly)
std::vector<Print> ahead(const std::vector<Print>& prints, float at, float skip, float most, float fade);
// A colour's alpha times k (0..1)
uint32_t faded(uint32_t argb, float k);
// A print's outline: an oval sole ahead, then an oval heel behind (kSoleSides and kHeelSides points round), `grow`
// world units bigger all round (its dark edge); world x and z (y 0)
constexpr int kSoleSides = 10, kHeelSides = 8;
std::vector<Point> printShape(const Print& print, float grow);
// The prints as triangles on the ground - each oval a fan round its middle, every corner on the map's heights (`g`,
// `cellSize`, `sign`, `lift` as along()'s) placed on the screen as strip() places its own, `argb` faded by its alpha;
// a print off the map, faded out or with a corner behind the near plane left out
size_t footprints(const std::vector<Print>& prints, const Heights& g, int cellSize, float sign, float lift, float grow,
                  const float view[12], const camera::Projection& p, uint32_t argb, float bias,
                  std::vector<Vertex>& out);

// 3.0.11-beta.87 (the concept «Контроллер в интерфейсе uaRO», stage 3): a ring `width` wide round (x, y, z) - its
// middle `radius` world units out, `sides` pieces (dashed: every other one) - placed as strip() places its corners
size_t ring(float x, float y, float z, float radius, float width, int sides, bool dashed, const float view[12],
            const camera::Projection& p, uint32_t argb, float bias, std::vector<Vertex>& out);
// Four notches out of such a ring (a friend's): `width` wide, 0.9 world units long from its outer edge
size_t notches(float x, float y, float z, float radius, float width, const float view[12], const camera::Projection& p,
               uint32_t argb, float bias, std::vector<Vertex>& out);
// 3.0.11-beta.88 (stage 4): an arrow on the ground at height y from (x0, z0) to (x1, z1) - a shaft `width` wide, a head
// `head` long and `headWidth` wide at its end (all of it a head when shorter) - placed as strip() places its corners
size_t arrow(float x0, float z0, float x1, float z1, float y, float width, float head, float headWidth,
             const float view[12], const camera::Projection& p, uint32_t argb, float bias, std::vector<Vertex>& out);
// A chevron pointing down at (cx, tipY) in the screen's pixels, `w` wide and `h` high - no depth, over the world
size_t chevron(float cx, float tipY, float w, float h, uint32_t argb, std::vector<Vertex>& out);

}  // namespace uagfx::groundway
