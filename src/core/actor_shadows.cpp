#include "core/actor_shadows.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::actor_shadows {

std::vector<ShadowQuad> cast(const std::vector<Blob>& blobs, const std::vector<Layer>& layers,
                             const std::vector<std::array<float, 2>>& dirs, const float dir[2], float length) {
    std::vector<ShadowQuad> out;
    for (const Layer& l : layers) {
        float x0 = l.corner[0][0], x1 = x0, top = l.corner[0][1], bottom = top;
        for (int i = 1; i < 4; ++i) {
            x0 = std::min(x0, l.corner[i][0]);
            x1 = std::max(x1, l.corner[i][0]);
            top = std::min(top, l.corner[i][1]);
            bottom = std::max(bottom, l.corner[i][1]);
        }
        const float middle = (x0 + x1) * 0.5f;
        // 0.39.1: the layer's depth at a given height on the screen, along its tilt (its mean was off by up to half
        // its height's tilt - tall characters got no shadow).
        const float tilt = bottom > top ? (l.bottomZ - l.topZ) / (bottom - top) : 0.0f;
        int best = -1;
        float closest = 5.0f;  // world units: a character's layers stand at its depth
        for (size_t b = 0; b < blobs.size(); ++b) {
            const Blob& s = blobs[b];
            const float off = std::fabs(l.bottomZ + tilt * (s.y - bottom) - s.viewZ);
            if (std::fabs(middle - s.x) > std::max(s.halfW * 2.2f, 16.0f) || top >= s.y ||
                bottom > s.y + std::max(s.halfH * 2.5f, 20.0f) || off > closest)
                continue;
            closest = off;
            best = int(b);
        }
        if (best < 0) continue;
        const Blob& s = blobs[size_t(best)];
        const float* d = dirs.size() == blobs.size() ? dirs[size_t(best)].data() : dir;
        ShadowQuad q;
        for (int i = 0; i < 4; ++i) {
            const float h = std::max(0.0f, s.y - l.corner[i][1]) * length;
            q.corner[i][0] = l.corner[i][0] + d[0] * h;
            q.corner[i][1] = s.y + d[1] * h;
            q.uv[i][0] = l.uv[i][0];
            q.uv[i][1] = l.uv[i][1];
        }
        q.alpha = l.alpha;
        q.viewZ = s.viewZ;
        q.feetX = s.x;
        q.feetY = s.y;
        q.texture = l.texture;
        out.push_back(q);
    }
    return out;
}

}  // namespace uagfx::actor_shadows
