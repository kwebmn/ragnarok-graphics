#include "core/game_camera.hpp"

#include <cmath>
#include <cstring>

namespace uagfx::camera {

bool knownConstants(const float v[5]) {
    return v[0] == kPi && v[1] == kFar && v[2] == kNear && v[3] == kFov && v[4] == kFov;
}

Projection projectionFor(int width, int height) {
    Projection p;
    if (width <= 0 || height <= 0) return p;
    const int halfW = width / 2;
    const int halfH = height / 2;
    // SetSize: tan(pi * fov / 360) in the FPU, then a float per step.
    const double t = std::tan(double(kPi) * double(kFov) * double(1.0f / 360.0f));
    const float aspect = float(height) / float(width);
    p.f0 = aspect * float(double(halfW) / t);
    p.f1 = float(double(halfH) / t);
    p.cx = float(halfW);
    p.cy = float(halfH);
    return p;
}

bool plausibleView(const float m[12]) {
    for (int i = 0; i < 12; ++i)
        if (!std::isfinite(m[i]) || std::fabs(m[i]) > 1e7f) return false;
    for (int r = 0; r < 3; ++r) {
        const float* a = m + 3 * r;
        if (std::fabs(a[0] * a[0] + a[1] * a[1] + a[2] * a[2] - 1.0f) > 1e-3f) return false;
        for (int s = r + 1; s < 3; ++s) {
            const float* b = m + 3 * s;
            if (std::fabs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) > 1e-3f) return false;
        }
    }
    return true;
}

void affine4(const float m[12], float out[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) out[4 * r + c] = m[3 * r + c];
        out[4 * r + 3] = r == 3 ? 1.0f : 0.0f;
    }
}

void multiply4(const float a[16], const float b[16], float out[16]) {
    float t[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            double s = 0;
            for (int k = 0; k < 4; ++k) s += double(a[4 * r + k]) * double(b[4 * k + c]);
            t[4 * r + c] = float(s);
        }
    std::memcpy(out, t, sizeof t);
}

bool inverse4(const float m[16], float out[16]) {
    double a[4][8];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 8; ++c) a[r][c] = c < 4 ? double(m[4 * r + c]) : (c - 4 == r ? 1.0 : 0.0);
    for (int c = 0; c < 4; ++c) {
        int pivot = c;
        for (int r = c + 1; r < 4; ++r)
            if (std::fabs(a[r][c]) > std::fabs(a[pivot][c])) pivot = r;
        if (std::fabs(a[pivot][c]) < 1e-12) return false;
        if (pivot != c)
            for (int k = 0; k < 8; ++k) std::swap(a[c][k], a[pivot][k]);
        const double d = a[c][c];
        for (int k = 0; k < 8; ++k) a[c][k] /= d;
        for (int r = 0; r < 4; ++r) {
            if (r == c) continue;
            const double f = a[r][c];
            if (f != 0)
                for (int k = 0; k < 8; ++k) a[r][k] -= f * a[c][k];
        }
    }
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out[4 * r + c] = float(a[r][c + 4]);
    return true;
}

void reprojection(const float cur[12], const float prev[12], float out[12]) {
    float c[16], p[16], ic[16], r[16];
    affine4(cur, c);
    affine4(prev, p);
    if (!inverse4(c, ic)) {  // not a camera: no motion
        static const float identity[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        std::memcpy(out, identity, sizeof identity);
        return;
    }
    multiply4(ic, p, r);
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col) out[3 * row + col] = r[4 * row + col];
}

void reproject(const Projection& p, const float r[12], float sx, float sy, float sz, float& px, float& py) {
    px = sx;
    py = sy;
    const float zc = p.farZ * p.nearZ / (p.farZ - sz * (p.farZ - p.nearZ));
    if (!(zc > 0.0f) || p.f0 == 0.0f || p.f1 == 0.0f) return;
    const float xc = (sx - p.cx) * zc / p.f0;
    const float yc = (sy - p.cy) * zc / p.f1;
    const float x = xc * r[0] + yc * r[3] + zc * r[6] + r[9];
    const float y = xc * r[1] + yc * r[4] + zc * r[7] + r[10];
    const float z = xc * r[2] + yc * r[5] + zc * r[8] + r[11];
    if (!(z > 1e-4f)) return;  // behind the previous camera: no better guess than staying
    px = x / z * p.f0 + p.cx;
    py = y / z * p.f1 + p.cy;
}

void viewToClip(const Projection& p, int width, int height, float out[16]) {
    const float hw = float(width) * 0.5f;
    const float hh = float(height) * 0.5f;
    const float q = p.farZ / (p.farZ - p.nearZ);
    const float m[16] = {p.f0 / hw,         0.0f,                0.0f,         0.0f,  //
                         0.0f,              -p.f1 / hh,          0.0f,         0.0f,  //
                         (p.cx - hw) / hw,  -(p.cy - hh) / hh,   q,            1.0f,  //
                         0.0f,              0.0f,                -p.nearZ * q, 0.0f};
    std::memcpy(out, m, sizeof m);
}

}  // namespace uagfx::camera
