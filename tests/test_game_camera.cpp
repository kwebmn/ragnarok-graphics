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

namespace {

float det3(const float m[16]) {
    return m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) +
           m[2] * (m[4] * m[9] - m[5] * m[8]);
}

// The point in the game's world -> where Remix's view and projection put it on the screen.
void remixScreen(const float v[16], const float clip[16], const float r[3], int width, int height, float& sx,
                 float& sy, float& sz) {
    float e[4], c[4];
    for (int k = 0; k < 4; ++k) e[k] = r[0] * v[k] + r[1] * v[4 + k] + r[2] * v[8 + k] + v[12 + k];
    for (int k = 0; k < 4; ++k) c[k] = e[0] * clip[k] + e[1] * clip[4 + k] + e[2] * clip[8 + k] + e[3] * clip[12 + k];
    sx = (c[0] / c[3] + 1.0f) * float(width) * 0.5f;
    sy = (1.0f - c[1] / c[3]) * float(height) * 0.5f;
    sz = c[2] / c[3];
}

void checkRemixCamera(const float view[12], int width, int height) {
    const Projection p = projectionFor(width, height);
    const RemixSpace s = remixSpaceFor(view);
    float v[16], clip[16], g[16], iv[16];
    remixView(view, s, v);
    remixProjection(p, width, height, clip);
    CHECK(clip[0] > 0.0f && clip[5] > 0.0f);  // nothing upside down: Remix rejects a negative field of view
    CHECK_EQ(clip[11], 1.0f);                  // left-handed: w = view z
    CHECK(std::fabs(det3(v) - 1.0f) < 1e-3f);  // a rotation: Remix's world is left-handed like its view space
    affine4(view, g);
    CHECK(inverse4(g, iv));
    for (V3 w : {V3{50, 0, 80}, V3{120, -30, 10}, V3{-40, 5, 160}, V3{70, -60, 90}}) {
        float sx, sy, sz, r[3], rx, ry, rz;
        project(p, view, w, sx, sy, sz);
        const float zc = view[2] * w.x + view[5] * w.y + view[8] * w.z + view[11];
        CHECK(remixPoint(p, iv, s, sx, sy, 1.0f / zc, r));
        CHECK(std::fabs(r[0] - s.flip[0] * w.x) < 0.05f && std::fabs(r[1] - s.flip[1] * w.y) < 0.05f &&
              std::fabs(r[2] - s.flip[2] * w.z) < 0.05f);
        remixScreen(v, clip, r, width, height, rx, ry, rz);
        CHECK(std::fabs(rx - sx) < 0.02f && std::fabs(ry - sy) < 0.02f && std::fabs(rz - sz) < 1e-4f);
    }
}

}  // namespace

TEST(game_camera_hands_remix_a_y_up_world_that_shows_as_the_game_draws_it) {
    // Part 4 (RTX Remix): the game's world turned so that +y is up, the view a rotation and the projection the right
    // way up, and every point the game drew lands where it drew it.
    float view[12];
    lookAt({300, -250, 100}, {50, 0, 80}, {0, 1, 0}, view);  // uaRO's ground: y points down, the camera above it
    const RemixSpace s = remixSpaceFor(view);
    CHECK_EQ(s.flip[1], -1.0f);
    checkRemixCamera(view, 2194, 1234);
    checkRemixCamera(view, 1921, 1081);  // odd: the centre is not the middle
    // Up is up: a point above another in the game's world (smaller y) has the larger y in Remix's.
    float g[16], iv[16], a[3], b[3];
    affine4(view, g);
    CHECK(inverse4(g, iv));
    const Projection p = projectionFor(2194, 1234);
    float sx, sy, sz;
    project(p, view, V3{50, 0, 80}, sx, sy, sz);
    CHECK(remixPoint(p, iv, s, sx, sy, 1.0f / (view[2] * 50 + view[8] * 80 + view[11]), a));
    project(p, view, V3{50, -40, 80}, sx, sy, sz);
    CHECK(remixPoint(p, iv, s, sx, sy, 1.0f / (view[2] * 50 + view[5] * -40 + view[8] * 80 + view[11]), b));
    CHECK(b[1] > a[1]);
    // A world whose y points up keeps its y and is mirrored in x instead, to stay left-handed.
    float other[12];
    lookAt({300, 250, 100}, {50, 0, 80}, {0, -1, 0}, other);
    const RemixSpace t = remixSpaceFor(other);
    CHECK_EQ(t.flip[1], 1.0f);
    CHECK_EQ(t.flip[0], -1.0f);
    checkRemixCamera(other, 2194, 1234);
    // A picture flat on the screen (1/z of 1: nearer than the near plane) or no 1/z at all is no point of the world.
    CHECK(!remixPoint(p, iv, s, 100, 100, 1.0f, a));
    CHECK(!remixPoint(p, iv, s, 100, 100, 0.0f, a));
    CHECK(!remixPoint(p, iv, s, 100, 100, -0.01f, a));
}

TEST(game_camera_target_height_is_the_characters_ground) {
    // 2.2.2: the world height of the point the game's camera looks at (the character on the ground) from the view ->
    // world affine and that point's depth - the dungeons' low fog lies at it (five depths across the screen jumped as
    // walls' tops and the void came under them)
    for (const V3 at : {V3{50, 0, 80}, V3{50, -12.5f, 80}, V3{-30, 7, 140}}) {
        float view[12], v4[16], inv[16], toWorld[12];
        lookAt({at.x + 250, at.y - 250, at.z + 20}, at, {0, -1, 0}, view);
        affine4(view, v4);
        CHECK(inverse4(v4, inv));
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c) toWorld[3 * r + c] = inv[4 * r + c];
        const float a[3] = {at.x, at.y, at.z};
        CHECK(std::fabs(targetHeight(toWorld, viewZ(view, a)) - at.y) < 1e-3f);
    }
}

TEST(game_camera_up_in_the_view_raises_a_point) {
    // 2.7.22: an orb of light's light raised over its caster's head - the world's up (y down positive: -y) in the view,
    // by the view -> world affine
    const V3 at{50, -12.5f, 80};
    float view[12], v4[16], inv[16], toWorld[12];
    lookAt({at.x + 250, at.y - 250, at.z + 20}, at, {0, -1, 0}, view);
    affine4(view, v4);
    CHECK(inverse4(v4, inv));
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 3; ++c) toWorld[3 * r + c] = inv[4 * r + c];
    float up[3];
    upInView(toWorld, up);
    const float p[3] = {3, -4, 120}, q[3] = {p[0] + 15 * up[0], p[1] + 15 * up[1], p[2] + 15 * up[2]};
    float wp[3], wq[3];
    for (int k = 0; k < 3; ++k) {
        wp[k] = p[0] * toWorld[k] + p[1] * toWorld[3 + k] + p[2] * toWorld[6 + k] + toWorld[9 + k];
        wq[k] = q[0] * toWorld[k] + q[1] * toWorld[3 + k] + q[2] * toWorld[6 + k] + toWorld[9 + k];
    }
    CHECK(std::fabs(wq[1] - (wp[1] - 15)) < 1e-3f);  // 15 higher
    CHECK(std::fabs(wq[0] - wp[0]) < 1e-3f && std::fabs(wq[2] - wp[2]) < 1e-3f);  // nowhere else
}

