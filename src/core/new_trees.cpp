#include "core/new_trees.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::new_trees {
namespace {

uint32_t mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

}  // namespace

Coverage coverageOf(const uint32_t* argb, int width, int height) {
    Coverage c;
    if (!argb || width < 8 || height < 8) return c;
    for (int cy = 0; cy < 8; ++cy)
        for (int cx = 0; cx < 8; ++cx) {
            const int x0 = cx * width / 8, x1 = (cx + 1) * width / 8, y0 = cy * height / 8, y1 = (cy + 1) * height / 8;
            uint32_t solid = 0, all = 0;
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    ++all;
                    solid += (argb[size_t(y) * size_t(width) + size_t(x)] >> 24) >= 128;
                }
            c.cell[cy * 8 + cx] = uint8_t(all ? solid * 255 / all : 0);
            c.any = c.any || c.cell[cy * 8 + cx] > 150;
        }
    return c;
}

void bunchesOf(const float lo[3], const float hi[3], const Coverage& cov, double t, const float toSun[3],
               float sunLight, std::vector<Bunch>& out, uint32_t steadySeed) {
    if (!cov.any) return;
    const float mid[3] = {(lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f};
    const float rad[3] = {std::max((hi[0] - lo[0]) * 0.5f, 0.5f), std::max((hi[1] - lo[1]) * 0.5f, 0.5f),
                          std::max((hi[2] - lo[2]) * 0.5f, 0.5f)};
    const float across = std::max(rad[0], rad[2]);
    // the crown's own seed, from its place (a tenth of a unit apart: steady as the camera moves)
    uint32_t seed = steadySeed ? steadySeed
                               : mix(uint32_t(int(std::floor(mid[0] * 2.0f))) * 73856093u ^
                                     uint32_t(int(std::floor(mid[2] * 2.0f))) * 19349663u);
    const auto random = [&seed]() {
        seed = mix(seed + 0x9e3779b9u);
        return float(seed & 0xffffff) / float(0x1000000);
    };
    // the cells of the picture that are leaves, to cut the bunches from
    int cells[64], usable = 0;
    for (int k = 0; k < 64; ++k)
        if (cov.cell[k] > 150) cells[usable++] = k;
    const int count = std::clamp(int(across * across * 1.6f), 24, 140);
    for (int k = 0; k < count; ++k) {
        // a place in the crown's ellipsoid, more of them towards its skin
        float x = 0, y = 0, z = 0, r2 = 2;
        while (r2 > 1.0f) {
            x = random() * 2 - 1;
            y = random() * 2 - 1;
            z = random() * 2 - 1;
            r2 = x * x + y * y + z * z;
        }
        const float skin = std::sqrt(r2) < 0.45f ? 0.6f : 1.0f;  // the middle thinner: nobody sees it
        if (random() > skin) continue;
        Bunch b;
        // the wind: the top more, each bunch a little its own way
        const float up = std::clamp(0.5f - y * 0.5f, 0.0f, 1.0f);  // y up along -y: the top is y -1
        const float phase = random() * 6.2831853f;
        // 1.25.1: a third of 1.10's - the bunches went round in circles
        const float sway = (std::sin(float(t) * 1.4f + phase * 0.3f + mid[0] * 0.05f) * 0.02f +
                            std::sin(float(t) * 3.1f + phase) * 0.007f) * up * across;
        b.pos[0] = mid[0] + x * rad[0] + sway;
        b.pos[1] = mid[1] + y * rad[1];
        b.pos[2] = mid[2] + z * rad[2] + sway * 0.5f;
        b.size = across * (0.42f + 0.28f * random());
        const int cell = cells[int(random() * float(usable)) % usable];
        const float cu = float(cell % 8) / 8.0f, cv = float(cell / 8) / 8.0f, span = 0.25f;
        b.uv[0] = std::clamp(cu - span * 0.5f + 0.0625f, 0.0f, 1.0f - span);
        b.uv[1] = std::clamp(cv - span * 0.5f + 0.0625f, 0.0f, 1.0f - span);
        b.uv[2] = b.uv[0] + span;
        b.uv[3] = b.uv[1] + span;
        // lit as the crown's part: towards the sun lighter, the inside and the bottom darker
        const float len = std::max(std::sqrt(r2), 1e-3f);
        const float n[3] = {x / len, y / len, z / len};
        const float facing = n[0] * toSun[0] + n[1] * toSun[1] + n[2] * toSun[2];
        const float inside = 0.65f + 0.35f * std::sqrt(r2);
        const float bottom = 1.0f - 0.18f * std::clamp(y, 0.0f, 1.0f);
        b.light = std::clamp((0.72f + 0.38f * facing * sunLight) * inside * bottom, 0.4f, 1.3f);
        b.turn = random() * 6.2831853f;
        out.push_back(b);
    }
}

uint32_t Steady::settle(uint32_t texture, float lo[3], float hi[3]) {
    const float mid[3] = {(lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f};
    Kept* best = nullptr;
    float bestD = 1.5f * 1.5f;
    for (Kept& k : kept_) {
        if (k.texture != texture || k.used) continue;
        const float dx = (k.lo[0] + k.hi[0]) * 0.5f - mid[0], dz = (k.lo[2] + k.hi[2]) * 0.5f - mid[2];
        const float dy = (k.lo[1] + k.hi[1]) * 0.5f - mid[1];
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < bestD) {
            bestD = d;
            best = &k;
        }
    }
    if (!best) {
        Kept k;
        k.texture = texture;
        k.used = true;
        k.seed = mix(uint32_t(int(std::floor(mid[0] * 2.0f))) * 73856093u ^
                     uint32_t(int(std::floor(mid[2] * 2.0f))) * 19349663u ^ texture * 83492791u ^
                     uint32_t(kept_.size()) * 2654435761u) | 1u;  // a second crown of one tree: its own
        for (int a = 0; a < 3; ++a) {
            k.lo[a] = lo[a];
            k.hi[a] = hi[a];
        }
        kept_.push_back(k);
        return kept_.back().seed;
    }
    best->unseen = 0;
    best->used = true;
    bool moved = false;
    for (int a = 0; a < 3; ++a)
        moved = moved || std::fabs(best->lo[a] - lo[a]) > 1.0f || std::fabs(best->hi[a] - hi[a]) > 1.0f;
    for (int a = 0; a < 3; ++a) {
        if (moved) {
            best->lo[a] = lo[a];
            best->hi[a] = hi[a];
        } else {
            lo[a] = best->lo[a];
            hi[a] = best->hi[a];
        }
    }
    return best->seed;
}

void Steady::endFrame() {
    for (Kept& k : kept_) {
        ++k.unseen;
        k.used = false;
    }
    kept_.erase(std::remove_if(kept_.begin(), kept_.end(), [](const Kept& k) { return k.unseen > 120; }),
                kept_.end());
}

}  // namespace uagfx::new_trees
