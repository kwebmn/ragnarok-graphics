#include "core/view3d.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::view3d {
namespace {

constexpr float kRad = 3.14159265358979f / 180.0f;
constexpr float kWorldUp[3] = {0.0f, -1.0f, 0.0f};  // world y points down

void cross(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

bool normalize(float v[3]) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(len > 1e-6f)) return false;
    for (int i = 0; i < 3; ++i) v[i] /= len;
    return true;
}

}  // namespace

Mode next(Mode m) {
    switch (m) {
        case Mode::Off: return Mode::Behind;
        case Mode::Behind: return Mode::First;
        default: return Mode::Off;
    }
}

const char* nameOf(Mode m) {
    switch (m) {
        case Mode::Behind: return "behind";
        case Mode::First: return "first person";
        default: return "the game's own";
    }
}

float fovFor(Mode m) { return fovFor(m, int(kFirstFov), int(kBehindFov)); }

float fovFor(Mode m, int first, int behind) {
    switch (m) {
        case Mode::Behind: return float(behind);
        case Mode::First: return float(first);
        default: return kGameFov;
    }
}

void turn(Look& l, float dx, float dy, float degreesPerPixel, float sign) {
    float yaw = l.yaw + dx * degreesPerPixel * (sign < 0.0f ? -1.0f : 1.0f);
    yaw = std::fmod(yaw, 360.0f);
    if (yaw < 0.0f) yaw += 360.0f;
    l.yaw = yaw;
    l.pitch = std::clamp(l.pitch - dy * degreesPerPixel, -kPitchMost, kPitchMost);
}

bool ahead(const float view[12], float out[3]) {
    out[0] = view[2];
    out[1] = 0.0f;
    out[2] = view[8];
    return normalize(out);
}

float turnSign(const float view[12], float yawDegrees) {
    const float y = yawDegrees * kRad;
    const float grows[3] = {std::cos(y), 0.0f, -std::sin(y)};  // d/dyaw of (sin, 0, cos)
    const float right = grows[0] * view[0] + grows[1] * view[3] + grows[2] * view[6];
    return right >= 0.0f ? 1.0f : -1.0f;
}

float freeDistance(const float eye[3], const float target[3], float distance,
                   const std::function<float(float x, float z)>& groundY) {
    if (!(distance > kPullNearest) || !groundY) return distance;
    float dir[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
    if (!normalize(dir)) return distance;
    const float head[3] = {eye[0] + dir[0] * distance, eye[1] + dir[1] * distance, eye[2] + dir[2] * distance};
    for (float s = kPullStep;; s += kPullStep) {
        const float at = std::min(s, distance);
        const float p[3] = {head[0] - dir[0] * at, head[1] - dir[1] * at, head[2] - dir[2] * at};
        const float h = groundY(p[0], p[2]);
        if (std::isfinite(h) && p[1] > h - kPullMargin) return std::max(kPullNearest, at - kPullStep);
        if (at >= distance) return distance;
    }
}

namespace {
float gapTo(const AimBox& b, float x, float y) {
    const float dx = std::max({b.x0 - x, 0.0f, x - b.x1}), dy = std::max({b.y0 - y, 0.0f, y - b.y1});
    return std::hypot(dx, dy);
}
float centreGap(const AimBox& b, float x, float y) { return std::hypot((b.x0 + b.x1) / 2 - x, (b.y0 + b.y1) / 2 - y); }
}  // namespace

Aim aimAt(const std::vector<AimBox>& boxes, float cx, float cy, float reach, uint32_t locked, float lockedReach) {
    Aim a{cx, cy, 0};
    const AimBox* best = nullptr;
    float bestRank = 0, bestGap = 0;
    for (const AimBox& b : boxes) {
        if (!b.id || !(b.x1 > b.x0) || !(b.y1 > b.y0)) continue;
        const bool lock = locked && b.id == locked;
        const float gap = gapTo(b, cx, cy);
        if (gap > (lock ? std::max(reach, lockedReach) : reach)) continue;
        const float rank = gap <= 0.0f ? (lock ? -3.0f : -2.0f) : lock ? -1.0f : gap;
        if (!best || rank < bestRank) best = &b, bestRank = rank, bestGap = gap;
    }
    if (!best) return a;
    a.id = best->id;
    if (bestGap <= 0.0f) return a;
    const float ix = (best->x1 - best->x0) * 0.2f, iy = (best->y1 - best->y0) * 0.2f;
    a.x = std::clamp(cx, best->x0 + ix, best->x1 - ix);
    a.y = std::clamp(cy, best->y0 + iy, best->y1 - iy);
    return a;
}

uint32_t nextLock(const std::vector<AimBox>& boxes, float cx, float cy, uint32_t current) {
    std::vector<std::pair<std::pair<float, float>, uint32_t>> order;
    for (const AimBox& b : boxes)
        if (b.id && b.x1 > b.x0 && b.y1 > b.y0) order.push_back({{gapTo(b, cx, cy), centreGap(b, cx, cy)}, b.id});
    if (order.empty()) return 0;
    std::sort(order.begin(), order.end());
    for (size_t i = 0; i < order.size(); ++i)
        if (order[i].second == current) return order[(i + 1) % order.size()].second;
    return order[0].second;
}

bool monsterClass(int cls) { return cls >= 1001 && cls <= 3999; }

namespace {
// The way's span in the box: [t0, t1] of its share; false: it misses the box
bool spanIn(const Box& b, const float from[3], const float to[3], float& t0, float& t1) {
    const float d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    const float o[3] = {from[0] - b.centre[0], from[1] - b.centre[1], from[2] - b.centre[2]};
    t0 = -1e30f, t1 = 1e30f;
    for (int i = 0; i < 3; ++i) {
        const float* a = b.axis[i];
        const float len = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
        if (!(len > 1e-6f) || !std::isfinite(b.half[i])) return false;
        const float half = std::fabs(b.half[i]) * len;  // the world's units
        const float at = (o[0] * a[0] + o[1] * a[1] + o[2] * a[2]) / len;
        const float v = (d[0] * a[0] + d[1] * a[1] + d[2] * a[2]) / len;
        if (std::fabs(v) < 1e-9f) {
            if (std::fabs(at) > half) return false;
            continue;
        }
        float e0 = (-half - at) / v, e1 = (half - at) / v;
        if (e0 > e1) std::swap(e0, e1);
        t0 = std::max(t0, e0);
        t1 = std::min(t1, e1);
        if (t0 > t1) return false;
    }
    return t1 >= 0.0f && t0 <= 1.0f;
}
}  // namespace

float entersAt(const Box& b, const float from[3], const float to[3]) {
    float t0 = 0, t1 = 0;
    if (!spanIn(b, from, to, t0, t1) || t0 <= 0.0f) return -1.0f;
    return t0;
}

bool crosses(const Box& b, const float from[3], const float to[3]) {
    float t0 = 0, t1 = 0;
    return spanIn(b, from, to, t0, t1);
}

float freeOfBoxes(const float eye[3], const float target[3], float distance, const std::vector<Box>& boxes) {
    if (!(distance > kPullNearest) || boxes.empty()) return distance;
    float dir[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
    if (!normalize(dir)) return distance;
    const float head[3] = {eye[0] + dir[0] * distance, eye[1] + dir[1] * distance, eye[2] + dir[2] * distance};
    float free = distance;
    for (const Box& b : boxes) {
        const float t = entersAt(b, head, eye);
        if (t > 0.0f) free = std::min(free, t * distance);
    }
    return free < distance ? std::max(kPullNearest, free - kPullStep) : distance;
}

float behindDistanceOf(float gameDistance) {
    if (!std::isfinite(gameDistance) || gameDistance <= 0.0f) return kBehindDistance;
    return std::clamp(gameDistance * (kBehindDistance / 300.0f), 22.0f, 160.0f);
}

void place(Mode m, const float feet[3], const float flat[3], const float up[3], float pitch, float eye[3],
           float target[3], float distance) {
    const float p = pitch * kRad;
    float f[3] = {flat[0] * std::cos(p) + kWorldUp[0] * std::sin(p), kWorldUp[1] * std::sin(p),
                  flat[2] * std::cos(p) + kWorldUp[2] * std::sin(p)};
    normalize(f);
    if (m == Mode::First) {
        for (int i = 0; i < 3; ++i) eye[i] = feet[i] + kWorldUp[i] * kEyeHeight;
    } else {
        float right[3];
        cross(up, f, right);  // the screen's right as the game's LookAt makes it
        if (!normalize(right)) right[0] = right[1] = right[2] = 0.0f;
        for (int i = 0; i < 3; ++i)
            eye[i] = feet[i] + kWorldUp[i] * kHeadHeight - f[i] * distance + right[i] * kShoulder;
    }
    for (int i = 0; i < 3; ++i) target[i] = eye[i] + f[i] * 10.0f;
}

Sky skyOf(float night, bool vault) {
    static const float kDay[3] = {0.76f, 0.83f, 0.90f}, kNight[3] = {0.09f, 0.11f, 0.17f};
    static const float kVault[3] = {0.075f, 0.068f, 0.062f};
    const float n = std::clamp(night, 0.0f, 1.0f);
    Sky s;
    for (int k = 0; k < 3; ++k) s.horizon[k] = vault ? kVault[k] : kDay[k] + (kNight[k] - kDay[k]) * n;
    s.reach = vault ? 650.0f : 1450.0f;
    return s;
}

void lookAt(const float eye[3], const float at[3], const float up[3], float out[12]) {
    float f[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
    normalize(f);
    float r[3];
    cross(up, f, r);
    normalize(r);
    float u[3];
    cross(f, r, u);
    normalize(u);
    out[0] = r[0];
    out[3] = r[1];
    out[6] = r[2];
    out[1] = u[0];
    out[4] = u[1];
    out[7] = u[2];
    out[2] = f[0];
    out[5] = f[1];
    out[8] = f[2];
    out[9] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    out[10] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    out[11] = -(f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
}

void inverse(const float m[12], float out[12]) {
    // as 0x409860: the 3 x 3's inverse by its cofactors, then the move taken back
    const float c0 = m[4] * m[8] - m[7] * m[5];
    const float det = (m[7] * m[3] - m[6] * m[4]) * m[2] + (c0 * m[0] - (m[3] * m[8] - m[6] * m[5]) * m[1]);
    const float k = std::fabs(det) > 1e-12f ? 1.0f / det : 0.0f;
    out[0] = c0 * k;
    out[1] = -((m[1] * m[8] - m[7] * m[2]) * k);
    out[2] = (m[1] * m[5] - m[2] * m[4]) * k;
    out[3] = -((m[3] * m[8] - m[6] * m[5]) * k);
    out[4] = (m[0] * m[8] - m[6] * m[2]) * k;
    out[5] = -((m[0] * m[5] - m[3] * m[2]) * k);
    out[6] = (m[7] * m[3] - m[6] * m[4]) * k;
    out[7] = -((m[7] * m[0] - m[6] * m[1]) * k);
    out[8] = (m[4] * m[0] - m[3] * m[1]) * k;
    out[9] = -(m[10] * out[3] + m[9] * out[0] + m[11] * out[6]);
    out[10] = -(m[9] * out[1] + out[4] * m[10] + m[11] * out[7]);
    out[11] = -(m[11] * out[8] + m[9] * out[2] + out[5] * m[10]);
}

}  // namespace uagfx::view3d
