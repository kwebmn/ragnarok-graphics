#include "core/smooth_shade.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"

namespace uagfx::smooth_shade {
namespace {

struct Vec {
    float x, y, z;
};

// Lit face by face: most triangles one colour at their three corners (colourOf a vertex's), not all of them the same.
template <typename ColourOf>
bool faceLit(const std::vector<uint32_t>& triangles, size_t vertices, ColourOf colourOf) {
    const size_t n = triangles.size() / 3;
    size_t flat = 0;
    bool varied = false;
    for (size_t t = 0; t < n; ++t) {
        const uint32_t a = triangles[t * 3], b = triangles[t * 3 + 1], c = triangles[t * 3 + 2];
        if (a >= vertices || b >= vertices || c >= vertices) return false;
        const uint32_t ca = colourOf(a) & 0xFFFFFFu;
        if (ca == (colourOf(b) & 0xFFFFFFu) && ca == (colourOf(c) & 0xFFFFFFu)) ++flat;
        if (ca != (colourOf(triangles[0]) & 0xFFFFFFu)) varied = true;
    }
    return varied && flat * 10 >= n * 8;
}

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
    if (!faceLit(triangles, vertices.size(), [&](uint32_t v) { return vertices[v].colour; })) return out;
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

std::vector<RecordChange> recordChanges(const FrameRecord& rec, int gameWidth, int gameHeight, size_t& draws,
                                        size_t most) {
    std::vector<RecordChange> out;
    draws = 0;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const std::vector<uint16_t>& ix = rec.indices();
    size_t triangles = 0;
    std::vector<Corner> corners;
    std::vector<uint32_t> tris;
    for (const DrawCmd& d : rec.draws()) {
        const DrawSetup& s = rec.states()[d.state];
        if (s.ui || s.blend || !s.zWrite || d.fvf != d3d7::FVF_TL1 || d.vertexCount < 6 || triangles >= most)
            continue;
        const size_t stride = FrameRecord::stride(d.fvf);
        if (d.vertexOffset + size_t(d.vertexCount) * stride > vb.size()) continue;
        tris.clear();
        const uint32_t n = d.indexCount ? d.indexCount : d.vertexCount;
        const bool strip = d.primitive == d3d7::PT_TRIANGLESTRIP;
        const uint32_t count = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
        // 2.6.6: a model the game lights smoothly already is no work and no part of the budget - every opaque model
        // counted against it, and a busy frame could use it up before its face-lit ones came. Told as its triangles
        // are gathered (faceLit's rule): past a fifth of them not one colour, it is not face-lit.
        const uint8_t* colours = vb.data() + d.vertexOffset + 16;
        const auto colourOf = [&](uint32_t v) {
            uint32_t c;
            std::memcpy(&c, colours + size_t(v) * stride, 4);
            return c & 0xFFFFFFu;
        };
        bool bad = false, varied = false;
        uint32_t uneven = 0;
        for (uint32_t t = 0; t < count && !bad; ++t) {
            for (uint32_t k = 0; k < 3; ++k) {
                const uint32_t at = (strip ? t : t * 3) + k;
                uint32_t vi = at;
                if (d.indexCount) {
                    if (d.firstIndex + at >= ix.size()) {
                        bad = true;
                        break;
                    }
                    vi = ix[d.firstIndex + at];
                }
                if (vi >= d.vertexCount) {
                    bad = true;
                    break;
                }
                tris.push_back(vi);
            }
            if (bad) break;
            const uint32_t* tri = tris.data() + tris.size() - 3;
            const uint32_t c = colourOf(tri[0]);
            if (c != colourOf(tri[1]) || c != colourOf(tri[2])) bad = ++uneven * 5 > count;
            varied = varied || c != colourOf(tris[0]);
        }
        if (bad || !varied || tris.size() < 6) continue;
        corners.resize(d.vertexCount);
        for (uint32_t k = 0; k < d.vertexCount; ++k) {
            const uint8_t* at = vb.data() + d.vertexOffset + size_t(k) * stride;
            float xyzw[4];
            std::memcpy(xyzw, at, sizeof xyzw);
            corners[k].x = xyzw[0];
            corners[k].y = xyzw[1];
            corners[k].rhw = xyzw[3];
            std::memcpy(&corners[k].colour, at + 16, 4);
        }
        triangles += tris.size() / 3;
        const std::vector<Change> changes = smooth(corners, tris, p.f0, p.f1, p.cx, p.cy);
        if (!changes.empty()) ++draws;
        for (const Change& ch : changes)
            out.push_back(RecordChange{d.vertexOffset + size_t(ch.vertex) * stride + 16, ch.colour});
    }
    return out;
}

}  // namespace uagfx::smooth_shade
