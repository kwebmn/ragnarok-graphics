#include "core/portal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/d3d7.hpp"
#include "core/report_upload.hpp"

namespace uagfx::portal {
Part partOf(int effectId, bool blend, uint8_t srcBlend, uint8_t dstBlend, uint32_t vertexCount) {
    if (effectId != kWarpEffect || !blend || srcBlend != d3d7::BLEND_SRCALPHA) return Part::None;
    if (dstBlend == d3d7::BLEND_ONE) return vertexCount == 3 ? Part::Ring : Part::None;
    if (dstBlend != d3d7::BLEND_INVSRCALPHA) return Part::None;
    return vertexCount == 3 ? Part::Floor : vertexCount == 4 ? Part::Spark : Part::None;
}

std::vector<std::string> pictureLines(uint32_t texture, int width, int height, const uint32_t* argb) {
    char head[96];
    std::snprintf(head, sizeof head, "portal capture: texture %u %dx%d (ARGB, base64)", unsigned(texture), width,
                  height);
    std::vector<std::string> lines{head};
    if (width <= 0 || height <= 0 || !argb) return lines;
    const std::string text =
        report::base64(std::string(reinterpret_cast<const char*>(argb), size_t(width) * size_t(height) * 4));
    for (size_t at = 0; at < text.size(); at += 160) lines.push_back("  t " + text.substr(at, 160));
    return lines;
}

bool floorMiddle(const Corner c[3], Corner& out) {
    for (int k = 0; k < 3; ++k)
        if (c[k].v > 0.99f) {
            out = c[k];
            return true;
        }
    return false;
}

void follow(std::vector<Track>& tracks, const std::vector<Seen>& draws, float step) {
    struct Gate {
        float view[3] = {0.0f, 0.0f, 0.0f}, gx = 0.0f, gy = 0.0f, size = 0.0f;
        float left = 1e30f, right = -1e30f, bottom = -1e30f;
        int n = 0;
        bool claimed = false;
    };
    std::vector<Gate> seen;
    for (const Seen& s : draws) {
        if (s.size > 600.0f) continue;
        Gate* g = nullptr;
        for (Gate& o : seen)
            if (std::hypot(o.gx / float(o.n) - s.gx, o.gy / float(o.n) - s.gy) <
                std::max(80.0f, std::max(o.size, s.size) * 0.8f))
                g = &o;
        if (!g) {
            seen.push_back(Gate{});
            g = &seen.back();
        }
        for (int k = 0; k < 3; ++k) g->view[k] += s.view[k];
        g->gx += s.gx;
        g->gy += s.gy;
        g->size = std::max(g->size, s.size);
        g->left = std::min(g->left, s.left);
        g->right = std::max(g->right, s.right);
        g->bottom = std::max(g->bottom, s.bottom);
        ++g->n;
    }
    for (Gate& g : seen) {
        for (float& v : g.view) v /= float(g.n);
        g.gx /= float(g.n);
        g.gy /= float(g.n);
    }
    const float ease = 1.0f - std::exp(-step * 5.0f), rise = step / 0.4f;
    for (Track& t : tracks) {
        Gate* best = nullptr;
        float bestD = 140.0f;
        for (Gate& g : seen) {
            const float d = std::hypot(g.gx - t.gx, g.gy - t.gy);
            if (!g.claimed && d < bestD) {
                bestD = d;
                best = &g;
            }
        }
        if (best) {
            best->claimed = true;
            for (int k = 0; k < 3; ++k) t.view[k] += (best->view[k] - t.view[k]) * ease;
            t.gx += (best->gx - t.gx) * ease;
            t.gy += (best->gy - t.gy) * ease;
            t.size += (best->size - t.size) * ease;
            t.left += (best->left - t.left) * ease;
            t.right += (best->right - t.right) * ease;
            t.bottom += (best->bottom - t.bottom) * ease;
            t.strength = std::min(1.0f, t.strength + rise);
        } else {
            t.strength -= step / 1.2f;  // its rings gone a moment: it fades rather than goes out
        }
    }
    for (const Gate& g : seen)
        if (!g.claimed && tracks.size() < 16)
            tracks.push_back(Track{{g.view[0], g.view[1], g.view[2]}, g.gx, g.gy, g.size, std::max(rise, 1e-3f),
                                   g.left, g.right, g.bottom});
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const Track& t) { return t.strength <= 0.0f; }),
                 tracks.end());
}

void anchorOf(const Track& t, float squash, float out[2]) {
    out[0] = (t.left + t.right) * 0.5f;
    out[1] = t.bottom - (t.right - t.left) * 0.5f * std::fabs(squash);
}

void footOf(const Track& t, float squash, const camera::Projection& p, float out[3]) {
    float at[2];
    anchorOf(t, squash, at);
    const float z = t.view[2];
    out[0] = (at[0] - p.cx) / p.f0 * z;
    out[1] = (at[1] - p.cy) / p.f1 * z;
    out[2] = z;
}

}  // namespace uagfx::portal
