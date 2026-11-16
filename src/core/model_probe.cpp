#include "core/model_probe.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::modelprobe {

float depthOf(const camera::Projection& p, float z) {
    const float a = p.farZ / (p.farZ - p.nearZ), b = p.farZ * p.nearZ / (p.farZ - p.nearZ);
    return a - b / z;
}

bool pointOf(const float a[3], const float b[3], const float c[3], const camera::Projection& p, float scale,
             const float jitter[2], int width, int height, Point& out) {
    const float* corner[3] = {a, b, c};
    float sx[3], sy[3];  // on the target
    for (int k = 0; k < 3; ++k) {
        const float z = corner[k][2];
        if (!(z > p.nearZ) || z > p.farZ - 10.0f) return false;
        sx[k] = scale * (corner[k][0] / z * p.f0 + p.cx) + 0.5f + jitter[0];
        sy[k] = scale * (corner[k][1] / z * p.f1 + p.cy) + 0.5f + jitter[1];
    }
    const float area = std::fabs((sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])) * 0.5f;
    float around = 0.0f;
    for (int k = 0; k < 3; ++k) around += std::hypot(sx[(k + 1) % 3] - sx[k], sy[(k + 1) % 3] - sy[k]);
    // a pixel well inside it wanted: its inscribed circle (twice the area over the way round) 1.5 pixels at least
    if (area < 12.0f || 2.0f * area / around < 1.5f) return false;
    const float mx = (sx[0] + sx[1] + sx[2]) / 3.0f, my = (sy[0] + sy[1] + sy[2]) / 3.0f;
    const int x = int(std::floor(mx)), y = int(std::floor(my));
    if (x < 1 || y < 1 || x >= width - 1 || y >= height - 1) return false;
    // the face's plane in the view: n . (P - a) = 0; a pixel's ray (rx, ry, 1) z meets it at z = (n . a) / (n . ray)
    const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    const float na = n[0] * a[0] + n[1] * a[1] + n[2] * a[2];
    float lo = 2.0f, hi = -1.0f;
    for (int k = 0; k < 4; ++k) {  // the pixel's corners: every sample of it lies between them
        const float px = float(x + (k & 1)), py = float(y + (k >> 1));
        const float rx = ((px - 0.5f - jitter[0]) / scale - p.cx) / p.f0;
        const float ry = ((py - 0.5f - jitter[1]) / scale - p.cy) / p.f1;
        const float along = n[0] * rx + n[1] * ry + n[2];
        if (std::fabs(along) < 1e-6f * (std::fabs(n[0]) + std::fabs(n[1]) + std::fabs(n[2])) || along * na <= 0.0f)
            return false;  // edge on
        const float z = na / along;
        if (!(z > p.nearZ) || z > p.farZ) return false;
        const float d = depthOf(p, z);
        lo = std::min(lo, d), hi = std::max(hi, d);
    }
    if (hi - lo > kMostSpan) return false;
    out.x = x, out.y = y;
    out.lo = lo, out.hi = hi;
    out.area = area;
    return true;
}

Seen judge(const Point& p, float depth) {
    if (depth < p.lo - kSlack) return Seen::Nearer;
    if (depth > p.hi + kSlack) return Seen::Farther;
    return Seen::There;
}

}  // namespace uagfx::modelprobe
