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
        const auto over = [&](const Blob& s) {  // over its feet: across them, above them
            return std::fabs(middle - s.x) <= std::max(s.halfW * 2.2f, 16.0f) && top < s.y &&
                   bottom <= s.y + std::max(s.halfH * 2.5f, 20.0f);
        };
        const auto off = [&](const Blob& s) { return std::fabs(l.bottomZ + tilt * (s.y - bottom) - s.viewZ); };
        // 2.6.3: the layer of a character the game drew a shadow for (its seed) stands on that one's feet. The game
        // draws each clip nearer by the clips before it (0.8 units each at 300 away) and by the screen's height (4.7
        // units at 1080p): by depth two of a character's layers stood on its feet, the head lost at a turn that drew
        // the weapon first. Not a sprite over everything (the game's nearest depth).
        int best = -1;
        float closest = 0.0f;
        for (size_t b = 0; b < blobs.size() && l.owner; ++b) {
            const Blob& s = blobs[b];
            if (s.owner != l.owner || !over(s) || off(s) > 0.25f * s.viewZ || (best >= 0 && off(s) >= closest))
                continue;
            closest = off(s);
            best = int(b);
        }
        // Else - its character unknown or without its shadow there - the one at its depth.
        const bool owned = best >= 0;
        closest = 5.0f;  // world units: a character's layers stand at its depth
        for (size_t b = 0; b < blobs.size() && !owned; ++b) {
            const Blob& s = blobs[b];
            if (!over(s) || off(s) > closest) continue;
            closest = off(s);
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
        q.owned = owned;
        out.push_back(q);
    }
    return out;
}

}  // namespace uagfx::actor_shadows
