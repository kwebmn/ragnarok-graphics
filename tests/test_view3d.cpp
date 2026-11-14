// 3.0.19-beta.6 (the owner: «давай попробуем ... чтобы сразу получить качественную картинку»): the 3D view - the game's
// own camera replaced from the character's eyes or from behind
// (concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy)
#include <cmath>

#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
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

TEST(view3d_sky_by_the_night_and_under_a_vault) {
    const view3d::Sky day = view3d::skyOf(0.0f, false), night = view3d::skyOf(1.0f, false);
    const view3d::Sky vault = view3d::skyOf(0.0f, true);
    CHECK(day.horizon[2] > night.horizon[2] && day.horizon[0] > 0.5f);  // light by day, dark at night
    CHECK(vault.horizon[0] < 0.1f && vault.reach < day.reach);         // a dungeon: dark, the haze nearer
    CHECK(day.reach > 1000.0f);
}

// 3.0.19-beta.7: the game's wheel zoom moves the camera behind nearer or further back
// 3.0.19-beta.15 (R-YCGGJR: the game zoomed out to 521 put the camera 78 behind, into the houses): the 3D view starts
// kBehindDistance behind whatever the game's zoom; the wheel moves it from there by its share
TEST(view3d_the_wheel_sets_the_distance_behind) {
    CHECK(std::fabs(view3d::behindDistanceOf(300.0f, 300.0f) - view3d::kBehindDistance) < 1e-3f);
    CHECK(std::fabs(view3d::behindDistanceOf(521.6f, 521.6f) - view3d::kBehindDistance) < 1e-3f);  // any start
    CHECK(std::fabs(view3d::behindDistanceOf(600.0f, 300.0f) - 2.0f * view3d::kBehindDistance) < 1e-3f);
    CHECK(view3d::behindDistanceOf(150.0f, 600.0f) == view3d::kBehindNearest);  // in: no nearer than that
    CHECK(view3d::behindDistanceOf(2040.0f, 230.0f) == view3d::kBehindFarthest);
    CHECK(view3d::behindDistanceOf(-1.0f, 300.0f) == view3d::kBehindDistance);
    CHECK(view3d::behindDistanceOf(300.0f, 0.0f) == view3d::kBehindDistance);
    const float feet[3] = {0.0f, 0.0f, 0.0f}, ahead[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, -1.0f, 0.0f};
    float nearEye[3], farEye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, nearEye, target, 25.0f);
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, farEye, target, 120.0f);
    CHECK(farEye[0] < nearEye[0] - 90.0f);
}

// 3.0.19-beta.10: [view3d] - the field of view from the eyes and from behind, the mouse's speed; in «Гра» under its
// own heading «3D-режим», applied at once
TEST(config_view3d_defaults_and_limits) {
    const Config d = parseConfig("");
    CHECK(d.view3dFovFirst == 70 && d.view3dFovBehind == 60 && d.view3dMouse == 100);
    const Config c = parseConfig("[view3d]\nfov_first = 85\nfov_behind = 50\nmouse = 150\n");
    CHECK(c.view3dFovFirst == 85 && c.view3dFovBehind == 50 && c.view3dMouse == 150);
    const Config low = parseConfig("[view3d]\nfov_first = 10\nfov_behind = 10\nmouse = 1\n");
    CHECK(low.view3dFovFirst == 50 && low.view3dFovBehind == 40 && low.view3dMouse == 25);
    const Config high = parseConfig("[view3d]\nfov_first = 200\nfov_behind = 200\nmouse = 999\n");
    CHECK(high.view3dFovFirst == 100 && high.view3dFovBehind == 90 && high.view3dMouse == 300);
    CHECK(parseConfig("[view3d]\nfov_first = wide\n").view3dFovFirst == 70);
}

TEST(view3d_fov_from_the_settings) {
    CHECK(view3d::fovFor(view3d::Mode::First, 85, 50) == 85.0f);
    CHECK(view3d::fovFor(view3d::Mode::Behind, 85, 50) == 50.0f);
    CHECK(view3d::fovFor(view3d::Mode::Off, 85, 50) == 15.0f);
    CHECK(view3d::fovFor(view3d::Mode::First, 70, 60) == view3d::fovFor(view3d::Mode::First));
    CHECK(view3d::fovFor(view3d::Mode::Behind, 70, 60) == view3d::fovFor(view3d::Mode::Behind));
}

TEST(schema_view3d_rows_on_the_game_page) {
    const Config d = parseConfig("");
    const char* keys[] = {"view3d.fov_first", "view3d.fov_behind", "view3d.mouse"};
    const char* tokens[] = {"70", "60", "100"};
    for (int i = 0; i < 3; ++i) {
        const settings::IniSetting* s = settings::findSetting(keys[i]);
        CHECK(s != nullptr);
        if (!s) continue;
        CHECK(s->page == settings::Page::Game && s->control == settings::Control::Slider);
        CHECK(settings::tokenOf(*s, d) == tokens[i]);
        CHECK(appliesNow(keys[i]));
    }
}

TEST(live_config_takes_the_view3d_rows_at_once) {
    const Config running = parseConfig("");
    const Config file = parseConfig("[view3d]\nfov_first = 90\nfov_behind = 45\nmouse = 200\n");
    const LiveUpdate u = liveUpdate(running, file);
    CHECK(u.next.view3dFovFirst == 90 && u.next.view3dFovBehind == 45 && u.next.view3dMouse == 200);
    CHECK(u.applied.size() == 3);
}

// 3.0.19-beta.10: behind - the ground between the head and the eye (a wall, a cliff, the ground itself when looking
// up) brings the eye in front of it: the free distance back from the head along the view
TEST(view3d_behind_free_ground_keeps_the_distance) {
    const float feet[3] = {0.0f, 0.0f, 0.0f}, ahead[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, -1.0f, 0.0f};
    float eye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, -15.0f, eye, target, 45.0f);
    CHECK(view3d::freeDistance(eye, target, 45.0f, [](float, float) { return 0.0f; }) == 45.0f);
}

TEST(view3d_behind_a_wall_brings_the_eye_in_front_of_it) {
    const float feet[3] = {0.0f, 0.0f, 0.0f}, ahead[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, -1.0f, 0.0f};
    const auto wall = [](float x, float) { return x < -20.0f ? -100.0f : 0.0f; };  // 100 high behind x = -20
    float eye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, eye, target, 45.0f);
    const float free = view3d::freeDistance(eye, target, 45.0f, wall);
    CHECK(free >= 18.0f && free <= 20.0f);
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, eye, target, free);
    CHECK(eye[0] >= -20.0f && eye[0] < -10.0f);
}

TEST(view3d_behind_looking_up_comes_in_over_the_ground) {
    const float feet[3] = {0.0f, 0.0f, 0.0f}, ahead[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, -1.0f, 0.0f};
    float eye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 40.0f, eye, target, 45.0f);
    CHECK(eye[1] > 0.0f);  // under the ground at the whole distance
    const float free = view3d::freeDistance(eye, target, 45.0f, [](float, float) { return 0.0f; });
    CHECK(free < 45.0f && free > view3d::kPullNearest);
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 40.0f, eye, target, free);
    CHECK(eye[1] < -view3d::kPullMargin);  // over it
}

TEST(view3d_behind_against_a_wall_keeps_the_nearest) {
    const float feet[3] = {0.0f, 0.0f, 0.0f}, ahead[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, -1.0f, 0.0f};
    float eye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, eye, target, 45.0f);
    const auto wall = [](float x, float) { return x < -1.0f ? -100.0f : 0.0f; };  // right behind the head
    const float free = view3d::freeDistance(eye, target, 45.0f, wall);
    CHECK(free == view3d::kPullNearest);
}

// 3.0.19-beta.11 (the concept's decision: «Tab lock YES but the mouse stays free - only a light pull of the crosshair
// near the target»): the point the game's cursor is put at - on a monster under the crosshair, pulled into one near it
TEST(view3d_aim_on_a_monster_stays) {
    const std::vector<view3d::AimBox> boxes = {{7, 90, 80, 110, 120}};
    const view3d::Aim a = view3d::aimAt(boxes, 100, 100, 30, 0, 100);
    CHECK(a.id == 7 && a.x == 100 && a.y == 100);
}

TEST(view3d_aim_near_a_monster_pulled_into_it) {
    const std::vector<view3d::AimBox> boxes = {{7, 120, 80, 140, 120}};
    const view3d::Aim a = view3d::aimAt(boxes, 100, 100, 30, 0, 100);
    CHECK(a.id == 7);
    CHECK(a.x > 120 && a.x < 140 && a.y == 100);
}

TEST(view3d_aim_far_from_any_stays_at_the_crosshair) {
    const std::vector<view3d::AimBox> boxes = {{7, 200, 80, 220, 120}};
    const view3d::Aim a = view3d::aimAt(boxes, 100, 100, 30, 0, 100);
    CHECK(a.id == 0 && a.x == 100 && a.y == 100);
}

TEST(view3d_aim_the_nearer_of_two_and_the_one_under_it_first) {
    const std::vector<view3d::AimBox> two = {{7, 125, 80, 140, 120}, {8, 60, 80, 90, 120}};
    CHECK(view3d::aimAt(two, 100, 100, 30, 0, 100).id == 8);  // 10 off against 25
    const std::vector<view3d::AimBox> under = {{7, 95, 80, 140, 120}, {8, 60, 80, 99, 120}};
    CHECK(view3d::aimAt(under, 97, 100, 30, 8, 100).id == 8);  // both under it: the locked one
    const std::vector<view3d::AimBox> other = {{7, 90, 80, 110, 120}, {8, 150, 80, 170, 120}};
    CHECK(view3d::aimAt(other, 100, 100, 30, 8, 100).id == 7);  // the one under the crosshair before the locked one
}

TEST(view3d_aim_the_locked_one_pulls_from_farther) {
    const std::vector<view3d::AimBox> boxes = {{7, 160, 80, 180, 120}};  // 60 off
    CHECK(view3d::aimAt(boxes, 100, 100, 30, 0, 100).id == 0);
    const view3d::Aim a = view3d::aimAt(boxes, 100, 100, 30, 7, 100);
    CHECK(a.id == 7 && a.x > 160 && a.x < 180);
}

TEST(view3d_tab_locks_the_nearest_then_the_next) {
    const std::vector<view3d::AimBox> boxes = {{1, 190, 90, 200, 110}, {2, 105, 90, 115, 110}, {3, 140, 90, 150, 110}};
    CHECK(view3d::nextLock(boxes, 100, 100, 0) == 2);
    CHECK(view3d::nextLock(boxes, 100, 100, 2) == 3);
    CHECK(view3d::nextLock(boxes, 100, 100, 3) == 1);
    CHECK(view3d::nextLock(boxes, 100, 100, 1) == 2);   // round again
    CHECK(view3d::nextLock(boxes, 100, 100, 99) == 2);  // the locked one gone: the nearest
    CHECK(view3d::nextLock({}, 100, 100, 2) == 0);      // none on the screen: unlocked
}

TEST(view3d_monster_classes) {
    CHECK(!view3d::monsterClass(0) && !view3d::monsterClass(45) && !view3d::monsterClass(1000));
    CHECK(view3d::monsterClass(1001) && view3d::monsterClass(1002) && view3d::monsterClass(3999));
    CHECK(!view3d::monsterClass(4000));
}

// 3.0.19-beta.12 (R-ZQWGVD: behind, the eye went into Morocc's buildings - a roof edge-on, a striped slab): the game's
// boxes of things (a model's OBB, a ground node's) - where the way back from the head goes into one
TEST(view3d_box_entered_on_the_way) {
    view3d::Box b;  // 10 x 10 x 10 round (-30, 0, 0), the world's axes
    b.centre[0] = -30.0f;
    b.half[0] = b.half[1] = b.half[2] = 5.0f;
    const float head[3] = {0.0f, 0.0f, 0.0f}, eye[3] = {-50.0f, 0.0f, 0.0f};
    CHECK(near(view3d::entersAt(b, head, eye), 0.5f));  // at x = -25 of the 50
    const float aside[3] = {-50.0f, 0.0f, 20.0f};
    CHECK(view3d::entersAt(b, head, aside) < 0.0f);  // past its side
    const float shortEye[3] = {-20.0f, 0.0f, 0.0f};
    CHECK(view3d::entersAt(b, head, shortEye) < 0.0f);  // ends before it
    const float inside[3] = {-30.0f, 0.0f, 0.0f};
    CHECK(view3d::entersAt(b, inside, eye) < 0.0f);  // the head in it: not this one's
    CHECK(view3d::crosses(b, inside, eye) && view3d::crosses(b, head, eye) && !view3d::crosses(b, head, shortEye));
}

TEST(view3d_box_turned_and_scaled_axes) {
    view3d::Box b;  // turned 45 degrees round y, its axes twice as long (the half sizes in their units)
    const float k = 2.0f * 0.70710678f;
    b.centre[0] = -30.0f;
    b.axis[0][0] = k, b.axis[0][2] = k;
    b.axis[1][1] = 2.0f;
    b.axis[2][0] = -k, b.axis[2][2] = k;
    b.half[0] = b.half[1] = b.half[2] = 2.5f;  // 5 world units
    const float head[3] = {0.0f, 0.0f, 0.0f}, eye[3] = {-50.0f, 0.0f, 0.0f};
    const float t = view3d::entersAt(b, head, eye);
    CHECK(t > 0.0f && near(t * 50.0f, 30.0f - 5.0f * 1.41421356f, 0.05f));  // its corner towards the head
}

TEST(view3d_behind_brought_in_before_a_building) {
    const float feet[3] = {0.0f, 0.0f, 0.0f}, ahead[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, -1.0f, 0.0f};
    float eye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, eye, target, 60.0f);
    view3d::Box house;  // 40 wide, 30 high, from x = -30 back
    house.centre[0] = -50.0f, house.centre[1] = -15.0f, house.centre[2] = 0.0f;
    house.half[0] = 20.0f, house.half[1] = 15.0f, house.half[2] = 30.0f;
    const float free = view3d::freeOfBoxes(eye, target, 60.0f, {house});
    CHECK(free < 30.0f && free >= view3d::kPullNearest);
    CHECK(view3d::freeOfBoxes(eye, target, 60.0f, {}) == 60.0f);
    view3d::Box around = house;  // the head in it (a hall): no pull
    around.centre[0] = 0.0f, around.half[0] = 100.0f;
    CHECK(view3d::freeOfBoxes(eye, target, 60.0f, {around}) == 60.0f);
}
