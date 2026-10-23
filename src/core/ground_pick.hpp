#pragma once
#include <utility>

namespace uagfx::pick {

// 0.89: the cell of the ground under the mouse, as uaRO.exe finds it (0x564A20) but walking the ray across the
// ground cell by cell in the order it crosses them, and stopping at the first cell it hits - the nearest hit, since
// a cell's triangles lie within its own square and the squares the ray crosses later lie farther along it. The game
// tests every cell of every piece of the map the ray passes over: thousands when the mouse points far.
//
// ray: origin x, y, z then direction x, y, z (the game's world; y is height, not used here); w, h: the ground's cells
// across x and z; size: a cell's side in world units; the map is centred as the game has it - cell x spans
// (x - w / 2) * size to one size further (an integer half), cell z likewise with h. test(x, z) returns the distance
// along the ray to the cell's ground, or none (no hit). True and the cell when one is hit. 0.90.1: hits nearer than
// minDistance are not counted (the game tests only the map's pieces that reach in front of the eye: 0x4ED660 drops a
// box whose far side is behind it - a hit behind the eye is not the mouse's).
constexpr float kNone = 999999.0f;  // uaRO.exe's own "no hit" (0x6958BC, 0x69F380)

template <typename Test>
bool walk(const float ray[6], int w, int h, float size, Test&& test, int& cellX, int& cellZ, float& distance,
          float minDistance = -1e30f) {
    if (w <= 0 || h <= 0 || !(size > 0.0f)) return false;
    // In cell units: the map's square is [0, w) x [0, h).
    const double ox = double(ray[0]) / size + double(w / 2), oz = double(ray[2]) / size + double(h / 2);
    const double dx = double(ray[3]) / size, dz = double(ray[5]) / size;
    const double tiny = 1e-12;
    const auto tryCell = [&](int x, int z) {
        if (x < 0 || z < 0 || x >= w || z >= h) return false;
        const float t = test(x, z);
        if (!(t < kNone) || t < minDistance) return false;
        cellX = x;
        cellZ = z;
        distance = t;
        return true;
    };
    if (dx * dx + dz * dz < tiny) {  // straight down (or up): the one column under the origin
        const double fx = ox, fz = oz;
        if (fx < 0 || fz < 0 || fx >= w || fz >= h) return false;
        return tryCell(int(fx), int(fz));
    }
    // Where the ray's line is over the map's square: the slabs of x and z.
    double t0 = -1e300, t1 = 1e300;
    const auto slab = [&](double o, double d, double hi) {
        if (d > -tiny && d < tiny) return o >= 0.0 && o < hi;
        double a = (0.0 - o) / d, b = (hi - o) / d;
        if (a > b) std::swap(a, b);
        if (a > t0) t0 = a;
        if (b < t1) t1 = b;
        return t0 <= t1;
    };
    if (!slab(ox, dx, w) || !slab(oz, dz, h)) return false;
    // Not before the nearest that counts: the cells there are crossed before it. Units: the ray's direction is per
    // world unit along it, the cell units per world unit across - the same parameter t.
    if (double(minDistance) > t0) t0 = double(minDistance);
    if (t0 > t1) return false;
    // Into the square a hair past its edge, then cell by cell.
    const double start = t0 + 1e-9 * (1.0 + (t0 < 0 ? -t0 : t0));
    double px = ox + dx * start, pz = oz + dz * start;
    int cx = int(px < 0 ? 0 : px >= w ? w - 1 : px), cz = int(pz < 0 ? 0 : pz >= h ? h - 1 : pz);
    const int stepX = dx > 0 ? 1 : dx < 0 ? -1 : 0, stepZ = dz > 0 ? 1 : dz < 0 ? -1 : 0;
    const double inf = 1e300;
    double nextX = stepX ? ((stepX > 0 ? cx + 1 : cx) - ox) / dx : inf;
    double nextZ = stepZ ? ((stepZ > 0 ? cz + 1 : cz) - oz) / dz : inf;
    const double deltaX = stepX ? 1.0 / (dx < 0 ? -dx : dx) : inf, deltaZ = stepZ ? 1.0 / (dz < 0 ? -dz : dz) : inf;
    for (int steps = 0; steps <= w + h + 2; ++steps) {
        if (tryCell(cx, cz)) return true;
        if (nextX < nextZ) {
            if (nextX > t1) break;
            cx += stepX;
            nextX += deltaX;
        } else {
            if (nextZ > t1) break;
            cz += stepZ;
            nextZ += deltaZ;
        }
        if (cx < 0 || cz < 0 || cx >= w || cz >= h) break;
    }
    return false;
}

// The same distance, give or take the float's rounding: two cells sharing the edge the ray meets the ground at.
inline bool sameDistance(float a, float b) {
    const float d = a > b ? a - b : b - a, m = (a < 0 ? -a : a) > 1.0f ? (a < 0 ? -a : a) : 1.0f;
    return d <= 1e-3f * m;
}

// 0.89: whether the walk is used on the map now: once it has agreed with the game's own answer kAgree times running
// on it, and then checked again every kEvery picks. 0.90.1: a disagreement checks the map anew (a ray grazing the
// eye's own cell may go either way); the kOff-th and the game's own pick is used from then on, on every map. 0.90.2:
// in a map's first kSettle picks the game's answer is none while it is still setting the map up. 3.0.2-beta.13: those
// picks ask neither (none, as the game's own then), and kAgree 30 in place of 240 - the game's own takes 7.6 ms on a
// 200x200 map, and a map's 240 checks made the second half of its loading (beta.12's measure: 1.8 s of 3.7).
class Trust {
public:
    static constexpr int kAgree = 30, kEvery = 120, kOff = 3, kSettle = 60;
    // The map now (its ground's address, cells and size): another map's is checked anew (true).
    bool on(unsigned long map) {
        if (map == map_) return false;
        map_ = map;
        agreed_ = 0;
        onMap_ = 0;
        return true;
    }
    bool trusted() const { return !off_ && agreed_ >= kAgree; }
    bool off() const { return off_; }
    // 3.0.2-beta.13: one of the map's first kSettle picks - no cell, neither asked
    bool settling() {
        if (onMap_ >= kSettle) return false;
        ++onMap_;
        return true;
    }
    // Whether this pick is checked against the game's own.
    bool check() { return !trusted() || ++picks_ % kEvery == 0; }
    // A checked pick: whether both answers were the same. True when it is the one that makes the walk trusted.
    bool result(bool same) {
        if (!same) {
            agreed_ = 0;
            off_ = off_ || ++disagreed_ >= kOff;
            return false;
        }
        if (off_ || agreed_ >= kAgree) return false;
        return ++agreed_ == kAgree;
    }

private:
    unsigned long map_ = 0;
    int agreed_ = 0;
    unsigned picks_ = 0;
    int disagreed_ = 0, onMap_ = 0;
    bool off_ = false;
};

}  // namespace uagfx::pick
