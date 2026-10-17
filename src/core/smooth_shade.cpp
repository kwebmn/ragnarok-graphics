#include "core/smooth_shade.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace uagfx::smooth_shade {
namespace {

struct Vec {
    float x, y, z;
};

uint64_t keyOf(const Corner& c) {
    uint32_t a, b, r;
    std::memcpy(&a, &c.x, 4);
    std::memcpy(&b, &c.y, 4);
    std::memcpy(&r, &c.rhw, 4);
    return (uint64_t(a) * 0x9E3779B97F4A7C15ull) ^ (uint64_t(b) * 0xC2B2AE3D27D4EB4Full) ^ (uint64_t(r) << 17);
}

}  // namespace

std::vector<Change> smooth(const std::vector<Corner>& vertices, const std::vector<uint32_t>& triangles, float f0,
                           float f1, float cx, float cy, float crease) {
    std::vector<Change> out;
    const size_t n = triangles.size() / 3;
    if (n < 2) return out;
    // lit face by face: most triangles one colour at their three corners, and not all of them the same
    size_t flat = 0;
    bool varied = false;
    for (size_t t = 0; t < n; ++t) {
        const uint32_t a = triangles[t * 3], b = triangles[t * 3 + 1], c = triangles[t * 3 + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) return out;
        const uint32_t ca = vertices[a].colour & 0xFFFFFFu;
        if (ca == (vertices[b].colour & 0xFFFFFFu) && ca == (vertices[c].colour & 0xFFFFFFu)) ++flat;
        if (ca != (vertices[triangles[0]].colour & 0xFFFFFFu)) varied = true;
    }
    if (!varied || flat * 10 < n * 8) return out;
    const auto view = [&](const Corner& c) {
        const float z = 1.0f / c.rhw;
        return Vec{(c.x - cx) * z / f0, (c.y - cy) * z / f1, z};
    };
    // each triangle's normal and twice its area (the weight); the corners at each place
    std::vector<Vec> normal(n);
    std::vector<float> area(n);
    for (size_t t = 0; t < n; ++t) {
        const Corner &ca = vertices[triangles[t * 3]], &cb = vertices[triangles[t * 3 + 1]],
                     &cc = vertices[triangles[t * 3 + 2]];
        if (ca.rhw <= 0.0f || cb.rhw <= 0.0f || cc.rhw <= 0.0f) {
            area[t] = 0.0f;
            normal[t] = Vec{0, 0, 0};
            continue;
        }
        const Vec a = view(ca), b = view(cb), c = view(cc);
        const Vec u{b.x - a.x, b.y - a.y, b.z - a.z}, v{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec m{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        const float l = std::sqrt(m.x * m.x + m.y * m.y + m.z * m.z);
        area[t] = l;
        normal[t] = l > 1e-12f ? Vec{m.x / l, m.y / l, m.z / l} : Vec{0, 0, 0};
    }
    // the corners by place: sorted, each place a run (a map of lists cost too much a frame)
    std::vector<std::pair<uint64_t, uint32_t>> byPlace(n * 3);
    for (size_t k = 0; k < n * 3; ++k) byPlace[k] = {keyOf(vertices[triangles[k]]), uint32_t(k)};
    std::sort(byPlace.begin(), byPlace.end());
    std::vector<bool> done(vertices.size(), false);
    for (size_t run = 0; run < byPlace.size();) {
        size_t end = run + 1;
        while (end < byPlace.size() && byPlace[end].first == byPlace[run].first) ++end;
        for (size_t i = run; i < end; ++i) {
            const uint32_t k = byPlace[i].second, vi = triangles[k];
            if (done[vi]) continue;  // a vertex shared by triangles: from its first
            done[vi] = true;
            const size_t own = k / 3;
            if (area[own] <= 0.0f || end - run < 2) continue;
            float sum[3] = {0, 0, 0}, weight = 0.0f;
            for (size_t j = run; j < end; ++j) {
                const uint32_t other = byPlace[j].second;
                const size_t t = other / 3;
                const float cosine =
                    normal[t].x * normal[own].x + normal[t].y * normal[own].y + normal[t].z * normal[own].z;
                if (area[t] <= 0.0f || cosine < crease) continue;
                const uint32_t c = vertices[triangles[other]].colour;
                sum[0] += float((c >> 16) & 255) * area[t];
                sum[1] += float((c >> 8) & 255) * area[t];
                sum[2] += float(c & 255) * area[t];
                weight += area[t];
            }
            if (weight <= 0.0f) continue;
            const uint32_t was = vertices[vi].colour;
            const uint32_t now = (was & 0xFF000000u) | uint32_t(std::lround(sum[0] / weight)) << 16 |
                                 uint32_t(std::lround(sum[1] / weight)) << 8 | uint32_t(std::lround(sum[2] / weight));
            if (now != was) out.push_back(Change{vi, now});
        }
        run = end;
    }
    return out;
}

}  // namespace uagfx::smooth_shade
