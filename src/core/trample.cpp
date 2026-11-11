#include "core/trample.hpp"

#include <algorithm>
#include <cmath>

#include "core/round.hpp"

namespace uagfx::trample {
namespace {

// The share of its life a step has had: 0 just left .. 1 gone (and gone with no life at all)
float ageOf(const footsteps::Step& s, float now, float life) {
    return life > 0.0f ? std::clamp((now - s.born) / life, 0.0f, 1.0f) : 1.0f;
}

// One step's part at a spot dx, dz from it (grass_ps's sum until 2.6.8)
void add(Hold& h, float dx, float dz, float age) {
    const float dd = dx * dx + dz * dz;
    if (dd < 12.0f && dd > 1e-4f) {
        const float k = (1.0f - age) * (1.0f - dd / 12.0f) / std::sqrt(dd);
        h.pushX += dx * k;
        h.pushZ += dz * k;
    }
    h.left *= 1.0f + (std::clamp(dd / 9.0f, 0.0f, 1.0f) - 1.0f) * (1.0f - age);
}

// 3.0.5-beta.9 (R-VMPQKX: lroundf a fifth of a Radeon 610M laptop's game thread on «Середні» - two values each of the
// grid's 16384 texels a frame): roundAway, the same answers without the library's call
int8_t snorm(float v) { return int8_t(roundAway(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }
}  // namespace

void build(const std::deque<footsteps::Step>& steps, float now, float life, Grid& out) {
    out.texels.clear();
    float x0 = 0.0f, z0 = 0.0f, x1 = 0.0f, z1 = 0.0f;
    bool any = false;
    for (const footsteps::Step& s : steps) {
        if (ageOf(s, now, life) >= 1.0f) continue;
        x0 = any ? std::min(x0, s.x) : s.x;
        x1 = any ? std::max(x1, s.x) : s.x;
        z0 = any ? std::min(z0, s.z) : s.z;
        z1 = any ? std::max(z1, s.z) : s.z;
        any = true;
    }
    if (!any) return;
    constexpr int n = Grid::kSide;
    // a step's reach around the bounds, then a texel more each side that nothing reaches: the edge holds nothing
    const float span = std::max(x1 - x0, z1 - z0) + 2.0f * Grid::kReach;
    out.cell = std::max(0.5f, span / float(n - 4));
    out.originX = 0.5f * (x0 + x1) - 0.5f * float(n) * out.cell;
    out.originZ = 0.5f * (z0 + z1) - 0.5f * float(n) * out.cell;
    std::vector<float>& sums = out.sums;
    sums.assign(size_t(n) * n * 3, 0.0f);
    for (size_t i = 2; i < sums.size(); i += 3) sums[i] = 1.0f;
    const float reach = Grid::kReach + out.cell;  // a texel's centre within reach of the step's place
    for (const footsteps::Step& s : steps) {
        const float age = ageOf(s, now, life);
        if (age >= 1.0f) continue;
        const int ix0 = std::max(0, floorInt((s.x - reach - out.originX) / out.cell));
        const int ix1 = std::min(n - 1, ceilInt((s.x + reach - out.originX) / out.cell));
        const int iz0 = std::max(0, floorInt((s.z - reach - out.originZ) / out.cell));
        const int iz1 = std::min(n - 1, ceilInt((s.z + reach - out.originZ) / out.cell));
        for (int iz = iz0; iz <= iz1; ++iz)
            for (int ix = ix0; ix <= ix1; ++ix) {
                float* t = &sums[(size_t(iz) * n + size_t(ix)) * 3];
                Hold h{t[0], t[1], t[2]};
                add(h, out.originX + (float(ix) + 0.5f) * out.cell - s.x,
                    out.originZ + (float(iz) + 0.5f) * out.cell - s.z, age);
                t[0] = h.pushX;
                t[1] = h.pushZ;
                t[2] = h.left;
            }
    }
    out.texels.resize(size_t(n) * n * 4);
    for (size_t i = 0, j = 0; i < sums.size(); i += 3, j += 4) {
        out.texels[j] = snorm(0.5f * sums[i]);
        out.texels[j + 1] = snorm(0.5f * sums[i + 1]);
        out.texels[j + 2] = snorm(sums[i + 2]);
        out.texels[j + 3] = 0;
    }
}

}  // namespace uagfx::trample
