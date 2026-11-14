// 3.0.19-beta.6 (the owner: «давай попробуем ... чтобы сразу получить качественную картинку»): the 3D view - the game's
// own camera replaced from the character's eyes or from behind
// (concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy)
#include <cmath>

#include "core/view3d.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
}  // namespace

TEST(view3d_modes_cycle_normal_behind_first) {
    CHECK(view3d::next(view3d::Mode::Off) == view3d::Mode::Behind);
    CHECK(view3d::next(view3d::Mode::Behind) == view3d::Mode::First);
    CHECK(view3d::next(view3d::Mode::First) == view3d::Mode::Off);
    CHECK(view3d::fovFor(view3d::Mode::Off) == 15.0f);  // the game's own
    CHECK(view3d::fovFor(view3d::Mode::First) > 60.0f);
    CHECK(view3d::fovFor(view3d::Mode::Behind) > 45.0f);
}

TEST(view3d_mouse_turns_by_the_sign_and_holds_the_pitch) {
    view3d::Look l;
    view3d::turn(l, 100.0f, 0.0f, 0.15f, 1.0f);
    CHECK(near(l.yaw, 15.0f));
    view3d::turn(l, 100.0f, 0.0f, 0.15f, -1.0f);  // the game's yaw the other way round
    CHECK(near(l.yaw, 0.0f));
    view3d::turn(l, -100.0f, 0.0f, 0.15f, 1.0f);
    CHECK(near(l.yaw, 345.0f));  // kept in 0-360 as the game keeps its own
    view3d::turn(l, 0.0f, -100.0f, 0.15f, 1.0f);  // the mouse up (y less): looking up
    CHECK(near(l.pitch, 15.0f));
    view3d::turn(l, 0.0f, -10000.0f, 0.15f, 1.0f);
    CHECK(near(l.pitch, view3d::kPitchMost));
    view3d::turn(l, 0.0f, 10000.0f, 0.15f, 1.0f);
    CHECK(near(l.pitch, -view3d::kPitchMost));
}

TEST(view3d_first_person_eye_over_the_feet_looking_along_the_pitch) {
    const float feet[3] = {100.0f, -5.0f, 200.0f};  // y down: up is -y
    const float ahead[3] = {0.0f, 0.0f, 1.0f};
    const float up[3] = {0.0f, -1.0f, 0.0f};
    float eye[3], target[3];
    view3d::place(view3d::Mode::First, feet, ahead, up, 0.0f, eye, target);
    CHECK(near(eye[0], 100.0f) && near(eye[1], -5.0f - view3d::kEyeHeight) && near(eye[2], 200.0f));
    CHECK(near(target[0] - eye[0], 0.0f) && near(target[1] - eye[1], 0.0f) && target[2] > eye[2]);
    view3d::place(view3d::Mode::First, feet, ahead, up, 30.0f, eye, target);  // looking up: target higher (less y)
    CHECK(target[1] < eye[1]);
}

TEST(view3d_behind_the_eye_back_and_above_the_head) {
    const float feet[3] = {0.0f, 0.0f, 0.0f};
    const float ahead[3] = {1.0f, 0.0f, 0.0f};
    const float up[3] = {0.0f, -1.0f, 0.0f};
    float eye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, -15.0f, eye, target);
    CHECK(eye[0] < -20.0f);   // behind: against the way it looks
    CHECK(eye[1] < -10.0f);   // above the head (y down)
    CHECK(target[0] > eye[0]);
}

TEST(view3d_look_at_as_the_game_builds_it) {
    // the game's LookAt 0x409540: right = up x forward, then forward x right; the last row the move
    const float eye[3] = {10.0f, -20.0f, 30.0f};
    const float at[3] = {10.0f, -20.0f, 40.0f};
    const float up[3] = {0.0f, -1.0f, 0.0f};
    float m[12];
    view3d::lookAt(eye, at, up, m);
    // a point ahead lands at view z = its distance, x = y = 0
    const float p[3] = {10.0f, -20.0f, 55.0f};
    const float x = p[0] * m[0] + p[1] * m[3] + p[2] * m[6] + m[9];
    const float y = p[0] * m[1] + p[1] * m[4] + p[2] * m[7] + m[10];
    const float z = p[0] * m[2] + p[1] * m[5] + p[2] * m[8] + m[11];
    CHECK(near(x, 0.0f) && near(y, 0.0f) && near(z, 25.0f));
    float inv[12];
    view3d::inverse(m, inv);
    // the inverse takes view space back to the world: the eye at the origin
    CHECK(near(inv[9], 10.0f) && near(inv[10], -20.0f) && near(inv[11], 30.0f));
}

TEST(view3d_forward_and_turn_sign_from_the_games_own_matrix) {
    // the game's matrix for yaw 0: forward +z, its right column whatever its up made it
    const float eye[3] = {0.0f, -50.0f, -50.0f};
    const float at[3] = {0.0f, 0.0f, 0.0f};
    const float up[3] = {0.0f, -1.0f, 0.0f};
    float m[12];
    view3d::lookAt(eye, at, up, m);
    float ahead[3];
    CHECK(view3d::ahead(m, ahead));
    CHECK(near(ahead[0], 0.0f) && near(ahead[1], 0.0f) && near(ahead[2], 1.0f));
    // the yaw grows toward (sin, 0, cos): +x at yaw 0 - the screen's right or left by the right column
    const float sign = view3d::turnSign(m, 0.0f);
    const float right[3] = {m[0], m[3], m[6]};
    CHECK(sign == (right[0] > 0.0f ? 1.0f : -1.0f));
}
