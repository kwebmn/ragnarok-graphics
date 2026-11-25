#include "core/view3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/game_camera.hpp"

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

Held hold(Mode mode, bool free, uint32_t world, const float fields[6]) {
    Held h;
    if (mode == Mode::Off) return h;
    h.on = true;
    h.mode = mode;
    h.free = free;
    h.world = world;
    std::copy(fields, fields + 6, h.fields);
    return h;
}

bool resumesHeld(const Held& h, Mode wanted, uint32_t world) {
    return h.on && wanted == h.mode && world == h.world;
}

Mode next(Mode m) {
    switch (m) {
        case Mode::Off: return Mode::Behind;
        case Mode::Behind: return Mode::First;
        default: return Mode::Off;
    }
}

Mode pressed(Mode now, Mode button) { return button == Mode::Off || now == button ? Mode::Off : button; }

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

uint32_t nextLock(const std::vector<AimBox>& boxes, float cx, float cy, uint32_t current, int dir) {
    std::vector<std::pair<std::pair<float, float>, uint32_t>> order;
    for (const AimBox& b : boxes)
        if (b.id && b.x1 > b.x0 && b.y1 > b.y0) order.push_back({{gapTo(b, cx, cy), centreGap(b, cx, cy)}, b.id});
    if (order.empty()) return 0;
    std::sort(order.begin(), order.end());
    for (size_t i = 0; i < order.size(); ++i)
        if (order[i].second == current) return order[(i + (dir < 0 ? order.size() - 1 : 1)) % order.size()].second;
    return order[0].second;
}

bool monsterClass(int cls) { return cls >= 1001 && cls <= 3999; }

uint32_t skillTargetOf(int wait, uint32_t pointed, int pointedClass, uint32_t aimed, uint32_t own, bool shift) {
    const bool monster = pointed && monsterClass(pointedClass);
    const bool fixed = pointed && pointedClass >= 45 && pointedClass < 1000;  // the warps (45) and the NPCs
    const bool person = pointed && !monster && !fixed && pointedClass >= 0;  // a player, a homunculus, a mercenary
    if (wait == 2) return monster || (person && pointed != own) ? pointed : aimed;
    if (wait != 4) return 0;
    if (monster && shift) return pointed;
    return person ? pointed : own;
}

bool companionSkill(int skill) { return (skill >= 8000 && skill <= 8016) || (skill >= 8200 && skill <= 8237); }

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

bool boxHides(const Box& b, const float eye[3], const float to[3]) {
    float t0 = 0, t1 = 0;
    if (!spanIn(b, eye, to, t0, t1)) return false;  // missed
    // 3.0.23-beta.8 (R-8R7XNK): the eye in a wall's box (a thin, tall slab - the eye pressed into the wall): what is
    // out past it hidden; the eye in a fat box (a tree's crown, a house's coarse box) hides nothing
    if (t0 <= 0.0f)
        return t1 < 1.0f && std::min(b.half[0], b.half[2]) <= kThinWallHalf && b.half[1] >= kThinWallLeastHalfHigh;
    // the way through its core (kHideShare of its half sizes) and out of it before the place - 3.0.23-beta.9
    // (R-M9NN4J): the place may lie in the box's margin past the core (a monster right behind a post); a place in the
    // core (inside a house's box) is not hidden by it
    Box core = b;
    for (float& h : core.half) h *= kHideShare;
    float c0 = 0, c1 = 0;
    return spanIn(core, eye, to, c0, c1) && c0 > 0.0f && c1 < 1.0f;
}

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

float behindDistanceOf(float gameDistance, float atEntry) {
    if (!std::isfinite(gameDistance) || gameDistance <= 0.0f || !std::isfinite(atEntry) || atEntry <= 0.0f)
        return kBehindDistance;
    return std::clamp(kBehindDistance * gameDistance / atEntry, kBehindNearest, kBehindFarthest);
}

void place(Mode m, const float feet[3], const float flat[3], const float up[3], float pitch, float eye[3],
           float target[3], float distance, bool shoulder) {
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
        const float over = kHeadHeight + (shoulder ? 0.0f : kCentreRise), aside = shoulder ? kShoulder : 0.0f;
        for (int i = 0; i < 3; ++i) eye[i] = feet[i] + kWorldUp[i] * over - f[i] * distance + right[i] * aside;
    }
    for (int i = 0; i < 3; ++i) target[i] = eye[i] + f[i] * 10.0f;
}

bool orbitStep(Orbit& o, Look& look, bool shift, uint32_t now) {
    if (shift && !o.on) {
        if (!o.back) o.home = look;  // on its way back: the same home
        o.on = true;
        o.back = false;
    } else if (!shift && o.on) {
        o.on = false;
        o.back = true;
        o.leftAt = look;
        o.backSince = now;
    }
    if (!o.back) return o.on;
    const float t = float(now - o.backSince) / float(kOrbitBackMs);
    if (t >= 1.0f) {
        look = o.home;
        o.back = false;
        return false;
    }
    const float k = t <= 0.0f ? 0.0f : t * t * (3.0f - 2.0f * t);
    float turn = std::fmod(o.home.yaw - o.leftAt.yaw, 360.0f);  // the shorter way round
    if (turn > 180.0f) turn -= 360.0f;
    if (turn <= -180.0f) turn += 360.0f;
    const float yaw = std::fmod(o.leftAt.yaw + turn * k, 360.0f);
    look.yaw = yaw < 0.0f ? yaw + 360.0f : yaw;
    look.pitch = o.leftAt.pitch + (o.home.pitch - o.leftAt.pitch) * k;
    return true;
}

float spriteDepth(float z, float rhw) {
    if (!(rhw > 0.0f) || z < 0.0f) return z;
    const float viewZ = 1.0f / rhw;
    const float v = std::max(viewZ - std::min(kSpriteNearer * viewZ, kSpriteNearerMost), camera::kNear);
    return std::max(0.0f, camera::kFar / (camera::kFar - camera::kNear) * (1.0f - camera::kNear / v));
}

bool overAll(const void* vertices, uint32_t count, size_t stride, float gameStep) {
    if (!vertices || !count || stride < 4 * sizeof(float)) return false;
    const auto* p = static_cast<const uint8_t*>(vertices);
    const bool sane = std::isfinite(gameStep) && gameStep > 1.0e-8f && gameStep < 1.0e-3f;
    const double top = 1.0 / double(kOnTopNear), step = double(sane ? gameStep : kOnTopStep);
    for (uint32_t i = 0; i < count; ++i) {
        float rhw = 0;
        std::memcpy(&rhw, p + i * stride + 3 * sizeof(float), sizeof rhw);
        // its steps from the near plane's own rhw: -kOnTopBack for a first clip on the layer 0; past 0 it is nearer
        // than the near plane (the card cuts it)
        const double steps = (double(rhw) - top) / step, whole = std::round(steps);
        if (!(std::fabs(steps - whole) <= kOnTopOff)) return false;  // between the game's steps; no number at all
        if (whole > 0.0 || whole < -double(kOnTopBack + kOnTopLayers)) return false;
    }
    return true;
}

bool effectAtTheEyes(const void* vertices, uint32_t count, size_t stride, bool own) {
    if (!vertices || stride < 4 * sizeof(float)) return false;
    const auto* p = static_cast<const uint8_t*>(vertices);
    double sum = 0;
    uint32_t known = 0;
    for (uint32_t i = 0; i < count; ++i) {
        float rhw = 0;
        std::memcpy(&rhw, p + i * stride + 3 * sizeof(float), sizeof rhw);
        if (rhw > 0.0f && std::isfinite(rhw)) sum += 1.0 / rhw, ++known;
    }
    if (!known) return false;
    return float(sum / known) < (own ? kOwnEffectNear : kAnyEffectNear);
}

void keepOffBoxes(const float from[3], float to[3], const std::vector<Box>& boxes) {
    float at[3] = {from[0], from[1], from[2]};
    for (int axis = 0; axis < 3; ++axis) {
        const float d = to[axis] - at[axis];
        if (!(std::fabs(d) > 1e-6f)) continue;
        const float way = std::fabs(d), sign = d > 0.0f ? 1.0f : -1.0f;
        float probe[3] = {at[0], at[1], at[2]};
        probe[axis] += sign * (way + kFreeGap);
        float allowed = way;
        for (const Box& b : boxes) {
            const float share = entersAt(b, at, probe);
            if (share > 0.0f) allowed = std::min(allowed, std::max(0.0f, share * (way + kFreeGap) - kFreeGap));
        }
        at[axis] += sign * allowed;
    }
    std::copy(at, at + 3, to);
}

namespace {
// (x, z)'s weights in the triangle a b c laid flat (x and z); false: outside it
bool weightsIn(const float* a, const float* b, const float* c, float x, float z, float w[3]) {
    const float d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2]);
    if (std::fabs(d) < 1e-6f) return false;
    w[0] = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / d;
    w[1] = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / d;
    w[2] = 1.0f - w[0] - w[1];
    constexpr float kEdge = -1e-4f;
    return w[0] >= kEdge && w[1] >= kEdge && w[2] >= kEdge;
}
}  // namespace

bool heightOnFace(const float corners[4][3], float x, float z, float& y) {
    static const int kTriangles[2][3] = {{0, 1, 2}, {1, 3, 2}};
    for (const auto& t : kTriangles) {
        const float *a = corners[t[0]], *b = corners[t[1]], *c = corners[t[2]];
        float w[3];
        if (!weightsIn(a, b, c, x, z, w)) continue;
        y = w[0] * a[1] + w[1] * b[1] + w[2] * c[1];
        return std::isfinite(y);
    }
    return false;
}

bool cellOf(int wide, int high, float zoom, float x, float z, bool flipped, int& col, int& row) {
    if (wide <= 0 || high <= 0 || !(zoom > 0.0f)) return false;
    const float c = std::floor((x + float(wide) * zoom * 0.5f) / zoom);
    const float r = std::floor((z + float(high) * zoom * 0.5f) / zoom);
    if (!(c >= 0.0f && c < float(wide) && r >= 0.0f && r < float(high))) return false;
    col = int(c);
    row = flipped ? high - 1 - int(r) : int(r);
    return true;
}

void keepOverGround(const float from[3], float to[3], const std::function<float(float x, float z)>& groundY) {
    float at[3] = {from[0], to[1], from[2]};
    for (const int axis : {0, 2}) {
        float next[3] = {at[0], at[1], at[2]};
        next[axis] = to[axis];
        if (next[axis] == at[axis]) continue;
        const float g = groundY(next[0], next[2]);
        if (std::isfinite(g) && g < next[1] - kFreeClimb) continue;  // rock over the eye there (y down)
        at[axis] = next[axis];
    }
    std::copy(at, at + 3, to);
}

int serverDir(float angle) { return ((int(angle) + 180) / 45) & 7; }

bool faceToward(float east, float north, float current, Facing& out) {
    if (!(std::hypot(east, north) > 1e-3f) || !std::isfinite(current)) return false;
    float a = std::atan2(east, -north) / kRad;
    if (a < 0.0f) a += 360.0f;
    float off = std::fmod(a - current, 360.0f);
    if (off > 180.0f) off -= 360.0f;
    if (off < -180.0f) off += 360.0f;
    if (std::fabs(off) <= kFaceKeep) return false;
    float step = std::round(a / 45.0f) * 45.0f;
    if (step >= 360.0f) step -= 360.0f;
    float from = std::fmod(step - current, 360.0f);
    if (from < 0.0f) from += 360.0f;
    if (from < 0.5f || from > 359.5f) return false;  // that way already
    out.angle = step;
    out.dir = serverDir(step);
    return true;
}

bool groundLetsSee(const float eye[3], const float to[3], const std::function<float(float x, float z)>& groundY) {
    const float d[3] = {to[0] - eye[0], to[1] - eye[1], to[2] - eye[2]};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(len > 2.0f * kSightEnds)) return true;
    for (float t = kSightEnds; t <= len - kSightEnds; t += kSightStep) {
        const float k = t / len, x = eye[0] + d[0] * k, y = eye[1] + d[1] * k, z = eye[2] + d[2] * k;
        const float g = groundY(x, z);
        if (std::isfinite(g) && g < y - kSightMargin) return false;  // the ground over the way (y down)
    }
    return true;
}

void axesOf(const float flat[3], const float up[3], float pitch, float f[3], float right[3]) {
    const float p = pitch * kRad;
    f[0] = flat[0] * std::cos(p) + kWorldUp[0] * std::sin(p);
    f[1] = kWorldUp[1] * std::sin(p);
    f[2] = flat[2] * std::cos(p) + kWorldUp[2] * std::sin(p);
    normalize(f);
    cross(up, f, right);  // the screen's right as the game's LookAt makes it
    if (!normalize(right)) right[0] = right[1] = right[2] = 0.0f;
}

void flyStep(float eye[3], const float f[3], const float right[3], int ahead, int aside, int rise, float seconds,
             bool fast, const float feet[3]) {
    if (!ahead && !aside && !rise) return;
    const float v = kFreeSpeed * (fast ? kFreeFast : 1.0f) * std::max(seconds, 0.0f);
    for (int i = 0; i < 3; ++i)
        eye[i] += (f[i] * float(ahead) + right[i] * float(aside) + kWorldUp[i] * float(rise)) * v;
    float d[3] = {eye[0] - feet[0], eye[1] - feet[1], eye[2] - feet[2]};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len > kFreeReach)
        for (int i = 0; i < 3; ++i) eye[i] = feet[i] + d[i] / len * kFreeReach;
}

namespace {

void keepWithin(float eye[3], const float feet[3]) {
    float d[3] = {eye[0] - feet[0], eye[1] - feet[1], eye[2] - feet[2]};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len > kFreeReach)
        for (int i = 0; i < 3; ++i) eye[i] = feet[i] + d[i] / len * kFreeReach;
}

float shortWay(float from, float to) {  // degrees, -180..180
    float d = std::fmod(to - from, 360.0f);
    if (d > 180.0f) d -= 360.0f;
    if (d < -180.0f) d += 360.0f;
    return d;
}

float catmull(float p0, float p1, float p2, float p3, float u) {
    return 0.5f * (2.0f * p1 + (p2 - p0) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u +
                   (3.0f * p1 - p0 - 3.0f * p2 + p3) * u * u * u);
}

// A point of the curve: segment i (from point i to i + 1) at u; yaws unwrapped along the points
Waypoint curveAt(const std::vector<Waypoint>& p, const std::vector<float>& yaws, size_t i, float u) {
    const size_t n = p.size();
    const size_t i0 = i > 0 ? i - 1 : 0, i2 = std::min(i + 1, n - 1), i3 = std::min(i + 2, n - 1);
    Waypoint w;
    for (int k = 0; k < 3; ++k) w.eye[k] = catmull(p[i0].eye[k], p[i].eye[k], p[i2].eye[k], p[i3].eye[k], u);
    w.yaw = catmull(yaws[i0], yaws[i], yaws[i2], yaws[i3], u);
    w.pitch = catmull(p[i0].pitch, p[i].pitch, p[i2].pitch, p[i3].pitch, u);
    w.pitch = std::clamp(w.pitch, -kPitchMost, kPitchMost);
    return w;
}

constexpr int kFlightSamples = 24;  // a segment's steps for its length

struct Track {
    std::vector<float> yaws;  // unwrapped
    std::vector<float> at;    // the length so far at each sample (segment * kFlightSamples + step)
    float total = 0.0f;
};

Track trackOf(const std::vector<Waypoint>& p) {
    Track t;
    t.yaws.resize(p.size());
    for (size_t i = 0; i < p.size(); ++i)
        t.yaws[i] = i == 0 ? p[0].yaw : t.yaws[i - 1] + shortWay(t.yaws[i - 1], p[i].yaw);
    if (p.size() < 2) return t;
    Waypoint last = p[0];
    t.at.push_back(0.0f);
    for (size_t i = 0; i + 1 < p.size(); ++i)
        for (int s = 1; s <= kFlightSamples; ++s) {
            const Waypoint w = curveAt(p, t.yaws, i, float(s) / float(kFlightSamples));
            const float dx = w.eye[0] - last.eye[0], dy = w.eye[1] - last.eye[1], dz = w.eye[2] - last.eye[2];
            const float turn = std::max(std::fabs(w.yaw - last.yaw), std::fabs(w.pitch - last.pitch));
            t.total += std::max(std::sqrt(dx * dx + dy * dy + dz * dz), turn * kFlightDegreeUnits);
            t.at.push_back(t.total);
            last = w;
        }
    return t;
}

float eased(float x) {  // the share of the way at the share of the time: a steady pace, its ends eased
    constexpr float r = kFlightRamp;
    x = std::clamp(x, 0.0f, 1.0f);
    if (x < r) return x * x / (2.0f * r * (1.0f - r));
    if (x > 1.0f - r) return 1.0f - (1.0f - x) * (1.0f - x) / (2.0f * r * (1.0f - r));
    return (x - r / 2.0f) / (1.0f - r);
}

}  // namespace

void glideStep(float eye[3], Glide& g, const float f[3], const float right[3], int ahead, int aside, int rise,
               float seconds, bool fast, float scale, bool smooth, const float feet[3]) {
    seconds = std::max(seconds, 0.0f);
    const float v = kFreeSpeed * (fast ? kFreeFast : 1.0f) * std::max(scale, 0.0f);
    float want[3];
    for (int i = 0; i < 3; ++i)
        want[i] = (f[i] * float(ahead) + right[i] * float(aside) + kWorldUp[i] * float(rise)) * v;
    const float k = smooth ? 1.0f - std::exp(-seconds / kGlideEase) : 1.0f;
    for (int i = 0; i < 3; ++i) g.v[i] += (want[i] - g.v[i]) * k;
    if (!ahead && !aside && !rise && std::fabs(g.v[0]) + std::fabs(g.v[1]) + std::fabs(g.v[2]) < 0.05f)
        g.v[0] = g.v[1] = g.v[2] = 0.0f;  // come to rest
    for (int i = 0; i < 3; ++i) eye[i] += g.v[i] * seconds;
    keepWithin(eye, feet);
}

void easeLook(Look& shown, const Look& wanted, float seconds) {
    const float k = 1.0f - std::exp(-std::max(seconds, 0.0f) / kLookEase);
    float yaw = std::fmod(shown.yaw + shortWay(shown.yaw, wanted.yaw) * k, 360.0f);
    shown.yaw = yaw < 0.0f ? yaw + 360.0f : yaw;
    shown.pitch += (wanted.pitch - shown.pitch) * k;
}

float flightSeconds(const std::vector<Waypoint>& points, float speed) {
    if (points.size() < 2 || !(speed > 0.0f)) return 0.0f;
    return std::max(trackOf(points).total / (speed * (1.0f - kFlightRamp)), 0.5f);
}

FlightPose flightAt(const std::vector<Waypoint>& points, float seconds, float speed) {
    FlightPose pose;
    if (points.empty()) {
        pose.done = true;
        return pose;
    }
    const Waypoint& end = points.back();
    const float whole = flightSeconds(points, speed);
    if (points.size() < 2 || whole <= 0.0f || seconds >= whole) {
        std::copy(end.eye, end.eye + 3, pose.eye);
        pose.yaw = end.yaw;
        pose.pitch = end.pitch;
        pose.done = true;
        return pose;
    }
    const Track t = trackOf(points);
    const float want = eased(seconds / whole) * t.total;
    size_t s = 1;
    while (s + 1 < t.at.size() && t.at[s] < want) ++s;
    const float span = t.at[s] - t.at[s - 1];
    const float step = float(s - 1) + (span > 1e-6f ? (want - t.at[s - 1]) / span : 1.0f);
    const size_t seg = std::min(size_t(step) / kFlightSamples, points.size() - 2);
    const float u = std::clamp((step - float(seg * kFlightSamples)) / float(kFlightSamples), 0.0f, 1.0f);
    const Waypoint w = curveAt(points, t.yaws, seg, u);
    std::copy(w.eye, w.eye + 3, pose.eye);
    const float yaw = std::fmod(w.yaw, 360.0f);
    pose.yaw = yaw < 0.0f ? yaw + 360.0f : yaw;
    pose.pitch = w.pitch;
    return pose;
}

bool ownOut(Mode m, float behind, bool outBefore) {
    if (m == Mode::First) return true;
    if (m != Mode::Behind || !(behind > 0.0f)) return false;
    return behind < (outBefore ? kOwnShownAgain : kOwnNear);
}

float hazeReach(float skyReach, float sight) {
    return std::max(std::min(skyReach, sight - kHazeWholePast), kHazeWholePast);
}

Ranges rangesFor(bool wanted, bool outdoors, bool hung, bool sea, bool sand, bool snow) {
    if (!wanted || !outdoors || hung) return Ranges::None;
    if (sea) return Ranges::Low;
    if (snow) return Ranges::Snow;
    return sand ? Ranges::Dunes : Ranges::Mountains;
}

bool hungMap(const std::string& name) {
    // Yuno and Gonryun stand on rocks in the sky, the airships fly, the novices' second ground and the Schwarzwald
    // guilds' hall hang in nothing (outskirts' own count of their open outer band)
    static const char* const kHung[] = {"yuno", "gonryun", "airplane", "airplane_01", "sch_gld", "new_zone02",
                                        "new_1-2",  "new_2-2", "new_3-2",  "new_4-2",    "new_5-2"};
    for (const char* hung : kHung)
        if (name == hung) return true;
    return false;
}

void skySun(int minute, const float mapWay[3], float out[3]) {
    for (int k = 0; k < 3; ++k) out[k] = mapWay[k];
    const float len = std::sqrt(mapWay[0] * mapWay[0] + mapWay[1] * mapWay[1] + mapWay[2] * mapWay[2]);
    if (minute < 0 || len < 1e-4f) return;
    const float n[3] = {mapWay[0] / len, mapWay[1] / len, mapWay[2] / len};  // noon's
    const float flat = std::sqrt(n[0] * n[0] + n[2] * n[2]);
    // where it rises: on the horizon, a quarter turn from noon's way (and so square to it)
    const float r[3] = {flat > 1e-4f ? n[2] / flat : 1.0f, 0.0f, flat > 1e-4f ? -n[0] / flat : 0.0f};
    constexpr float kPi = 3.14159265f;
    const float h = float(minute % 1440) / 60.0f;
    // along the circle: 0 rising (6:00), a quarter at noon (13:00), half setting (20:00); the night's ten hours the
    // other half, under the horizon
    const float a = h >= 6.0f && h <= 20.0f ? kPi * (h - 6.0f) / 14.0f
                                            : kPi + kPi * ((h < 6.0f ? h + 24.0f : h) - 20.0f) / 10.0f;
    const float c = std::cos(a), s = std::sin(a);
    for (int k = 0; k < 3; ++k) out[k] = (c * r[k] + s * n[k]) * len;
}

void lightWay(const float sun[3], float out[3]) {
    const bool under = sun[1] > 0.0f;  // the world's y down
    for (int k = 0; k < 3; ++k) out[k] = under ? -sun[k] : sun[k];
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
