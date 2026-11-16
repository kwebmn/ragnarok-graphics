#include "core/model_gaps.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

namespace uagfx::model_gaps {
namespace {

// an edge one face of a part has: the face, its two corners there (0-2), the welded corners they are
struct Edge {
    uint32_t face = 0;
    uint8_t a = 0, b = 0;
    int u = 0, v = 0;
};

struct Part {
    float low = 1e30f, high = -1e30f;  // up positive
    std::vector<uint32_t> faces;
    std::vector<Edge> open;  // its edges one face has
};

// a ring's reach on the ground
struct Reach {
    float x0 = 1e30f, z0 = 1e30f, x1 = -1e30f, z1 = -1e30f;
    void add(const float* p) {
        x0 = std::min(x0, p[0]), x1 = std::max(x1, p[0]);
        z0 = std::min(z0, p[2]), z1 = std::max(z1, p[2]);
    }
    bool holds(const Reach& o) const {
        return o.x0 >= x0 - 0.5f && o.x1 <= x1 + 0.5f && o.z0 >= z0 - 0.5f && o.z1 <= z1 + 0.5f;
    }
};

}  // namespace

size_t close(gamemodels::Node& n) {
    const size_t corners = n.vertices.size() / 3, faces = n.faces.size();
    if (n.moving || faces < 2 || corners < 4 || corners > 60000) return 0;
    // the corners of one place one corner
    std::map<std::tuple<int, int, int>, int> places;
    std::vector<int> welded(corners);
    std::vector<int> first;  // a welded corner's first vertex
    for (size_t v = 0; v < corners; ++v) {
        const float* p = &n.vertices[v * 3];
        const auto key = std::make_tuple(int(std::lround(p[0] * 16.0f)), int(std::lround(p[1] * 16.0f)),
                                         int(std::lround(p[2] * 16.0f)));
        const auto [it, fresh] = places.try_emplace(key, int(first.size()));
        if (fresh) first.push_back(int(v));
        welded[v] = it->second;
    }
    const auto at = [&](int corner) { return &n.vertices[size_t(first[size_t(corner)]) * 3]; };
    const auto upOf = [&](int corner) { return -at(corner)[1]; };
    // the parts: the faces that share corners
    std::vector<int> root(first.size());
    std::iota(root.begin(), root.end(), 0);
    const auto find = [&](int x) {
        while (root[size_t(x)] != x) x = root[size_t(x)] = root[size_t(root[size_t(x)])];
        return x;
    };
    for (const gamemodels::Face& f : n.faces) {
        for (int k = 0; k < 3; ++k)
            if (size_t(f.v[k]) >= corners) return 0;
        root[size_t(find(welded[f.v[0]]))] = find(welded[f.v[1]]);
        root[size_t(find(welded[f.v[1]]))] = find(welded[f.v[2]]);
    }
    std::map<int, size_t> partOf;
    std::vector<Part> parts;
    std::vector<size_t> faceParts(faces);
    for (size_t f = 0; f < faces; ++f) {
        const auto [it, fresh] = partOf.try_emplace(find(welded[n.faces[f].v[0]]), parts.size());
        if (fresh) parts.emplace_back();
        faceParts[f] = it->second;
        Part& p = parts[it->second];
        p.faces.push_back(uint32_t(f));
        for (int k = 0; k < 3; ++k) {
            const float up = upOf(welded[n.faces[f].v[k]]);
            p.low = std::min(p.low, up), p.high = std::max(p.high, up);
        }
    }
    if (parts.size() < 2) return 0;
    for (Part& p : parts) {  // its edges one face has
        std::map<std::pair<int, int>, std::pair<int, Edge>> edges;
        for (const uint32_t f : p.faces)
            for (int k = 0; k < 3; ++k) {
                Edge e;
                e.face = f;
                e.a = uint8_t(k), e.b = uint8_t((k + 1) % 3);
                e.u = welded[n.faces[f].v[e.a]], e.v = welded[n.faces[f].v[e.b]];
                if (e.u == e.v) continue;
                auto& slot = edges[{std::min(e.u, e.v), std::max(e.u, e.v)}];
                if (slot.first++ == 0) slot.second = e;
            }
        for (const auto& e : edges)
            if (e.second.first == 1) p.open.push_back(e.second.second);
    }
    // the face of another part straight over a place, the lowest one at it or over it; its height there
    const auto over = [&](const float* p, size_t part, size_t& hitPart, float& hitUp) {
        bool any = false;
        for (size_t f = 0; f < faces; ++f) {
            if (faceParts[f] == part) continue;
            const float* a = &n.vertices[size_t(n.faces[f].v[0]) * 3];
            const float* b = &n.vertices[size_t(n.faces[f].v[1]) * 3];
            const float* c = &n.vertices[size_t(n.faces[f].v[2]) * 3];
            const float d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2]);
            if (std::fabs(d) < 1e-6f) continue;  // upright: nothing over a place
            const float wa = ((b[2] - c[2]) * (p[0] - c[0]) + (c[0] - b[0]) * (p[2] - c[2])) / d;
            const float wb = ((c[2] - a[2]) * (p[0] - c[0]) + (a[0] - c[0]) * (p[2] - c[2])) / d;
            const float wc = 1.0f - wa - wb;
            if (wa < -1e-3f || wb < -1e-3f || wc < -1e-3f) continue;
            const float up = -(wa * a[1] + wb * b[1] + wc * c[1]);
            if (up < -p[1] - 0.01f || (any && up >= hitUp)) continue;
            any = true;
            hitUp = up;
            hitPart = faceParts[f];
        }
        return any;
    };
    struct Raise {
        Edge edge;
        float topA[3], topB[3];
    };
    std::vector<Raise> walls;
    std::vector<Edge> lids;       // an edge of a lid's ring ...
    std::vector<size_t> lidOf;    // ... and which lid
    std::vector<std::array<float, 3>> lidMiddles;
    for (size_t b = 0; b < parts.size(); ++b) {
        const Part& lower = parts[b];
        std::vector<Edge> ring;
        Reach reach;
        float ringUp = 0.0f;
        for (const Edge& e : lower.open) {
            if (upOf(e.u) < lower.high - kRingBand || upOf(e.v) < lower.high - kRingBand) continue;
            ring.push_back(e);
            reach.add(at(e.u));
            reach.add(at(e.v));
            ringUp += 0.5f * (upOf(e.u) + upOf(e.v));
        }
        if (int(ring.size()) < kLeastRing) continue;
        ringUp /= float(ring.size());
        // the part over every corner of the ring: one and the same
        size_t upper = parts.size();
        bool covered = true;
        std::map<int, float> hits;  // a corner's height on the part over it
        for (const Edge& e : ring)
            for (const int corner : {e.u, e.v}) {
                if (hits.count(corner)) continue;
                size_t part = 0;
                float up = 0.0f;
                if (!over(at(corner), b, part, up) || (upper != parts.size() && part != upper) ||
                    up - upOf(corner) > kMostRise) {
                    covered = false;
                    break;
                }
                upper = part;
                hits[corner] = up;
            }
        if (!covered || upper == parts.size()) continue;
        const Part& cap = parts[upper];
        const float gap = cap.low - ringUp;
        if (!(gap > kLeastGap && gap <= kMostGap) ||
            std::min(reach.x1 - reach.x0, reach.z1 - reach.z0) < kWide * gap)
            continue;
        // the upper part's open bottom, the ring within it: a lid, the walls up to it
        std::vector<Edge> bottom;
        Reach bottomReach;
        float lidUp = 0.0f, middle[3] = {0.0f, 0.0f, 0.0f};
        for (const Edge& e : cap.open) {
            if (upOf(e.u) > cap.low + kRingBand || upOf(e.v) > cap.low + kRingBand) continue;
            bottom.push_back(e);
            bottomReach.add(at(e.u));
            bottomReach.add(at(e.v));
            lidUp += 0.5f * (upOf(e.u) + upOf(e.v));
            for (int k = 0; k < 3; ++k) middle[k] += 0.5f * (at(e.u)[k] + at(e.v)[k]);
        }
        const bool lid = int(bottom.size()) >= kLeastRing && bottomReach.holds(reach);
        if (lid) {
            lidUp /= float(bottom.size());
            for (float& m : middle) m /= float(bottom.size());
            for (const Edge& e : bottom) {
                lids.push_back(e);
                lidOf.push_back(lidMiddles.size());
            }
            lidMiddles.push_back({middle[0], -lidUp, middle[2]});
        }
        for (const Edge& e : ring) {
            Raise r;
            r.edge = e;
            for (int side = 0; side < 2; ++side) {
                const int corner = side ? e.v : e.u;
                float* top = side ? r.topB : r.topA;
                top[0] = at(corner)[0];
                top[1] = -(lid ? lidUp : hits[corner]);
                top[2] = at(corner)[2];
            }
            walls.push_back(r);
        }
    }
    if (walls.empty()) return 0;
    if (corners + walls.size() * 2 + lidMiddles.size() > 65535) return 0;
    // the new corners and faces: a new corner wears what the corner under it (a lid's middle: its ring's first) wears
    const auto corner = [&](const float* p) {
        n.vertices.insert(n.vertices.end(), p, p + 3);
        return uint16_t(n.vertices.size() / 3 - 1);
    };
    const auto wear = [](gamemodels::Face& to, int k, const gamemodels::Face& from, int c) {
        to.uv[2 * k] = from.uv[2 * c], to.uv[2 * k + 1] = from.uv[2 * c + 1];
        to.colour[k] = from.colour[c];
        for (int i = 0; i < 3; ++i) to.lit[3 * k + i] = from.lit[3 * c + i];
    };
    size_t added = 0;
    const size_t cornersBefore = n.vertices.size() / 3;
    for (const Raise& r : walls) {
        const gamemodels::Face from = n.faces[r.edge.face];
        const uint16_t topA = corner(r.topA), topB = corner(r.topB);
        gamemodels::Face one = from, two = from;
        one.flags = two.flags = uint8_t(from.flags | gamemodels::kTwoSided);
        one.v[0] = from.v[r.edge.a], one.v[1] = from.v[r.edge.b], one.v[2] = topB;
        wear(one, 0, from, r.edge.a), wear(one, 1, from, r.edge.b), wear(one, 2, from, r.edge.b);
        two.v[0] = from.v[r.edge.a], two.v[1] = topB, two.v[2] = topA;
        wear(two, 0, from, r.edge.a), wear(two, 1, from, r.edge.b), wear(two, 2, from, r.edge.a);
        n.faces.push_back(one);
        n.faces.push_back(two);
        added += 2;
    }
    std::vector<uint16_t> middles;
    for (const auto& m : lidMiddles) middles.push_back(corner(m.data()));
    for (size_t i = 0; i < lids.size(); ++i) {
        const Edge& e = lids[i];
        const gamemodels::Face from = n.faces[e.face];
        gamemodels::Face lid = from;
        lid.flags = uint8_t(from.flags | gamemodels::kTwoSided);
        lid.v[0] = from.v[e.a], lid.v[1] = from.v[e.b], lid.v[2] = middles[lidOf[i]];
        wear(lid, 0, from, e.a), wear(lid, 1, from, e.b), wear(lid, 2, from, e.a);
        n.faces.push_back(lid);
        ++added;
    }
    n.addedVertices += uint32_t(n.vertices.size() / 3 - cornersBefore);
    n.addedFaces += uint32_t(added);
    return added;
}

}  // namespace uagfx::model_gaps
