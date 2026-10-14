#include <cmath>

#include "core/game_camera.hpp"
#include "test.hpp"

using namespace uagfx::camera;

namespace {

struct V3 {
    float x, y, z;
};
V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 unit(V3 a) {
    const float l = std::sqrt(dot(a, a));
    return {a.x / l, a.y / l, a.z / l};
}

// D3DXMatrixLookAtLH, as the CView build makes it (world y points down in the game).
void lookAt(V3 eye, V3 at, V3 up, float m[12]) {
    const V3 z = unit(sub(at, eye));
    const V3 x = unit(cross(up, z));
    const V3 y = cross(z, x);
    const float v[12] = {x.x, y.x, z.x, x.y, y.y, z.y, x.z, y.z, z.z, -dot(x, eye), -dot(y, eye), -dot(z, eye)};
    for (int i = 0; i < 12; ++i) m[i] = v[i];
}

// ProjectVertex 0x412cb0.
void project(const Projection& p, const float m[12], V3 w, float& sx, float& sy, float& sz) {
    const float zc = m[2] * w.x + m[5] * w.y + m[8] * w.z + m[11];
    const float rhw = 1.0f / zc;
    sx = (w.x * m[0] + m[3] * w.y + m[6] * w.z + m[9]) * rhw * p.f0 + p.cx;
    sy = (m[1] * w.x + m[4] * w.y + m[7] * w.z + m[10]) * p.f1 * rhw + p.cy;
    sz = (1.0f / rhw - p.nearZ) * (p.farZ / (p.farZ - p.nearZ)) * rhw;
}

}  // namespace

TEST(game_camera_knows_its_build_and_projects_as_setsize) {
    const float good[5] = {kPi, 1500.0f, 10.0f, 15.0f, 15.0f};
    CHECK(knownConstants(good));
    const float other[5] = {kPi, 2000.0f, 10.0f, 15.0f, 15.0f};
    CHECK(!knownConstants(other));
    const Projection p = projectionFor(1920, 1080);
    CHECK(std::fabs(p.f1 - 540.0 / std::tan(7.5 * 3.14159265358979 / 180.0)) < 0.01);
    CHECK(std::fabs(p.f0 - p.f1) < 0.01);  // square pixels: the same focal length on both axes
    CHECK_EQ(p.cx, 960.0f);
    CHECK_EQ(p.cy, 540.0f);
    CHECK_EQ(projectionFor(2195, 1235).cx, 1097.0f);  // whole pixels, as the game's (int) casts make them
}

TEST(game_camera_moves_a_depth_pixel_where_the_previous_camera_saw_it) {
    // Part 3 (DLSS): a point the game drew at (sx, sy, sz) with this frame's camera, back where the last frame's camera
    // put it - the motion vector without any help from the game.
    const Projection p = projectionFor(2194, 1234);
    float now[12], before[12];
    lookAt({300, -250, 100}, {50, 0, 80}, {0, -1, 0}, now);
    lookAt({310, -255, 140}, {52, 0, 79}, {0, -1, 0}, before);  // walked and turned a little
    CHECK(plausibleView(now));
    float r[12];
    reprojection(now, before, r);
    for (V3 w : {V3{50, 0, 80}, V3{120, -30, 10}, V3{-40, 5, 160}}) {
        float sx, sy, sz, ex, ey, ez, px, py;
        project(p, now, w, sx, sy, sz);
        project(p, before, w, ex, ey, ez);
        reproject(p, r, sx, sy, sz, px, py);
        CHECK(std::fabs(px - ex) < 0.02f && std::fabs(py - ey) < 0.02f);
    }
    const float garbage[12] = {2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    CHECK(!plausibleView(garbage));
}

TEST(game_camera_clip_matrices_agree_with_the_game) {
    // Streamline's view -> clip, and back, give the same screen point as ProjectVertex.
    const Projection p = projectionFor(1921, 1081);  // odd: the centre is not the middle
    float now[12];
    lookAt({0, -300, -300}, {0, 0, 0}, {0, -1, 0}, now);
    float clip[16], back[16], id[16];
    viewToClip(p, 1921, 1081, clip);
    CHECK(inverse4(clip, back));
    multiply4(clip, back, id);
    for (int i = 0; i < 16; ++i) CHECK(std::fabs(id[i] - (i % 5 == 0 ? 1.0f : 0.0f)) < 1e-4f);
    const V3 w{30, -20, 40};
    float sx, sy, sz;
    project(p, now, w, sx, sy, sz);
    const float xc = now[0] * w.x + now[3] * w.y + now[6] * w.z + now[9];
    const float yc = now[1] * w.x + now[4] * w.y + now[7] * w.z + now[10];
    const float zc = now[2] * w.x + now[5] * w.y + now[8] * w.z + now[11];
    const float c[4] = {xc * clip[0] + yc * clip[4] + zc * clip[8] + clip[12],
                        xc * clip[1] + yc * clip[5] + zc * clip[9] + clip[13],
                        xc * clip[2] + yc * clip[6] + zc * clip[10] + clip[14],
                        xc * clip[3] + yc * clip[7] + zc * clip[11] + clip[15]};
    const float nx = c[0] / c[3], ny = c[1] / c[3], nz = c[2] / c[3];
    CHECK(std::fabs((nx + 1.0f) * 1921 * 0.5f - sx) < 0.01f);
    CHECK(std::fabs((1.0f - ny) * 1081 * 0.5f - sy) < 0.01f);
    CHECK(std::fabs(nz - sz) < 1e-5f);
}
