#include "core/cast_look.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::castlook {

void Looks::frame() {
    last_.swap(now_);
    now_.clear();
    given_.clear();
    if (feet_.size() > 4096) feet_.clear();
}

void Looks::note(const uint8_t* v, uint32_t count, uint32_t stride, uint32_t owner) {
    if (!v || stride < 32) return;
    for (uint32_t i = 0; i < count; ++i) {
        float f[4], uv[2];
        uint32_t diffuse;
        std::memcpy(f, v + size_t(i) * stride, 16);
        std::memcpy(&diffuse, v + size_t(i) * stride + 16, 4);
        std::memcpy(uv, v + size_t(i) * stride + 24, 8);
        if (uv[1] < 0.5f || float(diffuse >> 24) < kBaseAlpha) continue;
        Box& b = now_[owner];
        if (f[1] < b.y0) b.zTop = f[2], b.rhwTop = f[3];
        if (f[1] > b.y1) b.zBottom = f[2], b.rhwBottom = f[3];
        b.x0 = std::min(b.x0, f[0]);
        b.y0 = std::min(b.y0, f[1]);
        b.x1 = std::max(b.x1, f[0]);
        b.y1 = std::max(b.y1, f[1]);
        b.z += f[2];
        b.rhw += f[3];
        ++b.n;
        b.colour = diffuse;
    }
}

void Looks::shorten(uint8_t* v, uint32_t count, uint32_t stride, uint32_t owner, float most) {
    if (!v || stride < 32 || most <= 0) return;
    const auto keyOf = [&](float u) { return (uint64_t(owner) << 32) | uint32_t(std::lround(u * 1000.0f)); };
    // the feet first: a triangle holds the foot of a top beside it
    for (uint32_t i = 0; i < count; ++i) {
        float f[4], uv[2];
        uint32_t diffuse;
        std::memcpy(f, v + size_t(i) * stride, 16);
        std::memcpy(&diffuse, v + size_t(i) * stride + 16, 4);
        std::memcpy(uv, v + size_t(i) * stride + 24, 8);
        if (uv[1] < 0.5f) continue;
        feet_[keyOf(uv[0])] = Foot{f[0], f[1], f[2], f[3]};
        if (float(diffuse >> 24) >= kBaseAlpha) {
            Box& b = now_[owner];
            if (f[1] < b.y0) b.zTop = f[2], b.rhwTop = f[3];
            if (f[1] > b.y1) b.zBottom = f[2], b.rhwBottom = f[3];
            b.x0 = std::min(b.x0, f[0]);
            b.y0 = std::min(b.y0, f[1]);
            b.x1 = std::max(b.x1, f[0]);
            b.y1 = std::max(b.y1, f[1]);
            b.z += f[2];
            b.rhw += f[3];
            ++b.n;
            b.colour = diffuse;
        }
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t* p = v + size_t(i) * stride;
        float f[4], uv[2];
        std::memcpy(f, p, 16);
        std::memcpy(uv, p + 24, 8);
        if (uv[1] >= 0.5f) continue;
        const auto it = feet_.find(keyOf(uv[0]));
        if (it == feet_.end()) continue;
        const Foot& foot = it->second;
        const float up = foot.y - f[1];  // pixels over its foot
        if (up <= 1.0f) continue;
        const float held = most * (1.0f - std::exp(-up / most));
        const float t = held / up;
        f[0] = foot.x + (f[0] - foot.x) * t;
        f[1] = foot.y - held;
        f[2] = foot.z + (f[2] - foot.z) * t;
        f[3] = foot.rhw + (f[3] - foot.rhw) * t;
        std::memcpy(p, f, 16);
    }
    // .5: all of it kShrink about the feet walls' middle of the frame before (none yet: as it is)
    const auto it = last_.find(owner);
    if (it == last_.end() || it->second.n == 0) return;
    const float cx = (it->second.x0 + it->second.x1) * 0.5f, cy = (it->second.y0 + it->second.y1) * 0.5f;
    for (uint32_t i = 0; i < count; ++i) {
        float xy[2];
        std::memcpy(xy, v + size_t(i) * stride, 8);
        xy[0] = cx + (xy[0] - cx) * kShrink;
        xy[1] = cy + (xy[1] - cy) * kShrink;
        std::memcpy(v + size_t(i) * stride, xy, 8);
    }
}

bool Looks::circleQuad(uint32_t owner, uint8_t out[4 * 32]) {
    if (std::find(given_.begin(), given_.end(), owner) != given_.end()) return false;
    const auto it = last_.find(owner);
    if (it == last_.end() || it->second.n == 0) return false;
    given_.push_back(owner);
    const Box& b = it->second;
    const float cx = (b.x0 + b.x1) * 0.5f, cy = (b.y0 + b.y1) * 0.5f;
    const float rx = std::max(4.0f, (b.x1 - b.x0) * 0.5f * kCircleScale);
    const float ry = std::max(2.0f, (b.y1 - b.y0) * 0.5f * kCircleScale);
    // the depth along the ground's plane through the feet (affine in the screen's y), a little nearer than it
    const float span = b.y1 - b.y0;
    const auto at = [&](float y, float top, float bottom, float mean) {
        if (span < 1.0f) return mean;
        return top + (bottom - top) * (y - b.y0) / span;
    };
    const float zMean = float(b.z / b.n), rhwMean = float(b.rhw / b.n);
    const uint32_t diffuse = b.colour | 0xFF000000u, specular = 0xFF000000u;  // no fog
    const float corner[4][4] = {{cx - rx, cy - ry, 0, 0}, {cx + rx, cy - ry, 1, 0}, {cx - rx, cy + ry, 0, 1},
                                {cx + rx, cy + ry, 1, 1}};  // a strip
    std::memset(out, 0, 4 * 32);
    for (int i = 0; i < 4; ++i) {
        uint8_t* p = out + i * 32;
        const float z = std::max(0.0f, at(corner[i][1], b.zTop, b.zBottom, zMean) - 0.0004f);
        const float rhw = std::max(1e-6f, at(corner[i][1], b.rhwTop, b.rhwBottom, rhwMean));
        const float pos[4] = {corner[i][0], corner[i][1], z, rhw};
        std::memcpy(p, pos, 16);
        std::memcpy(p + 16, &diffuse, 4);
        std::memcpy(p + 20, &specular, 4);
        std::memcpy(p + 24, &corner[i][2], 8);
    }
    return true;
}

}  // namespace uagfx::castlook
