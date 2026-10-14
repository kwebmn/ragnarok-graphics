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
        int best = -1;
        float closest = 4.0f;  // world units: a character's layers stand at its depth
        for (size_t b = 0; b < blobs.size(); ++b) {
            const Blob& s = blobs[b];
            const float off = std::fabs(l.viewZ - s.viewZ);
            if (std::fabs(middle - s.x) > std::max(s.halfW * 1.8f, 12.0f) || top >= s.y ||
                bottom > s.y + s.halfH + 4.0f || off > closest)
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
        q.texture = l.texture;
        out.push_back(q);
    }
    return out;
}

}  // namespace uagfx::actor_shadows
