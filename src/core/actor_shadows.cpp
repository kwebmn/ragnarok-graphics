#include "core/actor_shadows.hpp"

#include <algorithm>
#include <cmath>

#include "core/torch.hpp"

namespace uagfx::actor_shadows {

std::vector<ShadowQuad> cast(const std::vector<Blob>& blobs, const std::vector<Layer>& layers,
                             const std::vector<std::array<float, 2>>& dirs, const float dir[2], float length) {
    std::vector<std::vector<Cast>> casts(blobs.size());
    for (size_t b = 0; b < blobs.size(); ++b) {
        const float* d = dirs.size() == blobs.size() ? dirs[b].data() : dir;
        casts[b].push_back(Cast{{d[0], d[1]}, length, 1.0f});
    }
    return cast(blobs, layers, casts);
}

std::vector<ShadowQuad> cast(const std::vector<Blob>& blobs, const std::vector<Layer>& layers,
                             const std::vector<std::vector<Cast>>& casts) {
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
        if (best < 0 || size_t(best) >= casts.size()) continue;
        const Blob& s = blobs[size_t(best)];
        for (const Cast& c : casts[size_t(best)]) {
            ShadowQuad q;
            for (int i = 0; i < 4; ++i) {
                const float h = std::max(0.0f, s.y - l.corner[i][1]) * c.length;
                q.corner[i][0] = l.corner[i][0] + c.dir[0] * h;
                q.corner[i][1] = s.y + c.dir[1] * h;
                q.uv[i][0] = l.uv[i][0];
                q.uv[i][1] = l.uv[i][1];
            }
            q.alpha = l.alpha * c.weight;
            q.blob = best;
            q.weight = c.weight;
            q.viewZ = s.viewZ;
            q.feetX = s.x;
            q.feetY = s.y;
            q.texture = l.texture;
            q.owned = owned;
            out.push_back(q);
        }
    }
    return out;
}

std::vector<Lit> litBy(const std::vector<Light>& lights, float feetX, float feetY, float feetZ, double time,
                       float flicker) {
    struct Near {
        const Light* light;
        float distance;
    };
    std::vector<Near> near;
    for (const Light& l : lights) {
        const float d = std::hypot(feetX - l.x, feetZ - l.z);
        if (d > 1.0f && d < l.reach) near.push_back(Near{&l, d});
    }
    std::sort(near.begin(), near.end(), [](const Near& a, const Near& b) { return a.distance < b.distance; });
    if (near.size() > 2) near.resize(2);
    std::vector<Lit> out;
    for (const Near& n : near) {
        const Light& l = *n.light;
        const float height = std::clamp(feetY - l.y, 4.0f, 40.0f);  // up along -y
        const float t = std::clamp((n.distance - l.reach / 3.0f) / (l.reach * 2.0f / 3.0f), 0.0f, 1.0f);
        float weight = 1.0f - t * t * (3.0f - 2.0f * t);
        if (l.seed) weight *= torch::flicker(time, l.seed, flicker);
        Lit lit;
        lit.awayX = (feetX - l.x) / n.distance;
        lit.awayZ = (feetZ - l.z) / n.distance;
        lit.length = std::clamp(n.distance / height, 0.35f, 2.2f);
        lit.weight = std::clamp(weight, 0.0f, 1.0f);
        out.push_back(lit);
    }
    return out;
}

std::vector<float> spotShares(const std::vector<ShadowQuad>& quads, size_t blobs, bool spot) {
    std::vector<float> out(blobs, 1.0f);
    if (spot) return out;
    for (const ShadowQuad& q : quads)
        if (q.blob >= 0 && size_t(q.blob) < blobs)
            out[size_t(q.blob)] = std::min(out[size_t(q.blob)], 1.0f - std::clamp(q.weight * 3.0f, 0.0f, 1.0f));
    return out;
}

std::vector<Light> lightsOf(const rsw::World& map, const std::vector<rsw::Light>& lamps, const map::Axes& axes) {
    std::vector<Light> out;
    for (const rsw::Effect& e : map.effects)
        if (e.id == 47 || e.id == 690 || e.id == 691 || e.id == 696) {
            const map::Point p = map::toWorld(axes, map::Point{e.pos[0], e.pos[1], e.pos[2]});
            out.push_back(Light{p.x, p.y, p.z, 40.0f, torch::seedAt(p.x, p.z) | 1u});
        }
    for (const rsw::Light& r : lamps) {
        const map::Point p = map::toWorld(axes, map::Point{r.pos[0], r.pos[1], r.pos[2]});
        out.push_back(Light{p.x, p.y, p.z, std::clamp(r.range * 0.45f, 15.0f, 70.0f), 0});
    }
    return out;
}

std::vector<std::vector<Cast>> castsFor(const std::vector<Blob>& blobs, const std::vector<Light>& lights,
                                        const camera::Projection& p, const float viewToWorld[12], bool known,
                                        const float sun[2], float sunLength, bool openSky, float night, float base,
                                        double time, float flicker) {
    const float dark = std::clamp(night, 0.0f, 1.0f);
    const float sunWeight = openSky ? 1.0f - dark : 0.0f, fireWeight = openSky ? dark : 1.0f;
    const float* m = viewToWorld;
    std::vector<std::vector<Cast>> out(blobs.size());
    for (size_t i = 0; i < blobs.size(); ++i) {
        const Blob& b = blobs[i];
        if (known && fireWeight > 0.01f && !lights.empty() && p.f0 > 0.0f && p.f1 > 0.0f) {
            const float vz = b.viewZ, vx = (b.x - p.cx) * vz / p.f0, vy = (b.y - p.cy) * vz / p.f1;
            const float fx = vx * m[0] + vy * m[3] + vz * m[6] + m[9], fy = vx * m[1] + vy * m[4] + vz * m[7] + m[10];
            const float fz = vx * m[2] + vy * m[5] + vz * m[8] + m[11];
            for (const Lit& l : litBy(lights, fx, fy, fz, time, flicker)) {
                const float weight = l.weight * fireWeight;
                if (weight < 0.01f) continue;
                // the feet and a point five units away from the light, on the screen
                const float dx = fx + l.awayX * 5.0f - m[9], dy = fy - m[10], dz = fz + l.awayZ * 5.0f - m[11];
                const float px = dx * m[0] + dy * m[1] + dz * m[2], py = dx * m[3] + dy * m[4] + dz * m[5];
                const float pz = dx * m[6] + dy * m[7] + dz * m[8];
                if (pz <= p.nearZ) continue;
                const float sx = px / pz * p.f0 + p.cx - b.x, sy = py / pz * p.f1 + p.cy - b.y;
                const float len = std::hypot(sx, sy);
                if (len > 0.5f) out[i].push_back(Cast{{sx / len, sy / len}, base * l.length, weight});
            }
        }
        if (sunWeight > 0.01f) out[i].push_back(Cast{{sun[0], sun[1]}, base * sunLength, sunWeight});
    }
    return out;
}

float groundDepth(const camera::Projection& p, const float up[3], float feetX, float feetY, float feetZ, float slope,
                  float x, float y) {
    const float z = feetZ + (y - feetY) * slope;
    if (up[0] * up[0] + up[1] * up[1] + up[2] * up[2] < 0.5f || p.f0 <= 0.0f || p.f1 <= 0.0f) return z;
    // the view's ray through the pixel (1 deep) and the feet, against the plane through them
    const float ray[3] = {(x - p.cx) / p.f0, (y - p.cy) / p.f1, 1.0f};
    const float feet[3] = {(feetX - p.cx) * feetZ / p.f0, (feetY - p.cy) * feetZ / p.f1, feetZ};
    const float across = up[0] * ray[0] + up[1] * ray[1] + up[2] * ray[2];
    if (std::fabs(across) < 1e-4f) return z;  // the ray along the ground: its horizon
    const float flat = (up[0] * feet[0] + up[1] * feet[1] + up[2] * feet[2]) / across;
    return flat > 0.0f ? std::min(z, flat) : z;  // past the horizon: none
}

}  // namespace uagfx::actor_shadows
