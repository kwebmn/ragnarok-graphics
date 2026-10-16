#include <cmath>
#include <cstdint>
#include <vector>

#include "core/ground_pick.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// uaRO.exe's own: a triangle against the ray (0x40B250, Moller-Trumbore, the distance not bounded) and a cell's two
// triangles (0x42A9A0: corners (x0, h0, z0), (x1, h1, z0), (x0, h2, z1), (x1, h3, z1)), the nearer hit or none.
bool triangle(const float r[6], const float a[3], const float b[3], const float c[3], float& t) {
    const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float p[3] = {e2[2] * r[4] - e2[1] * r[5], e2[0] * r[5] - e2[2] * r[3], e2[1] * r[3] - e2[0] * r[4]};
    const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (det > -1e-5f && det < 1e-5f) return false;
    const float inv = 1.0f / det;
    const float s[3] = {r[0] - a[0], r[1] - a[1], r[2] - a[2]};
    const float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const float q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - e1[2] * s[0], s[0] * e1[1] - s[1] * e1[0]};
    const float v = (q[0] * r[3] + q[1] * r[4] + q[2] * r[5]) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = (q[0] * e2[0] + q[1] * e2[1] + q[2] * e2[2]) * inv;
    return true;
}

struct Ground {
    int w, h;
    float size;
    std::vector<float> heights;  // four a cell
    float cell(const float r[6], int x, int z) const {
        const float* hh = &heights[size_t(z * w + x) * 4];
        const float x0 = float(x - w / 2) * size, x1 = float(x - w / 2 + 1) * size;
        const float z0 = float(z - h / 2) * size, z1 = float(z - h / 2 + 1) * size;
        const float v0[3] = {x0, hh[0], z0}, v1[3] = {x1, hh[1], z0}, v2[3] = {x0, hh[2], z1}, v3[3] = {x1, hh[3], z1};
        float best = pick::kNone, t = 0;
        if (triangle(r, v0, v1, v2, t) && t < best) best = t;
        if (triangle(r, v1, v3, v2, t) && t < best) best = t;
        return best;
    }
};

uint32_t g_seed = 12345;
float rnd(float a, float b) {
    g_seed = g_seed * 1664525u + 1013904223u;
    return a + (b - a) * float(g_seed >> 8) / float(1 << 24);
}

}  // namespace

TEST(ground_pick_walk_finds_what_testing_every_cell_finds) {
    // 0.89: thousands of rays - from above the map and from beside it, steep and grazing, a few nearly straight down,
    // some missing the map - the walk's cell is the cell every cell's test finds (or the same distance: a tie).
    for (int map = 0; map < 6; ++map) {
        Ground g{20 + map * 13, 15 + map * 7, map % 2 ? 10.0f : 5.0f, {}};
        g.heights.resize(size_t(g.w * g.h) * 4);
        for (int z = 0; z < g.h; ++z)
            for (int x = 0; x < g.w; ++x)
                for (int k = 0; k < 4; ++k) {  // a rolling ground (corner by corner), a raised cell now and then
                    const float cx = float(x + (k & 1)), cz = float(z + (k >> 1));
                    const float raised = (x * 7 + z * 3) % 23 == 0 ? -30.0f : 0.0f;
                    g.heights[size_t(z * g.w + x) * 4 + size_t(k)] =
                        -8.0f * std::sin(cx * 0.3f) * std::cos(cz * 0.2f) + raised;
                }
        int hits = 0;
        for (int i = 0; i < 2000; ++i) {
            const float spanX = float(g.w) * g.size, spanZ = float(g.h) * g.size;
            float r[6] = {rnd(-0.8f * spanX, 0.8f * spanX), rnd(-300.0f, 100.0f), rnd(-0.8f * spanZ, 0.8f * spanZ),
                          rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f)};
            if (i % 50 == 0) r[3] = r[5] = 1e-9f;  // straight down
            if (i % 7 == 0) r[4] = std::fabs(r[4]) + 0.5f;  // down onto the ground
            const float len = std::sqrt(r[3] * r[3] + r[4] * r[4] + r[5] * r[5]);
            for (int k = 3; k < 6; ++k) r[k] /= len;
            int bx = -1, bz = -1;
            float best = pick::kNone;
            for (int z = 0; z < g.h; ++z)
                for (int x = 0; x < g.w; ++x) {
                    const float t = g.cell(r, x, z);
                    if (t < best) {
                        best = t;
                        bx = x;
                        bz = z;
                    }
                }
            int wx = -1, wz = -1;
            float wt = 0;
            const auto test = [&](int x, int z) { return g.cell(r, x, z); };
            const bool found = pick::walk(r, g.w, g.h, g.size, test, wx, wz, wt);
            CHECK(found == (best < pick::kNone));
            if (found && best < pick::kNone) {
                ++hits;
                CHECK((wx == bx && wz == bz) || std::fabs(wt - best) < 1e-3f);
            }
        }
        CHECK(hits > 150);  // most rays find the ground (a small map is missed more often)
    }
}

TEST(ground_pick_walk_not_behind_the_eye) {
    // 0.90.1: the game's pick counts no hit behind the eye (its pieces of the map wholly behind it are dropped): the
    // walk from the eye on finds what every cell's test finds among the hits in front of it.
    Ground g{60, 50, 10.0f, {}};
    g.heights.resize(size_t(g.w * g.h) * 4);
    for (size_t i = 0; i < g.heights.size(); ++i) g.heights[i] = -40.0f * std::sin(float(i % 977) * 0.01f);
    int behind = 0;
    for (int i = 0; i < 3000; ++i) {
        float r[6] = {rnd(-300, 300), rnd(-80, 40), rnd(-250, 250), rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)};
        const float len = std::sqrt(r[3] * r[3] + r[4] * r[4] + r[5] * r[5]);
        for (int k = 3; k < 6; ++k) r[k] /= len;
        int bx = -1, bz = -1;
        float best = pick::kNone, any = pick::kNone;
        for (int z = 0; z < g.h; ++z)
            for (int x = 0; x < g.w; ++x) {
                const float t = g.cell(r, x, z);
                if (t < any) any = t;
                if (t >= 0.0f && t < best) best = t, bx = x, bz = z;
            }
        behind += any < 0.0f ? 1 : 0;
        int wx = -1, wz = -1;
        float wt = 0;
        const auto test = [&](int x, int z) { return g.cell(r, x, z); };
        const bool found = pick::walk(r, g.w, g.h, g.size, test, wx, wz, wt, 0.0f);
        CHECK(found == (best < pick::kNone));
        if (found && best < pick::kNone) CHECK((wx == bx && wz == bz) || std::fabs(wt - best) < 1e-3f);
    }
    CHECK(behind > 100);  // rays that would have hit behind the eye were there
}

TEST(ground_pick_walk_no_map_no_cell) {
    const float r[6] = {0, -100, 0, 0.3f, 0.9f, 0.3f};
    int x = 0, z = 0;
    float t = 0;
    CHECK(!pick::walk(r, 0, 10, 10.0f, [](int, int) { return 1.0f; }, x, z, t));
    CHECK(!pick::walk(r, 10, 10, 0.0f, [](int, int) { return 1.0f; }, x, z, t));
    CHECK(!pick::walk(r, 10, 10, 10.0f, [](int, int) { return pick::kNone; }, x, z, t));
}

TEST(ground_pick_trusted_after_agreeing_on_each_map) {
    pick::Trust trust;
    CHECK(trust.on(0x1000));
    CHECK(!trust.trusted());
    for (int i = 1; i < pick::Trust::kAgree; ++i) CHECK(!trust.result(true));
    CHECK(trust.result(true));  // the 240th: trusted from now on
    CHECK(trust.trusted());
    CHECK(!trust.result(true));
    int checked = 0;
    for (int i = 0; i < 10 * pick::Trust::kEvery; ++i) checked += trust.check() ? 1 : 0;
    CHECK(checked == 10);  // trusted: one pick in kEvery checked still
    CHECK(!trust.on(0x1000));  // the same map: still
    CHECK(trust.trusted());
    trust.on(0x2000);  // another: checked anew, every pick
    CHECK(!trust.trusted());
    CHECK(trust.check() && trust.check());
    for (int i = 0; i < 10; ++i) {  // 0.90.2: while the map settles, not counted
        trust.check();
        CHECK(!trust.result(false));
    }
    CHECK(!trust.off());
    for (int i = 0; i < pick::Trust::kSettle; ++i) trust.check();
    CHECK(!trust.result(false));  // a disagreement: checked anew
    CHECK(!trust.off() && !trust.trusted());
    for (int i = 1; i < pick::Trust::kAgree; ++i) trust.result(true);
    CHECK(trust.result(true));
    CHECK(!trust.result(false) && !trust.result(false));  // the third: the game's own from then on
    CHECK(trust.off());
    for (int i = 0; i < 300; ++i) trust.result(true);
    trust.on(0x3000);
    CHECK(!trust.trusted());
}

TEST(ground_pick_same_distance_is_a_tie) {
    CHECK(pick::sameDistance(250.0f, 250.1f));
    CHECK(!pick::sameDistance(250.0f, 251.0f));
    CHECK(pick::sameDistance(0.0f, 0.0005f));
    CHECK(!pick::sameDistance(0.0f, 0.01f));
}
