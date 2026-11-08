// 3.0.16-beta.16: the torches' and lamps' shadows - the cube's faces as the shader reads them
#include "core/actor_shadows.hpp"
#include "core/point_shadows.hpp"
#include <cmath>

#include "test.hpp"

using namespace uagfx;

TEST(point_shadows_a_way_lands_on_the_face_it_points_at) {
    const float ways[][3] = {{5, 1, -2}, {-5, 1, 2}, {1, 7, 3}, {1, -7, 3}, {2, -1, 9}, {2, 1, -9}};
    for (int k = 0; k < 6; ++k) {
        int f = -1;
        float u = 0, v = 0;
        CHECK(point_shadows::faceOf(ways[k], f, u, v));
        CHECK_EQ(f, k);
        // as the face's rows project it (shadow_vs): x = right, y = up, w = fwd
        const point_shadows::Face fc = point_shadows::face(f);
        float x = 0, y = 0, w = 0;
        for (int i = 0; i < 3; ++i) {
            x += ways[k][i] * fc.right[i];
            y += ways[k][i] * fc.up[i];
            w += ways[k][i] * fc.fwd[i];
        }
        CHECK(w > 0);
        CHECK(std::fabs(x / w - u) < 1e-5f && std::fabs(y / w - v) < 1e-5f);
        CHECK(std::fabs(u) <= 1.0f && std::fabs(v) <= 1.0f);
    }
    float none[3] = {0, 0, 0};
    int f = 0;
    float u = 0, v = 0;
    CHECK(!point_shadows::faceOf(none, f, u, v));
}

TEST(point_shadows_a_box_beside_a_face_is_left_out) {
    const float lo[3] = {4, -1, -1}, hi[3] = {6, 1, 1};  // ahead along +x
    CHECK(point_shadows::boxOnFace(lo, hi, 0));
    CHECK(!point_shadows::boxOnFace(lo, hi, 1));  // behind -x
    CHECK(!point_shadows::boxOnFace(lo, hi, 2));  // +y: wholly beside its pyramid (x 4 and more, y 1 at most)
    const float round[3] = {-2, -2, -2}, round2[3] = {2, 2, 2};  // about the light: every face
    for (int f = 0; f < 6; ++f) CHECK(point_shadows::boxOnFace(round, round2, f));
}

TEST(point_shadows_pick_the_lights_that_light_the_most) {
    std::vector<point_shadows::Candidate> c = {
        {{0, 0, 100}, 45, 0.4f},  // far
        {{0, 0, 20}, 45, 0.4f},   // near
        {{0, 0, 30}, 70, 0.5f},   // a lamp
        {{0, 0, 10}, 45, -0.3f},  // a ground light: none
    };
    const std::vector<int> p = point_shadows::pick(c, 10.0f, 2);
    CHECK(p.size() == 2);
    CHECK(p[0] == 2 && p[1] == 1);  // the lamp lights more of the screen (0.5 x 70 / 30) than the near torch
}

// 3.0.16-beta.18: a character's picture turned to face a torch, set behind it
TEST(point_shadows_a_character_faces_the_light) {
    // a quad facing the camera (along z): 6 wide, 16 tall (up is -y), its feet at the origin
    const float world[4][3] = {{-3, -16, 0}, {3, -16, 0}, {-3, 0, 0}, {3, 0, 0}};
    const float screen[4][2] = {{100, 50}, {130, 50}, {100, 130}, {130, 130}};
    const float uv[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    point_shadows::Standing s;
    CHECK(point_shadows::standingOf(world, screen, uv, s));
    CHECK(std::fabs(s.halfWidth - 3.0f) < 1e-4f);
    CHECK(std::fabs(s.bottom[1]) < 1e-4f && std::fabs(s.top[1] + 16.0f) < 1e-4f);
    CHECK(s.uv[0][1] == 1 && s.uv[2][1] == 0);  // the bottom's texture row first
    const float light[3] = {-30, -20, 0};  // off to its left (-x), up high
    float q[4][3];
    point_shadows::facing(s, light, point_shadows::kSpritePush, q);
    for (int c = 0; c < 4; ++c) CHECK(std::fabs(q[c][0] - 4.0f) < 1e-4f);  // across the way, half width + 1 behind
    CHECK(std::fabs(q[1][2] - q[0][2] - 6.0f) < 1e-4f || std::fabs(q[0][2] - q[1][2] - 6.0f) < 1e-4f);
    CHECK(std::fabs(q[2][1] + 16.0f) < 1e-4f);  // still standing as tall
}

// 3.0.16-beta.20: the characters' shadows leave out a light with a real shadow
TEST(point_shadows_no_second_shadow_from_a_shadowed_light) {
    const std::vector<actor_shadows::Light> lights = {{0, 0, 0, 45, 1}, {100, 0, 0, 45, 2}};
    const std::vector<std::array<float, 3>> shadowed = {{{3, -5, 2}}};
    const std::vector<actor_shadows::Light> left = actor_shadows::without(lights, shadowed);
    CHECK(left.size() == 1 && left[0].seed == 2);
    CHECK(actor_shadows::without(lights, {}).size() == 2);
}
