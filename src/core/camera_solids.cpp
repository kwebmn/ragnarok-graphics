#include "core/camera_solids.hpp"

#include <algorithm>
#include <cmath>

#include "core/view3d.hpp"

namespace uagfx::solids {
namespace {

constexpr float kStart = 1e-4f;  // a triangle the way starts on is not met

// the way's share (0-1] at which it meets the triangle a b c, either side; -1 none (Möller-Trumbore)
float meetsOne(const float* a, const float* b, const float* c, const float from[3], const float dir[3],
               float* hitU = nullptr, float* hitV = nullptr) {
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
    if (hitU) *hitU = u;
    if (hitV) *hitV = v;
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

void opacityMask(const uint32_t* argb, int width, int height, uint8_t bits[kMaskBytes]) {
    std::fill(bits, bits + kMaskBytes, uint8_t(0));
    if (!argb || width <= 0 || height <= 0) return;
    for (int cy = 0; cy < kMaskSide; ++cy)
        for (int cx = 0; cx < kMaskSide; ++cx) {
            const int x0 = cx * width / kMaskSide, x1 = std::max(x0 + 1, (cx + 1) * width / kMaskSide);
            const int y0 = cy * height / kMaskSide, y1 = std::max(y0 + 1, (cy + 1) * height / kMaskSide);
            int solid = 0, all = 0;
            for (int y = y0; y < y1 && y < height; ++y)
                for (int x = x0; x < x1 && x < width; ++x, ++all)
                    solid += (argb[size_t(y) * size_t(width) + size_t(x)] >> 24) >= 128;
            if (all && solid * 2 >= all) bits[(cy * kMaskSide + cx) / 8] |= uint8_t(1u << ((cy * kMaskSide + cx) % 8));
        }
}

bool opaqueAt(const uint8_t bits[kMaskBytes], float u, float v) {
    if (!std::isfinite(u) || !std::isfinite(v)) return true;
    u -= std::floor(u);
    v -= std::floor(v);
    const int cx = std::min(kMaskSide - 1, int(u * kMaskSide)), cy = std::min(kMaskSide - 1, int(v * kMaskSide));
    return (bits[(cy * kMaskSide + cx) / 8] >> ((cy * kMaskSide + cx) % 8)) & 1u;
}

int16_t Set::addMask(uint32_t key, const uint8_t bits[kMaskBytes]) {
    const auto known = maskKeys_.find(key);
    if (known != maskKeys_.end()) return known->second;
    if (maskKeys_.size() >= kMostMasks) return -1;
    const auto index = int16_t(maskKeys_.size());
    masks_.insert(masks_.end(), bits, bits + kMaskBytes);
    maskKeys_.emplace(key, index);
    return index;
}

int16_t Set::maskOf(uint32_t key) const {
    const auto known = maskKeys_.find(key);
    return known == maskKeys_.end() ? int16_t(-1) : known->second;
}

void Set::clear() {
    openParts_.clear();
    openCorners_.clear();
    openUv_.clear();
    openMask_.clear();
    masks_.clear();
    maskKeys_.clear();
    parts_.clear();
    corners_.clear();
    known_.clear();
    leftOut_ = thin_ = 0;
}

void Set::add(uint32_t id, const float* vertices, size_t vertexCount, const Face* faces, size_t faceCount) {
    if (!vertices || !faces || !vertexCount || !faceCount || known_.count(id)) return;
    if (triangles() + openCorners_.size() / 9 + faceCount > kMostTriangles) return;
    Part part{{1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}, uint32_t(triangles()), 0};
    Part open{{1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}, uint32_t(openCorners_.size() / 9), 0};
    const size_t maskCount = masks_.size() / kMaskBytes;
    for (size_t k = 0; k < faceCount; ++k) {
        const Face& f = faces[k];
        const bool seenThrough = f.open && f.mask >= 0 && size_t(f.mask) < maskCount;
        if (f.open) ++leftOut_;
        if (f.open && !seenThrough) continue;
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
        Part& to = f.open ? open : part;
        (f.open ? openCorners_ : corners_).insert((f.open ? openCorners_ : corners_).end(), nine, nine + 9);
        if (f.open) {
            openUv_.insert(openUv_.end(), f.uv, f.uv + 6);
            openMask_.push_back(f.mask);
        }
        ++to.count;
        for (int corner = 0; corner < 3; ++corner)
            for (int i = 0; i < 3; ++i) {
                to.lo[i] = std::min(to.lo[i], nine[corner * 3 + i]);
                to.hi[i] = std::max(to.hi[i], nine[corner * 3 + i]);
            }
    }
    known_[id] = uint32_t(parts_.size());
    const auto thin = [](const Part& p) { return std::max(p.hi[0] - p.lo[0], p.hi[2] - p.lo[2]) < kThinUnder; };
    if (open.count) {
        if (thin(open)) {
            openCorners_.resize(size_t(open.first) * 9);
            openUv_.resize(size_t(open.first) * 6);
            openMask_.resize(open.first);
        } else {
            openParts_.push_back(open);
        }
    }
    if (!part.count) return;
    if (thin(part)) {  // a post: its triangles let go
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

float Set::meetsSeen(const float from[3], const float to[3]) const {
    float first = meets(from, to);
    const float dir[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    for (const Part& part : openParts_) {
        if (!through(part.lo, part.hi, from, dir)) continue;
        for (uint32_t k = part.first; k < part.first + part.count; ++k) {
            const float* c = openCorners_.data() + size_t(k) * 9;
            float u = 0.0f, v = 0.0f;
            const float t = meetsOne(c, c + 3, c + 6, from, dir, &u, &v);
            if (!(t > 0.0f) || (first >= 0.0f && t >= first)) continue;
            const float* uv = openUv_.data() + size_t(k) * 6;  // the corners' a + u (b - a) + v (c - a)
            const float pu = uv[0] + u * (uv[2] - uv[0]) + v * (uv[4] - uv[0]);
            const float pv = uv[1] + u * (uv[3] - uv[1]) + v * (uv[5] - uv[1]);
            if (opaqueAt(masks_.data() + size_t(openMask_[k]) * kMaskBytes, pu, pv)) first = t;
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
