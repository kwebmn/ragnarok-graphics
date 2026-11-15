// 3.0.19-beta.49 (the owner's «давай» - uaRO.exe's hot parts rewritten, after __ftol): its vertex projection (0x412CB0:
// three divisions a vertex, one of them the same for all) and its faces' sort (0x416850: MSVC 6's std::sort, every
// comparison through two faces' pointers) - the same numbers, the same order
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "core/exe_hot.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// uaRO.exe FUN_00412CB0 as it computes it (x87 at double precision, the outputs stored as floats)
void gameProject(float f0, float f1, int cx, int cy, float nearZ, float farZ, const float p[3], const float m[12],
                 float out[4]) {
    const float rhw = float(1.0 / (double(m[2]) * p[0] + double(m[5]) * p[1] + double(m[8]) * p[2] + m[11]));
    out[3] = rhw;
    out[0] = float((double(p[0]) * m[0] + double(m[3]) * p[1] + double(m[6]) * p[2] + m[9]) * rhw * f0 + cx);
    out[1] = float((double(m[1]) * p[0] + double(m[4]) * p[1] + double(m[7]) * p[2] + m[10]) * f1 * rhw + cy);
    out[2] = float((1.0 / rhw - nearZ) * (double(farZ) / (double(farZ) - nearZ)) * rhw);
}
}  // namespace

TEST(exe_hot_projection_as_the_games_own) {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> any(-400.0f, 400.0f), ahead(15.0f, 1400.0f);
    const float k = exehot::depthScale(10.0f, 1500.0f);
    for (int n = 0; n < 2000; ++n) {
        float m[12];
        for (float& v : m) v = any(rng) / 400.0f;
        m[11] = ahead(rng);  // the point well in front
        const float p[3] = {any(rng) * 0.1f, any(rng) * 0.1f, any(rng) * 0.1f};
        float game[4], ours[4];
        gameProject(1000.0f, 1000.0f, 960, 540, 10.0f, 1500.0f, p, m, game);
        exehot::project(1000.0f, 1000.0f, 960, 540, 10.0f, k, p, m, ours);
        if (!(game[3] > 0.0f)) continue;
        CHECK(std::fabs(ours[3] - game[3]) <= 1e-6f * std::fabs(game[3]));
        CHECK(std::fabs(ours[0] - game[0]) <= 1e-3f + 1e-6f * std::fabs(game[0]));
        CHECK(std::fabs(ours[1] - game[1]) <= 1e-3f + 1e-6f * std::fabs(game[1]));
        CHECK(std::fabs(ours[2] - game[2]) <= 1e-6f);
    }
}

TEST(exe_hot_faces_sorted_by_their_keys_in_order_when_alike) {
    std::vector<exehot::FaceKey> keys = {{exehot::faceKey(5, 1), 0, 100}, {exehot::faceKey(2, 9), 1, 101},
                                         {exehot::faceKey(5, 0), 2, 102}, {exehot::faceKey(2, 9), 3, 103},
                                         {exehot::faceKey(0xFFFFFFFFu, 0), 4, 104}, {exehot::faceKey(2, 8), 5, 105}};
    exehot::sortFaces(keys);
    const uint32_t want[6] = {105, 101, 103, 102, 100, 104};  // texture, then the second key; alike: as they came
    for (size_t i = 0; i < keys.size(); ++i) CHECK(keys[i].face == want[i]);
    CHECK(exehot::faceKey(1, 0) > exehot::faceKey(0, 0xFFFFFFFFu));  // the first key first, unsigned
}
