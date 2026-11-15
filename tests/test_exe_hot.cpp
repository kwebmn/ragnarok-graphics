// 3.0.19-beta.49 (the owner's «давай» - uaRO.exe's hot parts rewritten, after __ftol): its vertex projection (0x412CB0:
// three divisions a vertex, one of them the same for all) and its faces' sort (0x416850: MSVC 6's std::sort, every
// comparison through two faces' pointers) - the same numbers, the same order
#include <cmath>
#include <cstdint>
#include <cstring>
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

// 3.0.19-beta.53 (the owner's «берись»: the game's own busiest place in the 3D view - R-GHP9SJ's profile, 3.9 % of
// its thread - was MSVC 6's vector insert writing a blank vertex into every place the models' faces may take, each
// frame; the models' vertices projected a call each): the pool grown at its end with nothing written; the model's
// vertices projected in a row, each as one at a time makes it, the rest of each vertex untouched
TEST(exe_hot_pool_grows_at_its_end_without_blanks) {
    exehot::GameVector v{0, 0x1000, 0x1000 + 10 * 32, 0x1000 + 16 * 32};
    CHECK(exehot::growAtEnd(v, v.end, 6) && v.end == 0x1000 + 16 * 32);  // all of its room
    CHECK(!exehot::growAtEnd(v, v.end, 1) && v.end == 0x1000 + 16 * 32);  // no room: the game's own
    exehot::GameVector w{0, 0x2000, 0x2000 + 4 * 32, 0x2000 + 8 * 32};
    CHECK(!exehot::growAtEnd(w, w.begin + 32, 2) && w.end == 0x2000 + 4 * 32);  // not at its end: the game's own
    CHECK(exehot::growAtEnd(w, w.end, 0) && w.end == 0x2000 + 4 * 32);
    CHECK(exehot::kGrowSize == 7 && exehot::kGrowBytes[0] == 0x55 && exehot::kGrowBytes[6] == 0x53);
}

TEST(exe_hot_model_vertices_projected_in_a_row_as_one_at_a_time) {
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> any(-400.0f, 400.0f), ahead(15.0f, 1400.0f);
    const float k = exehot::depthScale(10.0f, 1500.0f);
    for (int round = 0; round < 50; ++round) {
        float m[12];
        for (float& v : m) v = any(rng) / 400.0f;
        m[11] = ahead(rng);
        const int count = 1 + round % 13;
        std::vector<float> points(size_t(count) * 3), out(size_t(count) * 8, 7.5f), one(4);
        for (float& p : points) p = any(rng) * 0.1f;
        exehot::projectMany(1000.0f, 990.0f, 960, 540, 10.0f, k, points.data(), m, out.data(), count);
        for (int i = 0; i < count; ++i) {
            exehot::project(1000.0f, 990.0f, 960, 540, 10.0f, k, &points[size_t(i) * 3], m, one.data());
            for (int c = 0; c < 4; ++c) CHECK(out[size_t(i) * 8 + c] == one[c]);  // the very same numbers
            for (int c = 4; c < 8; ++c) CHECK(out[size_t(i) * 8 + c] == 7.5f);   // its colours and place untouched
        }
    }
}

TEST(exe_hot_model_loop_bytes_are_the_games_call_a_vertex) {
    const uint8_t* b = exehot::kModelLoopBytes;
    CHECK(exehot::kModelLoopSize == 46 && exehot::kModelLoop + exehot::kModelLoopSize == exehot::kModelLoopEnd);
    CHECK(b[0] == 0x8B && b[1] == 0x4D && b[2] == 0xCC);  // mov ecx, [ebp-0x34]: the count
    uint32_t out = 0, renderer = 0;
    std::memcpy(&out, b + 4, 4);
    std::memcpy(&renderer, b + 13, 4);
    CHECK(b[3] == 0xBE && out == exehot::kModelOut);              // mov esi, 0x731150
    CHECK(b[11] == 0x8B && b[12] == 0x0D && renderer == exehot::kRenderer);  // mov ecx, [0x6EE3D8]
    int32_t rel = 0;
    std::memcpy(&rel, b + 0x1B, 4);
    CHECK(b[0x1A] == 0xE8 && exehot::kModelLoop + 0x1A + 5 + uint32_t(rel) == exehot::kProject);  // the projection
    CHECK(b[0x2C] == 0x75 && int8_t(b[0x2D]) == -0x23);  // jne back to the loop's head
}

TEST(exe_hot_model_stub_calls_ours_and_leaves_the_loop_as_it_would) {
    uint8_t b[exehot::kModelStubSize] = {};
    const uint32_t at = 0x10000000, ours = 0x10001000;
    exehot::modelStub(at, ours, b);
    const auto word = [&](size_t i) {
        uint32_t v = 0;
        std::memcpy(&v, b + i, 4);
        return v;
    };
    CHECK(b[0] == 0x8B && b[1] == 0x45 && b[2] == 0xCC);                 // mov eax, [ebp-0x34]: the count
    CHECK(b[3] == 0x8D && b[4] == 0x95 && word(5) == 0xFFFFFF78u);         // lea edx, [ebp-0x88]: the matrix
    CHECK(b[10] == 0x68 && word(11) == exehot::kModelOut);                 // push out
    CHECK(b[17] == 0xFF && b[18] == 0x35 && word(19) == exehot::kRenderer);  // push [the renderer]
    CHECK(b[23] == 0xE8 && at + 28 + word(24) == ours);                    // call ours
    CHECK(b[28] == 0x83 && b[29] == 0xC4 && b[30] == 20);                  // its 5 arguments off the stack
    CHECK(b[45] == 0x81 && b[46] == 0xC6 && word(47) == exehot::kModelOut);  // esi past the last out vertex
    CHECK(b[53] == 0x89 && b[54] == 0x45 && b[55] == 0xEC);                // its counter [ebp-0x14] 0
    CHECK(b[56] == 0xE9 && at + uint32_t(exehot::kModelStubSize) + word(57) == exehot::kModelLoopEnd);  // on past it
}
