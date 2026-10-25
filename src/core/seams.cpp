#include "core/seams.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <unordered_map>

#include "core/round.hpp"

namespace uagfx::seams {
namespace {

// A cell: its four corners - 0 (x0, z0), 1 (x1, z0), 2 (x0, z1), 3 (x1, z1) - and its group.
struct Cell {
    Vertex corner[4];
    bool have[4] = {false, false, false, false};
    uint32_t group = 0;
    bool across03 = false;  // 2.1.2: the game's two triangles meet along corners 0-3 (else 1-2)
};

// 2.1.2: the place on the cell's ground at (u, v) - on the game's own two triangles, not between the four corners: on a
// slope the two differ by tenths of a unit, the ground hid the strip there now and then and it blinked as the camera
// moved.
void onGround(const Cell& c, float u, float v, float out[3]) {
    const auto p = [&](int k, int a) { return c.corner[k].pos[a]; };
    for (int a = 0; a < 3; ++a) {
        if (!c.across03) {
            out[a] = u + v <= 1.0f ? p(0, a) + u * (p(1, a) - p(0, a)) + v * (p(2, a) - p(0, a))
                                   : p(3, a) + (1.0f - u) * (p(2, a) - p(3, a)) + (1.0f - v) * (p(1, a) - p(3, a));
        } else {
            out[a] = u >= v ? p(0, a) + u * (p(1, a) - p(0, a)) + v * (p(3, a) - p(1, a))
                            : p(0, a) + v * (p(2, a) - p(0, a)) + u * (p(3, a) - p(2, a));
        }
    }
}

int64_t keyOf(int x, int z) { return (int64_t(x) << 32) ^ int64_t(uint32_t(z)); }
int xOf(int64_t key) { return int(key >> 32); }
int zOf(int64_t key) { return int(int32_t(uint32_t(key))); }

// 2.7.9: two cells in order - by x, then by z, both signed. The keys' order put z -1 after 0 (their low half is z
// unsigned), and the grid's origin, taken from the first triangle's corner, falls either side of a cell's line by the
// rounding of the ground back from the screen - as the camera turned, every cell's place moved by one now and then,
// and the row of edges at z -1/0 took the other side's noise: the line between grass and road jumped there, at the
// middle of a map (prt_fild08 200 200: the neighbours' strips blinked). Moved all by one, this order stays.
bool before(int64_t a, int64_t b) { return xOf(a) < xOf(b) || (xOf(a) == xOf(b) && zOf(a) < zOf(b)); }

// The cell's attributes at (u, v) of its own square, bilinear (outside [0, 1] carried on), colours rounded channel by
// channel at each of the three steps. 2.5.11: only the parts a strip's corner keeps, each by the same arithmetic as
// before - a corner took two whole vertices of it and four of the neighbours' look-ups: 8.7 ms a frame.
// The channels at bit shifts from..to, the others 0.
uint32_t lerpChannels(uint32_t a, uint32_t b, float t, int from, int to) {
    uint32_t out = 0;
    for (int s = from; s <= to; s += 8) {
        const float ca = float((a >> s) & 255), cb = float((b >> s) & 255);
        out |= uint32_t(std::clamp(ca + (cb - ca) * t + 0.5f, 0.0f, 255.0f)) << s;
    }
    return out;
}

float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// at(c, u, v).uv0
void uv0At(const Cell& c, float u, float v, float out[2]) {
    for (int k = 0; k < 2; ++k) {
        const float top = lerpf(c.corner[0].uv0[k], c.corner[1].uv0[k], u);
        const float bottom = lerpf(c.corner[2].uv0[k], c.corner[3].uv0[k], u);
        out[k] = lerpf(top, bottom, v);
    }
}

// at(c, u, v)'s second coordinates, its diffuse colour without the alpha and its specular's alpha alone
void keptAt(const Cell& c, float u, float v, Vertex& p) {
    for (int k = 0; k < 2; ++k) {
        const float top = lerpf(c.corner[0].uv1[k], c.corner[1].uv1[k], u);
        const float bottom = lerpf(c.corner[2].uv1[k], c.corner[3].uv1[k], u);
        p.uv1[k] = lerpf(top, bottom, v);
    }
    const auto colour = [&](uint32_t Vertex::*field, int from, int to) {
        const uint32_t top = lerpChannels(c.corner[0].*field, c.corner[1].*field, u, from, to);
        const uint32_t bottom = lerpChannels(c.corner[2].*field, c.corner[3].*field, u, from, to);
        return lerpChannels(top, bottom, v, from, to);
    };
    p.diffuse = colour(&Vertex::diffuse, 0, 16);
    p.specular = colour(&Vertex::specular, 24, 24);
}

// 3.0.5-beta.9: keptAt's specular alone - the fog, made anew for a strip taken again (Cache)
uint32_t fogAt(const Cell& c, float u, float v) {
    const uint32_t top = lerpChannels(c.corner[0].specular, c.corner[1].specular, u, 24, 24);
    const uint32_t bottom = lerpChannels(c.corner[2].specular, c.corner[3].specular, u, 24, 24);
    return lerpChannels(top, bottom, v, 24, 24);
}

// 2.7.57: a texture repeats - a tile ending at 1 and the next starting at 0 is the same place on it (3.0.5-beta.9:
// roundAwayF, std::round's answer without the library's call)
bool same(float a, float b) {
    const float d = a - b;
    return std::fabs(d - roundAwayF(d)) < 0.02f;
}

// 2.7.8: the point a share t of the way from a to b - its place on the cell's ground at (u, v), the rest as the
// rasterizer had it there before the triangle was cut
Vertex between(const Cell& c, const Vertex& a, const Vertex& b, float t, float u, float v) {
    Vertex p;
    onGround(c, u, v, p.pos);
    p.diffuse = lerpChannels(a.diffuse, b.diffuse, t, 0, 24);
    p.specular = lerpChannels(a.specular, b.specular, t, 0, 24);
    for (int k = 0; k < 2; ++k) {
        p.uv0[k] = lerpf(a.uv0[k], b.uv0[k], t);
        p.uv1[k] = lerpf(a.uv1[k], b.uv1[k], t);
    }
    return p;
}

// 1.9.1: a place of the neighbour's square past its edge folded back into it - its texture mirrored across the edge.
// Carried straight on, the coordinates left the cell's part of the texture: many grounds take a quarter of an atlas
// of four, and the strip showed the next quarter's cracks and stones as dark blocks.
float fold(float x) { return x < 0.0f ? -x : x > 1.0f ? 2.0f - x : x; }

// 1.9.2: a place of a cell's square kept off its outermost kBand - mirrored back from it.
constexpr float kBand = 0.03f;  // 2.0.2: the rim only - wider, its mirror's axis showed as a light line
float inset(float x) { return x < kBand ? 2.0f * kBand - x : x > 1.0f - kBand ? 2.0f * (1.0f - kBand) - x : x; }

}  // namespace

float cellSide(const std::vector<Triangle>& ground) {
    std::vector<float> sides;
    for (const Triangle& t : ground) {
        if (std::fabs(t.v[0].pos[1] - t.v[1].pos[1]) > 4.0f || std::fabs(t.v[0].pos[1] - t.v[2].pos[1]) > 4.0f)
            continue;  // a wall
        float x0 = t.v[0].pos[0], x1 = x0, z0 = t.v[0].pos[2], z1 = z0;
        for (const Vertex& v : t.v) {
            x0 = std::min(x0, v.pos[0]);
            x1 = std::max(x1, v.pos[0]);
            z0 = std::min(z0, v.pos[2]);
            z1 = std::max(z1, v.pos[2]);
        }
        sides.push_back(std::max(x1 - x0, z1 - z0));
        if (sides.size() >= 256) break;
    }
    if (sides.empty()) return 0.0f;
    std::nth_element(sides.begin(), sides.begin() + std::ptrdiff_t(sides.size() / 2), sides.end());
    return sides[sides.size() / 2];
}

namespace {

// 3.0.5-beta.9 (R-VMPQKX: the seams most of a Radeon 610M laptop's game thread on «Високі», the camera moving - a
// thousand allocations a frame for the cells' hash map among it): the cells by their places in one table over a
// vector, both kept from frame to frame - nothing allocated a cell
class CellMap {
public:
    void reset(size_t most) {  // at most `most` cells: the table at least twice as big, never fuller than half
        size_t capacity = 16;
        while (capacity < most * 2) capacity <<= 1;
        slots_.assign(capacity, 0);
        mask_ = capacity - 1;
        all_.clear();
        all_.reserve(most);
    }
    Cell& at(int64_t key) {
        size_t i = slotOf(key);
        for (; slots_[i]; i = (i + 1) & mask_)
            if (all_[slots_[i] - 1].first == key) return all_[slots_[i] - 1].second;
        all_.emplace_back(key, Cell{});
        slots_[i] = uint32_t(all_.size());
        return all_.back().second;
    }
    const Cell* find(int64_t key) const {
        for (size_t i = slotOf(key); slots_[i]; i = (i + 1) & mask_)
            if (all_[slots_[i] - 1].first == key) return &all_[slots_[i] - 1].second;
        return nullptr;
    }
    const std::vector<std::pair<int64_t, Cell>>& all() const { return all_; }

private:
    size_t slotOf(int64_t key) const { return size_t((uint64_t(key) * 0x9E3779B97F4A7C15ull) >> 40) & mask_; }
    std::vector<uint32_t> slots_;  // all_'s index + 1, 0 none
    std::vector<std::pair<int64_t, Cell>> all_;
    size_t mask_ = 0;
};

// The four neighbours: the direction in the grid, this cell's corners on the shared edge (in the order along it) and
// the neighbour's same corners, and (u, v) of a point at (along, into) the cell from the edge.
struct Way {
    int dx, dz;
    int mine[2], theirs[2];
};
const Way kWays[4] = {{1, 0, {1, 3}, {0, 2}}, {-1, 0, {0, 2}, {1, 3}}, {0, 1, {2, 3}, {0, 1}}, {0, -1, {0, 1}, {2, 3}}};
constexpr int kAlong = 4, kAcross = 2;
// a strip's worst: the grid's points and two more for each triangle cut at the crease
constexpr size_t kPieceMost = (kAlong + 1) * (kAcross + 1) + 4 * kAlong * kAcross;

// 3.0.5-beta.9: all an edge's two strips are made of but the cell's places and fog - its cell's own coordinates,
// colours and crease, the neighbour's coordinates, the groups and their textures, which cells about it are the
// neighbour's texture (claim), which side's noise counts up, the width. The same in another frame: its strips again,
// their places and fog from that frame's cell (Piece::refresh).
struct Inputs {
    float uv0[4][2], uv1[4][2], theirUv0[4][2];
    uint32_t diffuse[4];  // without the alpha: keptAt's channels
    uint32_t inside, shows, ta, tb;
    uint32_t nearby;  // 9 bits: the 3x3 cells about it (claim)
    uint32_t flags;   // 1 across03, 2 creased, 4 low
    float width;
};

Inputs inputsOf(const Cell& a, const Cell& b, uint32_t ta, uint32_t tb, uint32_t nearby, bool creased, bool low,
                float width) {
    Inputs in;
    std::memset(&in, 0, sizeof in);  // compared byte by byte
    for (int k = 0; k < 4; ++k) {
        for (int c = 0; c < 2; ++c) {
            in.uv0[k][c] = a.corner[k].uv0[c];
            in.uv1[k][c] = a.corner[k].uv1[c];
            in.theirUv0[k][c] = b.corner[k].uv0[c];
        }
        in.diffuse[k] = a.corner[k].diffuse & 0x00FFFFFFu;
    }
    in.inside = a.group;
    in.shows = b.group;
    in.ta = ta;
    in.tb = tb;
    in.nearby = nearby;
    in.flags = (a.across03 ? 1u : 0u) | (creased ? 2u : 0u) | (low ? 4u : 0u);
    in.width = width;
    return in;
}

// 3.0.5-beta.9: one of an edge's two strips - the neighbour's texture over the cell, or its own band at the edge: its
// vertices, the indices from its first, and how each vertex's place and fog come from the cell's (a point of the
// grid at (u, v); one where the crease cuts a triangle, between two of them)
struct Piece {
    uint32_t inside = 0, shows = 0;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    struct Origin {
        float u = 0, v = 0, t = 0;
        int16_t n = -1, m = -1;  // a cut's two points of the grid; -1: a point of the grid itself
        uint32_t flags = 0;      // the grid's point's specular without the fog (emit's own bits)
    };
    std::vector<Origin> origins;

    // the places and the fog from cell a as it is now - as makePiece makes them
    void refresh(const Cell& a) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            const Origin& o = origins[i];
            Vertex& v = vertices[i];
            onGround(a, o.u, o.v, v.pos);
            v.specular = o.n < 0 ? fogAt(a, o.u, o.v) | o.flags
                                 : lerpChannels(vertices[size_t(o.n)].specular, vertices[size_t(o.m)].specular, o.t,
                                                0, 24);
        }
    }
};

// The strip of an edge from cell a (at ix, iz) to its neighbour b the way w: the neighbour's texture by the noise
// (self false, stripWidth wide), or over the band at the edge its own texture taken from a little inside (self).
void makePiece(const Cell& a, const Cell& b, const Way& w, int ix, int iz, const Inputs& in, bool self,
               float stripWidth, Piece& s) {
    s.inside = a.group;
    s.shows = self ? a.group : b.group;
    s.vertices.clear();
    s.indices.clear();
    s.origins.clear();
    const bool low = (in.flags & 4u) != 0;
    // 1.9.5: how much a place belongs to the neighbour's texture - its cells (the neighbour alone where both have one
    // texture) as 1 at their middles, bilinear between the cells' middles: 0.5 at the edge, more at a corner the
    // neighbour's texture goes round - the line rounds the cells' corners off (shade(), bit 31). 2.5.11: the cells
    // about this one looked up once an edge (Inputs::nearby), not four times a corner of a strip; claim reads no
    // others (u and v within the cell)
    const auto theirsNear = [&](int cx, int cz) {
        const int i = std::clamp(cx - (ix - 1), 0, 2), j = std::clamp(cz - (iz - 1), 0, 2);
        return (in.nearby >> (j * 3 + i) & 1u) ? 1.0f : 0.0f;
    };
    const auto claim = [&](float u, float v) {
        const float gx = float(ix) + u - 0.5f, gz = float(iz) + v - 0.5f;
        const int x0 = floorInt(gx), z0 = floorInt(gz);
        const float fx = gx - float(x0), fz = gz - float(z0);
        const float top = theirsNear(x0, z0) + (theirsNear(x0 + 1, z0) - theirsNear(x0, z0)) * fx;
        const float bottom = theirsNear(x0, z0 + 1) + (theirsNear(x0 + 1, z0 + 1) - theirsNear(x0, z0 + 1)) * fx;
        return top + (bottom - top) * fz;
    };
    float placeU[(kAlong + 1) * (kAcross + 1)], placeV[(kAlong + 1) * (kAcross + 1)];
    for (int j = 0; j <= kAcross; ++j)
        for (int i = 0; i <= kAlong; ++i) {
            const float along = float(i) / kAlong, into = stripWidth * float(j) / kAcross;
            // (u, v) in this cell for the point `into` from the shared edge
            float u = 0, v = 0;
            if (w.dx == 1) { u = 1.0f - into; v = along; }
            if (w.dx == -1) { u = into; v = along; }
            if (w.dz == 1) { u = along; v = 1.0f - into; }
            if (w.dz == -1) { u = along; v = into; }
            placeU[j * (kAlong + 1) + i] = u;
            placeV[j * (kAlong + 1) + i] = v;
            Vertex p;
            keptAt(a, u, v, p);
            onGround(a, u, v, p.pos);  // 2.1.2: on the game's triangles
            if (self)
                uv0At(a, inset(u), inset(v), p.uv0);
            else
                uv0At(b, inset(fold(u - float(w.dx))), inset(fold(v - float(w.dz))), p.uv0);
            const uint32_t f = uint32_t(255.0f * std::clamp(claim(u, v), 0.0f, 1.0f) + 0.5f);
            const uint32_t g = uint32_t(255.0f * float(j) / kAcross + 0.5f);
            const uint32_t flags = (self ? 0xff0000u : 0u) | (low ? 0xff00u : 0u) | g;
            p.diffuse |= f << 24;
            p.specular |= flags;
            s.vertices.push_back(p);
            Piece::Origin o;
            o.u = u;
            o.v = v;
            o.flags = flags;
            s.origins.push_back(o);
        }
    // 2.7.5: each quad split along the cell's own crease (the game's two triangles' diagonal) - its q0-q3 runs along
    // u = v where the strip goes towards -x or -z, along u + v = 1 the other two ways. Split the other way, on a hill
    // a strip cut across the crease, a third of a unit off the ground: hidden under it in places, it blinked as the
    // camera turned (half a cell wide, its grid lies on the crease)
    const bool alongQ03 = w.dx == -1 || w.dz == -1 ? a.across03 : !a.across03;
    // 2.7.8: and a triangle the crease still runs through cut along it - the own band's quads, 0.06 of a cell wide
    // and a quarter long, crossed it near the cell's corners: up to a third of a unit under prt_fild08's hills about
    // 200 200, hidden there now and then as the camera turned. A flat cell has no crease to follow.
    const bool creased = (in.flags & 2u) != 0;
    const auto creaseSide = [&](float u, float v) { return a.across03 ? u - v : u + v - 1.0f; };
    const auto triangle = [&](int k0, int k1, int k2) {  // the grid's points
        const int k[3] = {k0, k1, k2};
        float side[3];
        int sign[3], plus = 0, minus = 0;
        for (int n = 0; n < 3; ++n) {
            side[n] = creaseSide(placeU[k[n]], placeV[k[n]]);
            sign[n] = side[n] > 1e-5f ? 1 : side[n] < -1e-5f ? -1 : 0;
            plus += sign[n] > 0 ? 1 : 0;
            minus += sign[n] < 0 ? 1 : 0;
        }
        if (!creased || !plus || !minus) {
            for (int n = 0; n < 3; ++n) s.indices.push_back(uint16_t(k[n]));
            return;
        }
        // where the crease crosses the edge from the triangle's corner n to its corner m
        const auto cross = [&](int n, int m) {
            const float t = side[n] / (side[n] - side[m]);
            const float u = lerpf(placeU[k[n]], placeU[k[m]], t), v = lerpf(placeV[k[n]], placeV[k[m]], t);
            s.vertices.push_back(between(a, s.vertices[size_t(k[n])], s.vertices[size_t(k[m])], t, u, v));
            Piece::Origin o;
            o.u = u;
            o.v = v;
            o.t = t;
            o.n = int16_t(k[n]);
            o.m = int16_t(k[m]);
            s.origins.push_back(o);
            return uint16_t(s.vertices.size() - 1);
        };
        const auto put = [&](uint16_t x, uint16_t y, uint16_t z) {
            for (uint16_t q : {x, y, z}) s.indices.push_back(q);
        };
        // turned so the corner alone on its side (or on the crease) comes first, the order kept
        int lone = 0;
        for (int n = 0; n < 3; ++n)
            if (sign[n] == 0 || (sign[n] > 0 ? plus : minus) == 1) lone = n;
        for (int n = 0; n < 3; ++n)
            if (sign[n] == 0) lone = n;
        const int l = lone, b = (lone + 1) % 3, c = (lone + 2) % 3;
        const uint16_t at[3] = {uint16_t(k[0]), uint16_t(k[1]), uint16_t(k[2])};
        if (sign[l] == 0) {  // the crease from this corner across the opposite edge: two halves
            const uint16_t x = cross(b, c);
            put(at[l], at[b], x);
            put(at[l], x, at[c]);
        } else {  // a corner cut off: it, and the four-sided rest in two
            const uint16_t x1 = cross(l, b), x2 = cross(c, l);
            put(at[l], x1, x2);
            put(x1, at[b], at[c]);
            put(x1, at[c], x2);
        }
    };
    for (int j = 0; j < kAcross; ++j)
        for (int i = 0; i < kAlong; ++i) {
            const int q0 = j * (kAlong + 1) + i, q1 = q0 + 1, q2 = q0 + kAlong + 1, q3 = q2 + 1;
            if (alongQ03) {
                triangle(q0, q1, q3);
                triangle(q0, q3, q2);
            } else {
                triangle(q0, q1, q2);
                triangle(q2, q1, q3);
            }
        }
}

// A strip's group and the texture it shows, as one key
uint64_t stripOf(uint32_t inside, uint32_t shows) { return uint64_t(inside) << 32 | shows; }

// A piece onto its strip: the indices moved on by the vertices before; a strip full (65535 vertices with this piece's
// worst) takes no more
void onto(Strip& s, const Piece& p) {
    if (s.vertices.size() + kPieceMost > 65535) return;
    const size_t first = s.vertices.size(), at = s.indices.size();
    s.vertices.insert(s.vertices.end(), p.vertices.begin(), p.vertices.end());
    s.indices.resize(at + p.indices.size());
    for (size_t i = 0; i < p.indices.size(); ++i) s.indices[at + i] = uint16_t(first + p.indices[i]);
}

// The pieces into the strips in the order they come: one strip a cell's group and the texture it shows
void append(std::vector<Strip>& out, std::unordered_map<uint64_t, size_t>& strips, const Piece& p) {
    auto found = strips.find(stripOf(p.inside, p.shows));
    if (found == strips.end()) {
        found = strips.emplace(stripOf(p.inside, p.shows), out.size()).first;
        Strip st;
        st.inside = p.inside;
        st.shows = p.shows;
        out.push_back(std::move(st));
    }
    onto(out[found->second], p);
}

// The ground's cells and the edges between them that get strips, in the order the strips are made: the cells by
// their places, the four ways each
struct Edge {
    const Cell* a;
    const Cell* b;
    const Way* w;
    int ix, iz;
    int64_t key;  // a's place
    int way;
    uint32_t ta, tb, nearby;
    bool creased, low;
};

// cells: filled from the ground; false when there are none (no strips)
template <typename Visit>
bool edgesOf(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture, const Laid& laid,
             CellMap& cells, std::vector<uint32_t>& order, Visit&& visit) {
    const float side = cellSide(ground);
    if (side < 0.5f || ground.empty()) return false;
    // The grid's origin: the cells' corners lie on it.
    const float ox = ground[0].v[0].pos[0] - std::floor(ground[0].v[0].pos[0] / side) * side;
    const float oz = ground[0].v[0].pos[2] - std::floor(ground[0].v[0].pos[2] / side) * side;
    cells.reset(ground.size());
    for (const Triangle& t : ground) {
        // the ground's own faces only: its walls (the cliffs' sides) stand up
        const float ax = t.v[1].pos[0] - t.v[0].pos[0], ay = t.v[1].pos[1] - t.v[0].pos[1],
                    az = t.v[1].pos[2] - t.v[0].pos[2], bx = t.v[2].pos[0] - t.v[0].pos[0],
                    by = t.v[2].pos[1] - t.v[0].pos[1], bz = t.v[2].pos[2] - t.v[0].pos[2];
        const float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        const float n = std::sqrt(nx * nx + ny * ny + nz * nz);
        // 3.0.0-beta.2: nor a triangle of (almost) no area - the side face the game draws between two cells of one
        // height has no normal, and took a cell with the wall's texture (R-45AV5H: lines round Aldebaran's paving)
        if (n < 0.01f * side * side || std::fabs(ny) < 0.6f * n) continue;
        const float cx = (t.v[0].pos[0] + t.v[1].pos[0] + t.v[2].pos[0]) / 3.0f;
        const float cz = (t.v[0].pos[2] + t.v[1].pos[2] + t.v[2].pos[2]) / 3.0f;
        const int ix = floorInt((cx - ox) / side), iz = floorInt((cz - oz) / side);
        Cell& c = cells.at(keyOf(ix, iz));
        c.group = t.group;
        const float x0 = ox + float(ix) * side, z0 = oz + float(iz) * side;
        bool used[4] = {false, false, false, false};
        for (const Vertex& v : t.v) {
            const int cx1 = std::fabs(v.pos[0] - x0) < std::fabs(v.pos[0] - x0 - side) ? 0 : 1;
            const int cz1 = std::fabs(v.pos[2] - z0) < std::fabs(v.pos[2] - z0 - side) ? 0 : 1;
            const int k = cz1 * 2 + cx1;
            c.corner[k] = v;
            c.have[k] = true;
            used[k] = true;
        }
        if (used[0] && used[3]) c.across03 = true;  // a triangle with both ends of a diagonal: that one
    }
    // 2.7.6: the cells in the order of their places, not of the hash map's buckets - those followed the set of cells
    // in view, and as the camera turned the blended strips overlapping at the cells' corners were drawn in another
    // order each frame: another colour there, the hills blinked
    const auto& all = cells.all();
    order.resize(all.size());
    for (size_t i = 0; i < all.size(); ++i) order[i] = uint32_t(i);
    std::sort(order.begin(), order.end(), [&](uint32_t x, uint32_t y) { return before(all[x].first, all[y].first); });
    for (const uint32_t at : order) {
        const auto& entry = all[at];
        const Cell& a = entry.second;
        if (!(a.have[0] && a.have[1] && a.have[2] && a.have[3])) continue;
        const int ix = xOf(entry.first), iz = zOf(entry.first);
        for (int way = 0; way < 4; ++way) {
            const Way& w = kWays[way];
            const Cell* nb = cells.find(keyOf(ix + w.dx, iz + w.dz));
            if (!nb) continue;
            const Cell& b = *nb;
            if (!(b.have[0] && b.have[1] && b.have[2] && b.have[3])) continue;
            // 2.1.6: no seam over a step - a wall stands between the two; the strip at its foot, against the wall's
            // own faces, blinked on c_tower2's floors
            bool step = false;
            for (int k = 0; k < 2; ++k)
                step = step || std::fabs(a.corner[w.mine[k]].pos[1] - b.corner[w.theirs[k]].pos[1]) > 0.05f * side;
            if (step) continue;
            // 2.7.57: no seam round a patch laid by hand - its edge as straight as the game draws it
            if (laid && (laid(ox + (float(ix) + 0.5f) * side, oz + (float(iz) + 0.5f) * side) ||
                         laid(ox + (float(ix + w.dx) + 0.5f) * side, oz + (float(iz + w.dz) + 0.5f) * side)))
                continue;
            const uint32_t ta = a.group < texture.size() ? texture[a.group] : 0;
            const uint32_t tb = b.group < texture.size() ? texture[b.group] : 0;
            if (ta == tb) {  // one texture: a seam only where its coordinates jump at the edge
                bool alike = true;
                for (int k = 0; k < 2; ++k) {
                    const Vertex& m = a.corner[w.mine[k]];
                    const Vertex& t = b.corner[w.theirs[k]];
                    alike = alike && same(m.uv0[0], t.uv0[0]) && same(m.uv0[1], t.uv0[1]);
                }
                if (alike) continue;
            }
            Edge e;
            e.a = &a;
            e.b = &b;
            e.w = &w;
            e.ix = ix;
            e.iz = iz;
            e.key = entry.first;
            e.way = way;
            e.ta = ta;
            e.tb = tb;
            e.low = before(entry.first, keyOf(ix + w.dx, iz + w.dz));  // the side whose noise counts up for the other
            e.nearby = 0;
            for (int j = 0; j < 3; ++j)
                for (int i = 0; i < 3; ++i) {
                    const int cx = ix - 1 + i, cz = iz - 1 + j;
                    bool theirs = false;
                    if (cx == ix + w.dx && cz == iz + w.dz) {
                        theirs = true;
                    } else if (ta != tb) {
                        const Cell* c = cells.find(keyOf(cx, cz));
                        theirs = c && c->group < texture.size() && texture[c->group] == tb;
                    }
                    if (theirs) e.nearby |= 1u << (j * 3 + i);
                }
            e.creased = std::fabs(a.corner[0].pos[1] + a.corner[3].pos[1] - a.corner[1].pos[1] - a.corner[2].pos[1]) >
                        1e-3f;
            visit(e);
        }
    }
    return true;
}

}  // namespace

std::vector<Strip> build(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture, float width,
                         const Laid& laid) {
    std::vector<Strip> out;
    std::unordered_map<uint64_t, size_t> strips;  // (inside, shows) -> its strip
    CellMap cells;
    std::vector<uint32_t> order;
    Piece piece;
    edgesOf(ground, texture, laid, cells, order, [&](const Edge& e) {
        const Inputs in = inputsOf(*e.a, *e.b, e.ta, e.tb, e.nearby, e.creased, e.low, width);
        makePiece(*e.a, *e.b, *e.w, e.ix, e.iz, in, false, width, piece);
        append(out, strips, piece);
        makePiece(*e.a, *e.b, *e.w, e.ix, e.iz, in, true, kBand * 2.0f, piece);
        append(out, strips, piece);
    });
    return out;
}

// 3.0.5-beta.9: the edges' pieces kept from the last build, by their cell's place and way
struct Cache::Kept {
    struct Edge {
        Inputs in;
        Piece pieces[2];
        uint64_t built = 0;  // the build it was last seen in
    };
    std::unordered_map<uint64_t, Edge> edges;
    CellMap cells;
    std::vector<uint32_t> order;
    // the strips by their group and texture, kept from build to build (nothing allocated a strip a frame): its place
    // in this build's, the build that made it, and between builds its room - its vectors - to fill again
    struct Slot {
        size_t index = 0;
        uint64_t built = 0;
        Strip room;
    };
    std::unordered_map<uint64_t, Slot> slots;
    uint64_t builds = 0;
};

Cache::Cache() : kept_(std::make_unique<Kept>()) {}
Cache::~Cache() = default;

const std::vector<Strip>& Cache::build(const std::vector<Triangle>& ground, const std::vector<uint32_t>& texture,
                                       float width, const Laid& laid, uint64_t laidKey) {
    const size_t groundBytes = ground.size() * sizeof(Triangle), textureBytes = texture.size() * sizeof(uint32_t);
    const bool same = known_ && width == width_ && laidKey == laidKey_ && ground.size() == ground_.size() &&
                      texture.size() == texture_.size() &&
                      (!groundBytes || std::memcmp(ground.data(), ground_.data(), groundBytes) == 0) &&
                      (!textureBytes || std::memcmp(texture.data(), texture_.data(), textureBytes) == 0);
    if (same) return strips_;
    // 3.0.5-beta.9: the ground differs (the camera moved): each edge whose cells are the same as in the last build
    // takes its strips again, their places and fog from this frame's cell; the others are made
    Kept& k = *kept_;
    const uint64_t now = ++k.builds;
    if (width != width_ || laidKey != laidKey_) k.edges.clear();
    for (Strip& s : strips_) {  // their room back to their slots
        Strip& room = k.slots[stripOf(s.inside, s.shows)].room;
        room = std::move(s);
        room.vertices.clear();
        room.indices.clear();
    }
    strips_.clear();
    edgesOf(ground, texture, laid, k.cells, k.order, [&](const Edge& e) {
        const Inputs in = inputsOf(*e.a, *e.b, e.ta, e.tb, e.nearby, e.creased, e.low, width);
        Kept::Edge& kept = k.edges[uint64_t(e.key) * 4 + uint64_t(e.way)];
        if (kept.built && std::memcmp(&kept.in, &in, sizeof in) == 0) {
            for (Piece& p : kept.pieces) p.refresh(*e.a);
            ++reused_;
        } else {
            kept.in = in;
            makePiece(*e.a, *e.b, *e.w, e.ix, e.iz, in, false, width, kept.pieces[0]);
            makePiece(*e.a, *e.b, *e.w, e.ix, e.iz, in, true, kBand * 2.0f, kept.pieces[1]);
        }
        kept.built = now;
        for (const Piece& p : kept.pieces) {
            Kept::Slot& slot = k.slots[stripOf(p.inside, p.shows)];
            if (slot.built != now) {  // the first piece of its strip in this build
                slot.built = now;
                slot.index = strips_.size();
                slot.room.inside = p.inside;
                slot.room.shows = p.shows;
                strips_.push_back(std::move(slot.room));
            }
            onto(strips_[slot.index], p);
        }
    });
    for (auto it = k.edges.begin(); it != k.edges.end();)  // the edges out of view gone
        it = it->second.built == now ? std::next(it) : k.edges.erase(it);
    ground_ = ground;
    texture_ = texture;
    width_ = width;
    laidKey_ = laidKey;
    known_ = true;
    ++made_;
    return strips_;
}

void Cache::clear() {
    ground_.clear();
    texture_.clear();
    strips_.clear();
    known_ = false;
    kept_->edges.clear();
    kept_->slots.clear();
}

}  // namespace uagfx::seams
