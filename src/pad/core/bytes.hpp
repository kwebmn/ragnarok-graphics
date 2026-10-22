#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

namespace uapad {

using Bytes = std::vector<uint8_t>;

inline uint8_t rd8(const uint8_t* p, size_t o) { return p[o]; }
inline uint16_t rd16(const uint8_t* p, size_t o) { return uint16_t(p[o] | (p[o + 1] << 8)); }
inline uint32_t rd32(const uint8_t* p, size_t o) {
    return uint32_t(p[o]) | (uint32_t(p[o + 1]) << 8) | (uint32_t(p[o + 2]) << 16) | (uint32_t(p[o + 3]) << 24);
}
inline void wr8(Bytes& b, size_t o, uint8_t v) { b[o] = v; }
inline void wr16(Bytes& b, size_t o, uint16_t v) {
    b[o] = uint8_t(v);
    b[o + 1] = uint8_t(v >> 8);
}
inline void wr32(Bytes& b, size_t o, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[o + i] = uint8_t(v >> (8 * i));
}

struct Cell {
    int x = 0;
    int y = 0;
};
inline bool operator==(const Cell& a, const Cell& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Cell& a, const Cell& b) { return !(a == b); }
inline std::string cellText(const Cell& c) { return "(" + std::to_string(c.x) + "," + std::to_string(c.y) + ")"; }

// Chebyshev distance: how uAthena measures ranges (battle_check_range, check_distance).
inline int cellDistance(const Cell& a, const Cell& b) {
    const int dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const int dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx > dy ? dx : dy;
}

// The cell `steps` steps from `from` in the direction of the vector `dir` (not zero).
inline Cell stepsToward(const Cell& from, const Cell& dir, int steps) {
    const double longest = double(std::max(std::abs(dir.x), std::abs(dir.y)));
    return Cell{from.x + int(std::lround(steps * dir.x / longest)), from.y + int(std::lround(steps * dir.y / longest))};
}

// 3-byte packed position: WBUFPOS in uAthena src/map/clif.c.
inline Cell decodePos3(const uint8_t* p, size_t o) {
    const uint8_t* q = p + o;
    return Cell{q[0] * 4 + (q[1] >> 6), ((q[1] & 0x3f) << 4) + (q[2] >> 4)};
}
inline void encodePos3(Bytes& b, size_t o, Cell c, int dir) {
    b[o] = uint8_t(c.x >> 2);
    b[o + 1] = uint8_t((c.x << 6) | ((c.y >> 4) & 0x3f));
    b[o + 2] = uint8_t((c.y << 4) | (dir & 0x0f));
}

// 6-byte packed movement: WBUFPOS2 in uAthena src/map/clif.c.
struct Move {
    Cell from;
    Cell to;
};
inline Move decodePos6(const uint8_t* p, size_t o) {
    const uint8_t* q = p + o;
    Move m;
    m.from = Cell{q[0] * 4 + (q[1] >> 6), ((q[1] & 0x3f) << 4) + (q[2] >> 4)};
    m.to = Cell{((q[2] & 0x0f) << 6) + (q[3] >> 2), ((q[3] & 0x03) << 8) + q[4]};
    return m;
}
inline void encodePos6(Bytes& b, size_t o, Cell from, Cell to) {
    b[o] = uint8_t(from.x >> 2);
    b[o + 1] = uint8_t((from.x << 6) | ((from.y >> 4) & 0x3f));
    b[o + 2] = uint8_t((from.y << 4) | ((to.x >> 6) & 0x0f));
    b[o + 3] = uint8_t((to.x << 2) | ((to.y >> 8) & 0x03));
    b[o + 4] = uint8_t(to.y);
    b[o + 5] = 0x88;  // sub-cell offsets 8/8, as clif_walkok / clif_move send them
}

}  // namespace uapad
