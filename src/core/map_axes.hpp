#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::map {

// 0.25: a map file's places (core/rsw.hpp) and the game's world (its camera, core/game_camera.hpp) share the middle of
// the map, ten units a tile and up along -y, but the axes along the ground may be turned around: x and z are each
// the file's or its opposite. Found from the torches - where the file puts them and where they are seen.
struct Point {
    float x = 0, y = 0, z = 0;
};

struct Axes {
    int8_t x = 0, y = 1, z = 0;  // the signs; x 0: not known
    bool known() const { return x != 0 && z != 0; }
};

inline Point toWorld(const Axes& a, const Point& p) { return Point{a.x * p.x, a.y * p.y, a.z * p.z}; }

// The signs that put `placed` where `seen` are - the median of each seen point's distance along the ground to the
// nearest placed one the least by far - or unknown: too few, a layout that fits two ways, or nothing near.
// spread: that median.
Axes fitAxes(const std::vector<Point>& placed, const std::vector<Point>& seen, float* spread = nullptr);

}  // namespace uagfx::map
