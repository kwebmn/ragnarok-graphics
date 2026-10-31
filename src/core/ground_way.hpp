#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"
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
// 3.0.11-beta.102 (the owner: «чтобы стрелочка показывалась как в концепции»): the stick's walking arrow as the
// concept draws it, over the map's heights (`g`, `cellSize`, `sign`, `lift` as along()'s) - from near the character's
// feet (x0, z0: the game's world) to the near edge of the cell (toX, toY): a soft white shaft in half-cell pieces, a
// white head over a gold edge, the cell's square faintly filled and outlined in gold; nothing when the character
// stands on that cell or a corner is off the map
// 3.0.11-beta.103: which part of an arrow a triangle draws (the shade under all the rest)
constexpr uint8_t kPartShaft = 0, kPartEdge = 1, kPartHead = 2, kPartCellFill = 3, kPartCellLine = 4, kPartShade = 5,
                  kPartChevron = 6, kPartMarker = 7, kPartRing = 8, kPartPointer = 9, kPartDot = 10, kPartWall = 11;
struct Tri {
    Point a, b, c;
    uint32_t argb = 0;
    uint8_t part = kPartShaft;
};
constexpr uint32_t kWalkShaft = 0x66FFFFFFu, kWalkEdge = 0xE6E2C27Au, kWalkHead = 0xE6FFFFFFu;
constexpr uint32_t kWalkCellFill = 0x24FFFFFFu, kWalkCellLine = 0xD9E2C27Au;
std::vector<Tri> walkArrowShape(float x0, float z0, int toX, int toY, const Heights& g, int cellSize, float sign,
                                float lift);
// 3.0.11-beta.103 (the owner: «можешь сделать настройку, чтобы можно было выбрать эту стрелку в конфигураторе»): the
// stick's arrow in each look of the doc «Стрелка хода с контроллера - варианты» ([interface] walk_arrow) over the map's
// heights, drawn along `way` `len` on (as glide() gives them) to a cell-sized end: chevrons running on to a gold
// diamond (B), a slim tapered arrow to a cell's gold corners (A), beta.102's concept arrow, a ring round the feet
// with a gold pointer and a small gold ring at the end (C), dots to a gold ring (D), a cell's gold frame with a wedge
// at the feet (E); a wall cutting the way short - a reddish bar across it past the end. Each look over its dark shade
// (the same shapes a tenth of a cell bigger, black at 3/4 of their alpha), every colour times `alpha`; nothing when
// the end is under the feet (a wall's bar alone) or a corner is off the map
struct WalkArrow {
    float x0 = 0, z0 = 0;      // the character's feet (the game's world)
    float wayX = 1, wayZ = 0;  // the way drawn along (x east, z north)
    float len = 0;             // 3.0.11-beta.104: the end's middle along it (world units; it was the asked cell)
    bool blocked = false;      // a wall right past it
    float phase = 0;           // the chevrons' run, 0..1
    float alpha = 1;           // faded in and out
    bool mark = true;          // 3.0.11-beta.105: the end's mark (the diamond, the corners, the frame, the small ring)
};
std::vector<Tri> walkShape(WalkLook look, const WalkArrow& arrow, const Heights& g, int cellSize, float sign,
                           float lift);

// 3.0.11-beta.104 (the owner: «стрелочки очень дергаются при движении»): the arrow gliding with the character. The
// cell asked for is the game's cell + the lead along the way, worked out again at every frame: it hopped a cell on
// each time the character's cell changed (6-7 times a second at 150 ms a cell) while the feet glided up to it - the
// arrow's end and its cell shook by up to a cell in a frame (tests/test_walk_glide's model of the real asks: 0.9-1.3
// cells, 0.19 a frame on average against the feet, where none is right). Now the arrow lies along the way the
// character walks (the march's: the stick's own or a slide's along a wall) eased round in 80 ms, as long as the lead
// while the way is open, up to the asked cell along it when a wall cuts the way short (that cell stays put as the
// character comes up: its end kept where it is on the ground as the feet go on, so the easing does not lag into the
// wall), its length eased in 100 ms; the first frame of an arrow takes both at once, as a warp does (the feet a lead
// away from a frame ago)
struct WalkAim {
    float x0 = 0, z0 = 0;        // the feet
    float wayX = 1, wayZ = 0;    // the march's way
    float cellX = 0, cellZ = 0;  // the asked cell's middle
    bool cut = false;            // a wall short of the lead: up to that cell
    float lead = 0;              // the lead's cells in world units
};
struct WalkGlide {
    bool has = false;
    float angle = 0;  // radians from east (x) toward north (z)
    float len = 0;    // world units along it
    float x0 = 0, z0 = 0;  // the feet a frame ago
};
void glide(WalkGlide& g, const WalkAim& aim, float dtMs);
// The triangles on the screen as strip() places its corners; one with a corner behind the near plane left out
size_t placeTris(const std::vector<Tri>& tris, const float view[12], const camera::Projection& p, float bias,
                 std::vector<Vertex>& out);
// A chevron pointing down at (cx, tipY) in the screen's pixels, `w` wide and `h` high - no depth, over the world
size_t chevron(float cx, float tipY, float w, float h, uint32_t argb, std::vector<Vertex>& out);

}  // namespace uagfx::groundway
