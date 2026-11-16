#include "core/camera_solids.hpp"

#include <algorithm>
#include <cmath>

#include "core/view3d.hpp"

namespace uagfx::solids {
namespace {

constexpr float kStart = 1e-4f;  // a triangle the way starts on is not met

// the way's share (0-1] at which it meets the triangle a b c, either side; -1 none (Möller-Trumbore)
float meetsOne(const float* a, const float* b, const float* c, const float from[3], const float dir[3]) {
    const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float p[3] = {dir[1] * e2[2] - dir[2] * e2[1], dir[2] * e2[0] - dir[0] * e2[2],
                        dir[0] * e2[1] - dir[1] * e2[0]};
    const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (std::fabs(det) < 1e-9f) return -1.0f;  // along it
    const float inv = 1.0f / det;
    const float s[3] = {from[0] - a[0], from[1] - a[1], from[2] - a[2]};
    const float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
    if (u < 0.0f || u > 1.0f) return -1.0f;
    const float q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
    const float v = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    const float t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    return t > kStart && t <= 1.0f ? t : -1.0f;
}

// whether the way from `from` along `dir` (its whole length) goes through the box lo..hi
bool through(const float lo[3], const float hi[3], const float from[3], const float dir[3]) {
    float t0 = 0.0f, t1 = 1.0f;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dir[i]) < 1e-9f) {
            if (from[i] < lo[i] || from[i] > hi[i]) return false;
            continue;
        }
        float a = (lo[i] - from[i]) / dir[i], b = (hi[i] - from[i]) / dir[i];
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1) return false;
    }
    return true;
}

}  // namespace

void Set::clear() {
    parts_.clear();
    corners_.clear();
    known_.clear();
    leftOut_ = thin_ = 0;
}

void Set::add(uint32_t id, const float* vertices, size_t vertexCount, const Face* faces, size_t faceCount) {
    if (!vertices || !faces || !vertexCount || !faceCount || known_.count(id)) return;
    if (triangles() + faceCount > kMostTriangles) return;
    Part part{{1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}, uint32_t(triangles()), 0};
    for (size_t k = 0; k < faceCount; ++k) {
        const Face& f = faces[k];
        if (f.open) {
            ++leftOut_;
            continue;
        }
        if (f.v[0] >= vertexCount || f.v[1] >= vertexCount || f.v[2] >= vertexCount) continue;
        float nine[9];
        bool fine = true;
        for (int corner = 0; corner < 3; ++corner)
            for (int i = 0; i < 3; ++i) {
                const float x = vertices[size_t(f.v[corner]) * 3 + size_t(i)];
                fine = fine && std::isfinite(x) && std::fabs(x) <= kMostCorner;
                nine[corner * 3 + i] = x;
            }
        if (!fine) continue;
        corners_.insert(corners_.end(), nine, nine + 9);
        ++part.count;
        for (int corner = 0; corner < 3; ++corner)
            for (int i = 0; i < 3; ++i) {
                part.lo[i] = std::min(part.lo[i], nine[corner * 3 + i]);
                part.hi[i] = std::max(part.hi[i], nine[corner * 3 + i]);
            }
    }
    known_[id] = uint32_t(parts_.size());
    if (!part.count) return;
    if (std::max(part.hi[0] - part.lo[0], part.hi[2] - part.lo[2]) < kThinUnder) {  // a post: its triangles let go
        corners_.resize(size_t(part.first) * 9);
        ++thin_;
        return;
    }
    parts_.push_back(part);
}

float Set::meets(const float from[3], const float to[3]) const {
    const float dir[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    float first = -1.0f;
    for (const Part& part : parts_) {
        if (!through(part.lo, part.hi, from, dir)) continue;
        const float* c = corners_.data() + size_t(part.first) * 9;
        for (uint32_t k = 0; k < part.count; ++k, c += 9) {
            const float t = meetsOne(c, c + 3, c + 6, from, dir);
            if (t > 0.0f && (first < 0.0f || t < first)) first = t;
        }
    }
    return first;
}

float freeOfSolids(const float eye[3], const float target[3], float distance, const Set& set, float radius) {
    if (!(distance > view3d::kPullNearest) || !set.triangles()) return distance;
    float dir[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (!(len > 1e-6f)) return distance;
    for (float& d : dir) d /= len;
    const float head[3] = {eye[0] + dir[0] * distance, eye[1] + dir[1] * distance, eye[2] + dir[2] * distance};
    // across the way: the world's up (y down) and what is left of it
    float right[3] = {-dir[2], 0.0f, dir[0]};  // dir x up, laid flat
    const float flat = std::sqrt(right[0] * right[0] + right[2] * right[2]);
    if (flat > 1e-4f) {
        right[0] /= flat;
        right[2] /= flat;
    } else {
        right[0] = 1.0f, right[2] = 0.0f;  // straight up or down
    }
    const float up[3] = {dir[1] * right[2] - dir[2] * right[1], dir[2] * right[0] - dir[0] * right[2],
                         dir[0] * right[1] - dir[1] * right[0]};
    float free = distance;
    const int ways = radius > 0.0f ? 5 : 1;
    static const float kAside[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int k = 0; k < ways; ++k) {
        const float end[3] = {eye[0] + (right[0] * kAside[k][0] + up[0] * kAside[k][1]) * radius,
                              eye[1] + (right[1] * kAside[k][0] + up[1] * kAside[k][1]) * radius,
                              eye[2] + (right[2] * kAside[k][0] + up[2] * kAside[k][1]) * radius};
        const float share = set.meets(head, end);
        if (share > 0.0f) free = std::min(free, share * distance);
    }
    return free < distance ? std::max(view3d::kPullNearest, free - view3d::kPullStep) : distance;
}

void keepOffSolids(const float from[3], float to[3], const Set& set) {
    if (!set.triangles()) return;
    float at[3] = {from[0], from[1], from[2]};
    for (int axis = 0; axis < 3; ++axis) {
        const float d = to[axis] - at[axis];
        if (!(std::fabs(d) > 1e-6f)) continue;
        const float way = std::fabs(d), sign = d > 0.0f ? 1.0f : -1.0f;
        float probe[3] = {at[0], at[1], at[2]};
        probe[axis] += sign * (way + view3d::kFreeGap);
        float allowed = way;
        const float share = set.meets(at, probe);
        const float reach = way + view3d::kFreeGap;
        if (share > 0.0f) allowed = std::min(allowed, std::max(0.0f, share * reach - view3d::kFreeGap));
        at[axis] += sign * allowed;
    }
    std::copy(at, at + 3, to);
}

}  // namespace uagfx::solids
