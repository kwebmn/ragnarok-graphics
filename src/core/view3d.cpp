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

float fovFor(Mode m) {
    switch (m) {
        case Mode::Behind: return kBehindFov;
        case Mode::First: return kFirstFov;
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

void place(Mode m, const float feet[3], const float flat[3], const float up[3], float pitch, float eye[3],
           float target[3]) {
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
            eye[i] = feet[i] + kWorldUp[i] * kHeadHeight - f[i] * kBehindDistance + right[i] * kShoulder;
    }
    for (int i = 0; i < 3; ++i) target[i] = eye[i] + f[i] * 10.0f;
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
