#include "core/portal.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::portal {
namespace {
float smooth(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}  // namespace

std::array<Ring, kRings> ringsAt(double t) {
    std::array<Ring, kRings> out{};
    const double newest = std::floor(t / double(kRingEvery));
    for (int k = 0; k < kRings; ++k) {
        const float age = float(t - (newest - k) * double(kRingEvery));
        if (age < 0.0f || age >= kRingLife) continue;
        const float f = age / kRingLife;
        out[size_t(k)].radius = kRingRadius - kRingShrink * age / kTick;
        out[size_t(k)].alpha = kRingAlpha * smooth(0.0f, 0.125f, f) * (1.0f - smooth(0.8f, 1.0f, f));
    }
    return out;
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

bool screenBox(const float foot[3], const float up[3], const camera::Projection& p, float height, float box[4]) {
    // two ways along the ground, square to the up
    const float pick[3] = {std::fabs(up[0]) < 0.9f ? 1.0f : 0.0f, std::fabs(up[0]) < 0.9f ? 0.0f : 1.0f, 0.0f};
    float a[3] = {up[1] * pick[2] - up[2] * pick[1], up[2] * pick[0] - up[0] * pick[2],
                  up[0] * pick[1] - up[1] * pick[0]};
    const float al = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    for (float& v : a) v /= al;
    const float b[3] = {up[1] * a[2] - up[2] * a[1], up[2] * a[0] - up[0] * a[2], up[0] * a[1] - up[1] * a[0]};
    const float r = kColumnRadius * 1.2f;
    box[0] = box[1] = 1e30f;
    box[2] = box[3] = -1e30f;
    for (int k = 0; k < 8; ++k)
        for (const float h : {0.0f, height}) {
            const float c = std::cos(float(k) * 0.785398f) * r, s = std::sin(float(k) * 0.785398f) * r;
            float q[3];
            for (int i = 0; i < 3; ++i) q[i] = foot[i] + a[i] * c + b[i] * s + up[i] * h;
            if (q[2] <= p.nearZ) return false;
            const float x = q[0] / q[2] * p.f0 + p.cx, y = q[1] / q[2] * p.f1 + p.cy;
            box[0] = std::min(box[0], x);
            box[1] = std::min(box[1], y);
            box[2] = std::max(box[2], x);
            box[3] = std::max(box[3], y);
        }
    return true;
}

}  // namespace uagfx::portal
