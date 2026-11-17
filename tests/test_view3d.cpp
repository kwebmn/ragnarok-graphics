// 3.0.19-beta.6 (the owner: «давай попробуем ... чтобы сразу получить качественную картинку»): the 3D view - the game's
// own camera replaced from the character's eyes or from behind
// (concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy)
#include <cmath>
#include <cstdint>
#include <limits>

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

// 3.0.19-beta.18 (R-57KBSF «некоторые нпс утоплены в пол»): the game keeps a sprite a few units nearer than its feet
// by its rhw (~4.7 at its own distance 300); seen from the eyes that is a hundredth of a unit, the ground in front of
// the feet over them. In the 3D view a character's, a monster's, an NPC's sprite comes kSpriteNearer of its distance
// nearer (kSpriteNearerMost at most), never past the near plane; behind it (z below 0) or behind the eye kept as it is
namespace {
float gameZ(float viewZ) { return 1500.0f / 1490.0f * (1.0f - 10.0f / viewZ); }
}  // namespace

TEST(view3d_sprites_nearer_by_their_distance) {
    CHECK(near(view3d::spriteDepth(gameZ(100.0f), 0.01f), gameZ(96.0f), 1e-6f));    // 4 of 100
    CHECK(near(view3d::spriteDepth(gameZ(50.0f), 0.02f), gameZ(48.0f), 1e-6f));     // 2 of 50
    CHECK(near(view3d::spriteDepth(gameZ(400.0f), 0.0025f), gameZ(392.0f), 1e-6f));  // 8 at most
    CHECK(view3d::spriteDepth(gameZ(10.2f), 1.0f / 10.2f) == 0.0f);  // to the near plane, not past it
    const float tooNear = gameZ(5.0f);                               // nearer than the near plane: cut as before
    CHECK(view3d::spriteDepth(tooNear, 0.2f) == tooNear);
    CHECK(view3d::spriteDepth(0.5f, -0.1f) == 0.5f && view3d::spriteDepth(0.5f, 0.0f) == 0.5f);  // behind the eye
    float last = -1.0f;  // the farther stays the farther
    for (float d = 11.0f; d < 1500.0f; d *= 1.3f) {
        const float z = view3d::spriteDepth(gameZ(d), 1.0f / d);
        CHECK(z > last && z < gameZ(d));
        last = z;
    }
}

// 3.0.19-beta.18 (R-57KBSF «вижу свою или голову пеко ... когда иду и она мигает»): behind, the character's own
// picture left out under kOwnNear and shown again only past kOwnShownAgain - the eye pulled in and let out by a step
// at the walls no longer turns it on and off; from the eyes always out, the game's own view never
TEST(view3d_own_picture_out_near_with_a_margin) {
    using view3d::Mode;
    CHECK(view3d::ownOut(Mode::First, 0.0f, false) && view3d::ownOut(Mode::First, 80.0f, false));
    CHECK(!view3d::ownOut(Mode::Off, 5.0f, true));
    CHECK(!view3d::ownOut(Mode::Behind, 30.0f, false) && !view3d::ownOut(Mode::Behind, 30.0f, true));
    CHECK(view3d::ownOut(Mode::Behind, 11.0f, false));
    CHECK(view3d::ownOut(Mode::Behind, 14.0f, true) && !view3d::ownOut(Mode::Behind, 14.0f, false));
    CHECK(!view3d::ownOut(Mode::Behind, view3d::kOwnShownAgain + 0.5f, true));
    CHECK(!view3d::ownOut(Mode::Behind, 0.0f, false) && !view3d::ownOut(Mode::Behind, 0.0f, true));  // not yet known
    CHECK(view3d::kOwnNear < view3d::kOwnShownAgain);
}

// 3.0.19-beta.24 (R-T9FPZZ «лучше но еще не то», 49.7 fps: the renderer's drawing 14.4 ms a frame, 8.9 of it the
// game's own draws - from the eyes it draws all to 1500 ahead -, the game's thread 11.3): [view3d] distance - how far
// the game culls its world in the 3D view (its frustum's far 0x69C5DC), the haze whole there, the own ground just past
TEST(config_view3d_distance_defaults_and_limits) {
    CHECK(parseConfig("").view3dDistance == view3d::kSightDefault);
    CHECK(parseConfig("[view3d]\ndistance = 1000\n").view3dDistance == 1000);
    CHECK(parseConfig("[view3d]\ndistance = 100\n").view3dDistance == view3d::kSightLeast);
    CHECK(parseConfig("[view3d]\ndistance = 9000\n").view3dDistance == view3d::kSightMost);
    CHECK(parseConfig("[view3d]\ndistance = far\n").view3dDistance == view3d::kSightDefault);
    CHECK(view3d::kSightLeast == 800 && view3d::kSightMost == 1500 && view3d::kSightDefault == 1200);
    const settings::IniSetting* row = settings::findSetting("view3d.distance");
    CHECK(row != nullptr);
    if (row) {
        CHECK(row->page == settings::Page::Game && row->control == settings::Control::Slider);
        CHECK(settings::tokenOf(*row, parseConfig("")) == "1200");
    }
    CHECK(appliesNow("view3d.distance"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[view3d]\ndistance = 900\n"));
    CHECK(u.next.view3dDistance == 900 && u.applied.size() == 1);
}

TEST(view3d_haze_whole_at_the_sight) {
    const float outdoors = view3d::skyOf(0.0f, false).reach, vault = view3d::skyOf(0.0f, true).reach;
    CHECK(view3d::hazeReach(outdoors, 1500.0f) == 1500.0f - view3d::kHazeWholePast);
    CHECK(view3d::hazeReach(outdoors, 1000.0f) == 1000.0f - view3d::kHazeWholePast);
    CHECK(view3d::hazeReach(vault, 1200.0f) == vault);  // a dungeon's nearer already
    CHECK(view3d::hazeReach(outdoors, 0.0f) > 0.0f);     // nonsense: never none
}

// 3.0.19-beta.31 (the testers: in the 3D view «на себе неможливо накласти бафи, аля хіл ітд, і неможливо скастувати на
// моба скіл який вимагає кліка по мобу»): whom a skill the game waits a target for goes to at a left press
TEST(view3d_skill_target_at_a_press) {
    constexpr uint32_t own = 2000001, monster = 110000123, player = 2000777, homun = 110000999, npc = 110000555;
    using view3d::skillTargetOf;
    // an enemy's skill: the monster under the crosshair, else the one it is pulled onto, else none (the game's)
    CHECK_EQ(skillTargetOf(2, monster, 1002, 0, own, false), monster);
    CHECK_EQ(skillTargetOf(2, 0, -1, monster, own, false), monster);
    CHECK_EQ(skillTargetOf(2, 0, -1, 0, own, false), 0u);
    CHECK_EQ(skillTargetOf(2, npc, 101, monster, own, false), monster);  // an NPC under it: the pulled one
    CHECK_EQ(skillTargetOf(2, npc, 45, 0, own, false), 0u);              // a warp
    CHECK_EQ(skillTargetOf(2, player, 4010, 0, own, false), player);     // another player (the castles' wars)
    // a friend's: a player, a homunculus under it; a monster with Shift (as the game wants); else the character
    CHECK_EQ(skillTargetOf(4, player, 4010, 0, own, false), player);
    CHECK_EQ(skillTargetOf(4, player, 7, 0, own, false), player);
    CHECK_EQ(skillTargetOf(4, homun, 6001, 0, own, false), homun);
    CHECK_EQ(skillTargetOf(4, monster, 1002, monster, own, false), own);  // a Heal while fighting: on itself
    CHECK_EQ(skillTargetOf(4, monster, 1002, monster, own, true), monster);
    CHECK_EQ(skillTargetOf(4, 0, -1, 0, own, false), own);
    CHECK_EQ(skillTargetOf(4, npc, 101, 0, own, false), own);
    CHECK_EQ(skillTargetOf(4, own, 4010, 0, own, false), own);
    // no wait, a place's (1), a trap's (5): the game's
    CHECK_EQ(skillTargetOf(0, monster, 1002, monster, own, false), 0u);
    CHECK_EQ(skillTargetOf(1, monster, 1002, monster, own, false), 0u);
    CHECK_EQ(skillTargetOf(5, monster, 1002, monster, own, false), 0u);
}

// The homunculus' and the mercenary's own skills go through those (uaRO.exe FUN_00562580): left to the game
TEST(view3d_companion_skills_left_to_the_game) {
    using view3d::companionSkill;
    CHECK(companionSkill(8000) && companionSkill(8013) && companionSkill(8016));  // a homunculus'
    CHECK(companionSkill(8200) && companionSkill(8237));                          // a mercenary's
    CHECK(!companionSkill(7999) && !companionSkill(8017) && !companionSkill(8199) && !companionSkill(8238));
    CHECK(!companionSkill(28) && !companionSkill(271));  // Heal, Asura
}

// 3.0.19-beta.32: [view3d] vault - a dungeon's vault in the 3D view (core/cave_vault), on unless the file says 0; in
// «Гра» under «3D-режим»; an effect's field the renderers get (EffectSettings), at once
TEST(config_view3d_vault_on_by_default_and_live) {
    CHECK(parseConfig("").effects.caveVault);
    CHECK(!parseConfig("[view3d]\nvault = 0\n").effects.caveVault);
    CHECK(parseConfig("[view3d]\nvault = 1\n").effects.caveVault);
    const settings::IniSetting* row = settings::findSetting("view3d.vault");
    CHECK(row != nullptr);
    if (row) {
        CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
        CHECK(settings::tokenOf(*row, parseConfig("")) == "1");
    }
    CHECK(appliesNow("view3d.vault"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[view3d]\nvault = 0\n"));
    CHECK(!u.next.effects.caveVault);
    CHECK(!(parseConfig("").effects == parseConfig("[view3d]\nvault = 0\n").effects));  // the renderer told
}

// 3.0.19-beta.34 (the 3D view's concept: «Колесо: ... от первого лица: смена цели среди тех, что в прицеле»): the
// wheel from the eyes steps the lock either way - the one before as the next one's way back, round
TEST(view3d_lock_steps_back_too) {
    const std::vector<view3d::AimBox> boxes = {{1, 300, 80, 340, 120}, {2, 90, 90, 110, 110}, {3, 140, 90, 160, 110}};
    CHECK(view3d::nextLock(boxes, 100, 100, 2, -1) == 1);  // 2 the nearest, 3 next, 1 last: before 2 comes 1
    CHECK(view3d::nextLock(boxes, 100, 100, 1, -1) == 3);
    CHECK(view3d::nextLock(boxes, 100, 100, 0, -1) == 2);  // none locked: the nearest either way
    CHECK(view3d::nextLock(boxes, 100, 100, 2, 1) == 3);   // as before
}

// 3.0.19-beta.36 (the owner: «автонаводка на мобов раздражает, можно без нее?»): [view3d] aim_pull - the crosshair
// pulled onto a monster near it and the locked one, off unless the file says 1; off: the monster right under it alone
TEST(config_view3d_aim_pull_off_by_default) {
    CHECK(!parseConfig("").view3dAimPull);
    CHECK(parseConfig("[view3d]\naim_pull = 1\n").view3dAimPull);
    const settings::IniSetting* row = settings::findSetting("view3d.aim_pull");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && settings::tokenOf(*row, parseConfig("")) == "0");
    CHECK(appliesNow("view3d.aim_pull"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[view3d]\naim_pull = 1\n")).next.view3dAimPull);
    // no pull: the one under the crosshair as it is, none near it, not the locked one from afar
    const std::vector<view3d::AimBox> boxes = {{5, 90, 90, 110, 110}, {6, 130, 90, 150, 110}};
    const view3d::Aim under = view3d::aimAt(boxes, 100, 100, 0, 6, 0);
    CHECK(under.id == 5 && under.x == 100 && under.y == 100);
    const view3d::Aim none = view3d::aimAt(boxes, 120, 100, 0, 6, 0);
    CHECK(none.id == 0 && none.x == 120 && none.y == 100);
}

// 3.0.19-beta.36 (the concept's threat's arc): [view3d] threat - on unless the file says 0
TEST(config_view3d_threat_on_by_default) {
    CHECK(parseConfig("").view3dThreat);
    CHECK(!parseConfig("[view3d]\nthreat = 0\n").view3dThreat);
    CHECK(settings::findSetting("view3d.threat") != nullptr);
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[view3d]\nthreat = 0\n")).next.view3dThreat);
}

// 3.0.19-beta.37 (the owner: «можнол попробовать» - the concept's photo mode with a free camera): the free camera's
// step - along the view, aside, up - at its speed (Shift faster), kept within its reach of the character's feet
TEST(view3d_free_camera_flies_within_its_reach) {
    const float f[3] = {0, 0, 1}, right[3] = {1, 0, 0}, feet[3] = {0, 0, 0};
    float eye[3] = {0, -13, 0};
    view3d::flyStep(eye, f, right, 1, 0, 0, 1.0f, false, feet);  // ahead a second
    CHECK(std::fabs(eye[2] - view3d::kFreeSpeed) < 1e-3f && std::fabs(eye[0]) < 1e-3f);
    view3d::flyStep(eye, f, right, 0, -1, 0, 0.5f, true, feet);  // to the left, fast, half a second
    CHECK(std::fabs(eye[0] + view3d::kFreeSpeed * view3d::kFreeFast * 0.5f) < 1e-3f);
    const float y0 = eye[1];
    view3d::flyStep(eye, f, right, 0, 0, 1, 0.25f, false, feet);  // up: the world's y down
    CHECK(eye[1] < y0 - 9.0f);
    float far[3] = {0, -13, 240};
    view3d::flyStep(far, f, right, 1, 0, 0, 2.0f, false, feet);  // past the reach: kept at it
    const float d = std::sqrt(far[0] * far[0] + far[1] * far[1] + far[2] * far[2]);
    CHECK(d <= view3d::kFreeReach + 1e-2f && d > view3d::kFreeReach - 1.0f);
    float still[3] = {5, -13, 5};
    view3d::flyStep(still, f, right, 0, 0, 0, 1.0f, true, feet);  // no key: where it was
    CHECK(still[0] == 5 && still[1] == -13 && still[2] == 5);
}

// 3.0.19-beta.38 (R-S3NQBF: in the 3D view the monsters' HP bars stood in the air over the hills hiding them): whether
// the ground lets the eye see a place - the way between them sampled, a ground point over it hides it
TEST(view3d_ground_hides_what_is_behind_a_hill) {
    const float eye[3] = {0, -14, 0}, monster[3] = {0, -8, 100};  // y down: the eye 14 up, the monster's head 8 up
    const auto flat = [](float, float) { return 0.0f; };
    CHECK(view3d::groundLetsSee(eye, monster, flat));
    const auto hill = [](float, float z) { return z > 40 && z < 60 ? -20.0f : 0.0f; };  // 20 up halfway
    CHECK(!view3d::groundLetsSee(eye, monster, hill));
    const auto low = [](float, float z) { return z > 40 && z < 60 ? -9.0f : 0.0f; };  // under the way there (11)
    CHECK(view3d::groundLetsSee(eye, monster, low));
    const auto atItsFeet = [](float, float z) { return z > 97 ? -10.0f : 0.0f; };  // right at it: its own ground
    CHECK(view3d::groundLetsSee(eye, monster, atItsFeet));
}

// 3.0.19-beta.39 (the owner's «делаем все по порядку»: the HP bars hidden behind the map's models too - a house, a
// castle's wall): whether a model's box stands between the eye and a place - the way going into its core (kHideShare of
// its half sizes) from outside, the place itself out of it (under a tree's crown, inside a house's box: not hidden)
TEST(view3d_model_box_hides_what_is_behind_it) {
    view3d::Box house;
    house.centre[0] = 0, house.centre[1] = -10, house.centre[2] = 50;
    house.half[0] = 10, house.half[1] = 10, house.half[2] = 10;
    const float eye[3] = {0, -14, 0};
    const float behind[3] = {0, -8, 100}, before[3] = {0, -8, 30}, inside[3] = {0, -8, 50};
    CHECK(view3d::boxHides(house, eye, behind));
    CHECK(!view3d::boxHides(house, eye, before));
    CHECK(!view3d::boxHides(house, eye, inside));
    const float beside[3] = {25, -8, 100};  // the way passes its side
    CHECK(!view3d::boxHides(house, eye, beside));
    const float eyeInside[3] = {0, -12, 50};  // the eye in it (a camera under a tree): nothing hidden by it
    CHECK(!view3d::boxHides(house, eyeInside, behind));
}

// 3.0.19-beta.39 (the owner's «делаем все по порядку»: from the eyes the character's own effects - Kyrie's barrier,
// Assumptio, the spheres round it - stood in the camera): an effect's draw at the eyes (its vertices' mean depth from
// them, 1 / rhw) left out - one of the character's nearer than kOwnEffectNear, anyone's nearer than kAnyEffectNear
TEST(view3d_effects_at_the_eyes_left_out) {
    struct V { float x, y, z, rhw; };
    const V barrier[4] = {
        {0, 0, 0.1f, 1 / 6.0f}, {1, 0, 0.1f, 1 / 8.0f}, {0, 1, 0.1f, 1 / 7.0f}, {1, 1, 0.1f, 1 / 9.0f}};
    const V bolt[4] = {
        {0, 0, 0.5f, 1 / 60.0f}, {1, 0, 0.5f, 1 / 61.0f}, {0, 1, 0.5f, 1 / 62.0f}, {1, 1, 0.5f, 1 / 63.0f}};
    CHECK(view3d::effectAtTheEyes(barrier, 4, sizeof(V), true));    // the character's, 7.5 away: out
    CHECK(!view3d::effectAtTheEyes(barrier, 4, sizeof(V), false));  // another's at 7.5: kept
    CHECK(!view3d::effectAtTheEyes(bolt, 4, sizeof(V), true));      // flying off, 61 away: kept
    const V face[3] = {{0, 0, 0.0f, 1 / 2.0f}, {1, 0, 0.0f, 1 / 3.0f}, {0, 1, 0.0f, 1 / 4.0f}};
    CHECK(view3d::effectAtTheEyes(face, 3, sizeof(V), false));  // anyone's at the very eyes: out
    const V behind[3] = {{0, 0, 0, -1.0f}, {1, 0, 0, -1.0f}, {0, 1, 0, -1.0f}};
    CHECK(!view3d::effectAtTheEyes(behind, 3, sizeof(V), true));  // no depth known: kept
}

// 3.0.20-beta.78 (R-SCCWB2 «кудато пропал урон», R-HZD6EP «нету цифр урона ... в 3д режиме, в 2д есть»): the game keeps
// its damage numbers, Miss, Critical and the target's cursor over everything by drawing them AT its near plane,
// wherever they stand (uaRO.exe FUN_00546C50, a render object's +0x8E: rhw 1 / 10 less 20 steps of 9e-6, a step more
// for its layer and for each clip of its frame) - and [view3d] near_fade, which dissolves what is right at the
// camera (3.0.20-beta.70), took every one of them for a surface at the eye. Known by that depth: every vertex on one
// of the game's own steps there; a sprite that only stands near the camera is not one
namespace {
struct TopV {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u, v;
};
float onTopRhw(int layer, int clip) {  // as the game works it out
    const float step = 9.0e-6f;
    return float(layer) * step + ((1.0f / 10.0f + float(clip) * step) - step * 20.0f);
}
std::vector<TopV> spriteQuad(float top, float bottom) {
    return {{0, 0, 0, top, 0, 0, 0, 0}, {9, 0, 0, top, 0, 0, 1, 0}, {0, 9, 0, bottom, 0, 0, 0, 1},
            {9, 9, 0, bottom, 0, 0, 1, 1}};
}
bool overAll(float top, float bottom) {
    const std::vector<TopV> q = spriteQuad(top, bottom);
    return view3d::overAll(q.data(), 4, sizeof(TopV));
}
}  // namespace

TEST(view3d_the_games_sprites_kept_over_everything_known_by_their_depth) {
    CHECK(near(1.0f / onTopRhw(0, 0), 10.018f, 1e-3f));  // a damage digit: 10.018 from the eye wherever it stands
    CHECK(overAll(onTopRhw(0, 0), onTopRhw(0, 0)));
    for (int layer = -3; layer <= 3; ++layer)  // an object's layer, a frame's clips
        for (int clip = 0; clip < 12; ++clip) CHECK(overAll(onTopRhw(layer, clip), onTopRhw(layer, clip)));
    CHECK(overAll(onTopRhw(-1, 0), onTopRhw(-1, 1)));  // two of its steps in a draw
    // a monster's sprite near the camera: dissolved as the walls are
    CHECK(!overAll(1.0f / 10.5f, 1.0f / 10.5f));
    CHECK(!overAll(1.0f / 12.0f, 1.0f / 12.0f));
    CHECK(!overAll(1.0f / 56.0f, 1.0f / 56.0f));
    // one that stands at a digit's own depth but between the game's steps (10.0184 from the eye), or past them
    CHECK(!overAll(onTopRhw(0, 0) + 0.4f * 9.0e-6f, onTopRhw(0, 0) + 0.4f * 9.0e-6f));
    CHECK(!overAll(onTopRhw(0, 0) - 30.0f * 9.0e-6f, onTopRhw(0, 0) - 30.0f * 9.0e-6f));
    CHECK(!overAll(1.0f / 9.5f, 1.0f / 9.5f));  // nearer than the near plane: the card cuts it
    CHECK(!overAll(onTopRhw(0, 0), 1.0f / 11.0f));  // a surface through that depth, its other end farther
    CHECK(!overAll(-onTopRhw(0, 0), -onTopRhw(0, 0)) && !overAll(0.0f, 0.0f));  // behind the eye, no depth
    const std::vector<TopV> digit = spriteQuad(onTopRhw(0, 0), onTopRhw(0, 0));
    CHECK(!view3d::overAll(nullptr, 4, sizeof(TopV)) && !view3d::overAll(digit.data(), 0, sizeof(TopV)));
    CHECK(!view3d::overAll(digit.data(), 4, 12));  // vertices with no rhw
    struct Wide {  // the ground's vertex kind (two texture coordinates): the same place for rhw
        TopV v;
        float u1, v1;
    };
    const Wide wide[3] = {{digit[0], 0, 0}, {digit[1], 0, 0}, {digit[2], 0, 0}};
    CHECK(view3d::overAll(wide, 3, sizeof(Wide)));
}

TEST(view3d_what_dissolves_right_at_the_camera) {
    CHECK(view3d::nearFades(true, true, false, false));    // the world's draw in the 3D view
    CHECK(!view3d::nearFades(true, true, false, true));    // a damage number, Miss, the target's cursor: whole
    CHECK(!view3d::nearFades(true, true, true, false));    // the interface
    CHECK(!view3d::nearFades(true, false, false, false));  // the game's own view: as the game draws it
    CHECK(!view3d::nearFades(false, true, false, false));  // [view3d] near_fade off
}

// 3.0.19-beta.39 (the owner's «делаем все по порядку»: the character standing turned where the camera looks): the
// game's facing (actor + 0x38, degrees: 0 south, 90 east, 180 north - uaRO.exe's /bangbang, FUN_00567900) for the
// view's flat way, the nearest of the server's 8 directions (((int)angle + 180) / 45 & 7: 0 north, 2 west, 6 east);
// kept while the way is within kFaceKeep of the facing
TEST(view3d_character_faces_the_view) {
    view3d::Facing f;
    CHECK(view3d::faceToward(0.0f, 1.0f, 0.0f, f) && f.angle == 180.0f && f.dir == 0);    // north
    CHECK(view3d::faceToward(1.0f, 0.0f, 180.0f, f) && f.angle == 90.0f && f.dir == 6);   // east
    CHECK(view3d::faceToward(-1.0f, 0.0f, 180.0f, f) && f.angle == 270.0f && f.dir == 2); // west
    CHECK(view3d::faceToward(0.0f, -1.0f, 180.0f, f) && f.angle == 0.0f && f.dir == 4);   // south
    CHECK(view3d::faceToward(-0.7071f, 0.7071f, 0.0f, f) && f.angle == 225.0f && f.dir == 1);  // north-west
    const float k = 205.0f * 3.14159265f / 180.0f;  // 25 off north: within kFaceKeep, kept
    CHECK(!view3d::faceToward(std::sin(k), -std::cos(k), 180.0f, f));
    const float a = 215.0f * 3.14159265f / 180.0f;  // 35 off north: the next step (225)
    CHECK(view3d::faceToward(std::sin(a), -std::cos(a), 180.0f, f) && f.angle == 225.0f && f.dir == 1);
    CHECK(!view3d::faceToward(std::sin(a), -std::cos(a), 225.0f, f));  // already that way
    const float w = 10.0f * 3.14159265f / 180.0f;  // over the round: 10 degrees against 350
    CHECK(!view3d::faceToward(std::sin(w), -std::cos(w), 350.0f, f));
    CHECK(!view3d::faceToward(0.0f, 0.0f, 0.0f, f));  // no way
    CHECK(view3d::serverDir(180.0f) == 0 && view3d::serverDir(90.0f) == 6 && view3d::serverDir(315.0f) == 3);
}

// 3.0.19-beta.39: [view3d] face - standing, the character turned where the camera looks; on unless the file says 0
TEST(config_view3d_face_on_by_default) {
    CHECK(parseConfig("").view3dFace);
    CHECK(!parseConfig("[view3d]\nface = 0\n").view3dFace);
    const settings::IniSetting* row = settings::findSetting("view3d.face");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && settings::tokenOf(*row, parseConfig("")) == "1");
    CHECK(appliesNow("view3d.face"));
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[view3d]\nface = 0\n")).next.view3dFace);
}

// 3.0.19-beta.41 (item 7 of the owner's «делаем все по порядку»: the free camera flew through the houses): its step
// kept kFreeGap off the models' boxes (nearer, the game's near plane cuts the wall open), axis by axis - along a wall
// it slides; out of a box it is in, and away from one, it goes
TEST(view3d_free_camera_kept_off_the_models) {
    view3d::Box house;
    house.centre[0] = 30.0f;
    house.half[0] = house.half[1] = house.half[2] = 5.0f;  // its face at x 25
    const std::vector<view3d::Box> boxes = {house};
    const float from[3] = {0, 0, 0};
    float to[3] = {20.0f, 0, 0};
    view3d::keepOffBoxes(from, to, boxes);
    CHECK(std::fabs(to[0] - (25.0f - view3d::kFreeGap)) < 0.01f && to[1] == 0.0f && to[2] == 0.0f);
    const float at[3] = {15.0f, 0, 0};
    float on[3] = {16.0f, 0, 3.0f};  // into it and aside: aside alone
    view3d::keepOffBoxes(at, on, boxes);
    CHECK(std::fabs(on[0] - 15.0f) < 0.01f && on[2] == 3.0f);
    float back[3] = {10.0f, 0, 0};  // away from it
    view3d::keepOffBoxes(at, back, boxes);
    CHECK(back[0] == 10.0f);
    const float inside[3] = {30.0f, 0, 0};
    float out[3] = {40.0f, 0, 0};  // out of the box it is in
    view3d::keepOffBoxes(inside, out, boxes);
    CHECK(out[0] == 40.0f);
    float free[3] = {20.0f, 0, 0};
    view3d::keepOffBoxes(from, free, {});
    CHECK(free[0] == 20.0f);
    float over[3] = {20.0f, -30.0f, 0};  // over its roof: up first, then on
    view3d::keepOffBoxes(from, over, boxes);
    CHECK(over[1] == -30.0f && std::fabs(over[0] - 15.0f) < 0.01f);  // x before y: stopped first, then up
}

// 3.0.19-beta.43 (R-RRGQ5S: in pay_dun00 the HP bars still showed through the cave's walls - the line of sight read
// the walk map's heights, a wall's cell there at the floor): the drawn ground's height from a cell's top face (its
// four corners, the game's triangles 0 1 2 and 1 3 2) and the cell at a place
TEST(view3d_drawn_ground_height_from_a_cell_face) {
    const float flat[4][3] = {{0, -5, 0}, {10, -5, 0}, {0, -5, 10}, {10, -5, 10}};
    float y = 0;
    CHECK(view3d::heightOnFace(flat, 3.0f, 7.0f, y) && std::fabs(y + 5.0f) < 1e-4f);
    const float slope[4][3] = {{0, 0, 0}, {10, -5, 0}, {0, 0, 10}, {10, -5, 10}};  // rising along x (y down)
    CHECK(view3d::heightOnFace(slope, 4.0f, 3.0f, y) && std::fabs(y + 2.0f) < 1e-4f);
    CHECK(view3d::heightOnFace(slope, 8.0f, 9.0f, y) && std::fabs(y + 4.0f) < 1e-4f);  // the other triangle
    CHECK(!view3d::heightOnFace(slope, 12.0f, 3.0f, y));                               // past its side
    int col = -1, row = -1;
    CHECK(view3d::cellOf(4, 6, 10.0f, -15.0f, 25.0f, false, col, row) && col == 0 && row == 5);
    CHECK(view3d::cellOf(4, 6, 10.0f, -15.0f, 25.0f, true, col, row) && col == 0 && row == 0);
    CHECK(view3d::cellOf(4, 6, 10.0f, 19.9f, -30.0f, false, col, row) && col == 3 && row == 0);
    CHECK(!view3d::cellOf(4, 6, 10.0f, 20.0f, 0.0f, false, col, row));  // off the map
    CHECK(!view3d::cellOf(4, 6, 0.0f, 0.0f, 0.0f, false, col, row));    // no ground
}

// 3.0.19-beta.45 (after R-RRGQ5S: the cameras read the walk map's heights too - a cave's walls at the floor there): the
// free camera's step kept out of the ground rising over the eye (a cave's rock wall) axis by axis - along it it
// slides; a low rise (a step, a slope) it goes up as before (overGround)
TEST(view3d_free_camera_kept_out_of_rock) {
    const auto cave = [](float x, float) { return x > 10.0f ? -33.0f : 0.0f; };  // y down: a wall's top 33 up past x 10
    const float from[3] = {5.0f, -20.0f, 0.0f};
    float to[3] = {12.0f, -20.0f, 3.0f};  // into the wall and aside: aside alone
    view3d::keepOverGround(from, to, cave);
    CHECK(to[0] == 5.0f && to[2] == 3.0f && to[1] == -20.0f);
    float back[3] = {2.0f, -20.0f, 0.0f};  // away from it
    view3d::keepOverGround(from, back, cave);
    CHECK(back[0] == 2.0f);
    const auto step = [](float x, float) { return x > 10.0f ? -21.0f : 0.0f; };  // 1 over the eye: within kFreeClimb
    float up[3] = {12.0f, -20.0f, 0.0f};
    view3d::keepOverGround(from, up, step);
    CHECK(up[0] == 12.0f);
    const auto unknown = [](float, float) { return std::numeric_limits<float>::quiet_NaN(); };
    float any[3] = {30.0f, -20.0f, 0.0f};
    view3d::keepOverGround(from, any, unknown);
    CHECK(any[0] == 30.0f);  // no ground known: free
    float high[3] = {12.0f, -40.0f, 0.0f};  // over the wall's top: free
    view3d::keepOverGround(from, high, cave);
    CHECK(high[0] == 12.0f && high[1] == -40.0f);
}

// 3.0.19-beta.54 (the owner: «может персонажа по центру поставить в 3д?»): behind, the eye straight back from the head
// and kCentreRise over it - the character in the middle, the crosshair over its head; [view3d] shoulder the old way
TEST(view3d_behind_centred_the_eye_straight_back_and_over_the_head) {
    const float feet[3] = {0.0f, 0.0f, 0.0f}, ahead[3] = {1.0f, 0.0f, 0.0f}, up[3] = {0.0f, -1.0f, 0.0f};
    float eye[3], target[3];
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, eye, target, 45.0f, false);
    CHECK(near(eye[0], -45.0f) && near(eye[2], 0.0f));
    CHECK(near(eye[1], -(view3d::kHeadHeight + view3d::kCentreRise)));  // y down
    CHECK(target[0] > eye[0] && near(target[1], eye[1]) && near(target[2], 0.0f));
    view3d::place(view3d::Mode::Behind, feet, ahead, up, 0.0f, eye, target, 45.0f, true);
    CHECK(near(std::fabs(eye[2]), view3d::kShoulder) && near(eye[1], -view3d::kHeadHeight));
}

// 3.0.19-beta.54 (the owner: «и чтобы с шифтом можно было крутить?»): Shift held - the camera round the character, its
// facing waits; let go - the look back where it was before Shift, along the shorter way round, over kOrbitBackMs
TEST(view3d_orbit_with_shift_turns_round_then_back) {
    view3d::Orbit o;
    view3d::Look look{10.0f, -5.0f};
    CHECK(!view3d::orbitStep(o, look, false, 0));  // no Shift: nothing waits
    CHECK(view3d::orbitStep(o, look, true, 10));
    look.yaw = 200.0f;  // the mouse turned the camera round
    look.pitch = 20.0f;
    CHECK(view3d::orbitStep(o, look, true, 50) && near(look.yaw, 200.0f));  // held: the mouse's own
    CHECK(view3d::orbitStep(o, look, false, 100));  // let go: on its way back
    CHECK(view3d::orbitStep(o, look, false, 100 + view3d::kOrbitBackMs / 2));
    CHECK(near(look.yaw, 285.0f) && near(look.pitch, 7.5f));  // halfway: 200 -> 370 (= 10), the shorter way
    CHECK(!view3d::orbitStep(o, look, false, 100 + view3d::kOrbitBackMs));
    CHECK(near(look.yaw, 10.0f) && near(look.pitch, -5.0f));
    CHECK(!view3d::orbitStep(o, look, false, 2000));  // done: the mouse's again
}

TEST(view3d_orbit_shift_again_on_the_way_back_keeps_where_it_was) {
    view3d::Orbit o;
    view3d::Look look{90.0f, 0.0f};
    view3d::orbitStep(o, look, true, 0);
    look.yaw = 180.0f;
    view3d::orbitStep(o, look, false, 100);
    view3d::orbitStep(o, look, false, 100 + view3d::kOrbitBackMs / 2);  // halfway back
    CHECK(view3d::orbitStep(o, look, true, 200));                       // Shift again: round from here
    look.yaw = 300.0f;
    view3d::orbitStep(o, look, false, 300);
    CHECK(!view3d::orbitStep(o, look, false, 300 + view3d::kOrbitBackMs));
    CHECK(near(look.yaw, 90.0f));  // back where it was before the first Shift
}

// 3.0.19-beta.54: [view3d] shoulder - off unless the file says 1; shown on «Гра», applied at once
TEST(config_view3d_shoulder_off_by_default) {
    CHECK(!parseConfig("").view3dShoulder);
    CHECK(parseConfig("[view3d]\nshoulder = 1\n").view3dShoulder);
    const settings::IniSetting* row = settings::findSetting("view3d.shoulder");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && settings::tokenOf(*row, parseConfig("")) == "0");
    CHECK(appliesNow("view3d.shoulder"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[view3d]\nshoulder = 1\n")).next.view3dShoulder);
}

// 3.0.20-beta.4 (stage 2b of «Модели нашим рендером»): [view3d] own_models - on «Гра», applied at once; 3.0.20-beta.15
// on unless the file says 0 (the owner's «делай все», stage 5; config_models_by_our_renderer_on_by_default)
TEST(config_view3d_own_models_switch) {
    CHECK(!parseConfig("[view3d]\nown_models = 0\n").view3dOwnModels);
    CHECK(parseConfig("[view3d]\nown_models = 1\n").view3dOwnModels);
    const settings::IniSetting* row = settings::findSetting("view3d.own_models");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && settings::tokenOf(*row, parseConfig("")) == "1");
    CHECK(appliesNow("view3d.own_models"));
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[view3d]\nown_models = 0\n")).next.view3dOwnModels);
}

// 3.0.20-beta.49 (the owner's «делай 1-9», 3 - the 3D view's night sky and dawn): the sky's parts - stars and the
// moon at night, a warm glow along the horizon at dawn and dusk - each a switch of [view3d], on at first; the
// renderers give sky3d_ps and the water's reflection the parts as one number
TEST(config_view3d_sky_parts_on_by_default_and_live) {
    const EffectSettings e = parseConfig("").effects;
    CHECK(e.skyStars && e.skyMoon && e.skyDawn);
    CHECK_EQ(view3d::skyParts(true, true, true), 7);
    CHECK_EQ(view3d::skyParts(false, true, true), 6);
    CHECK_EQ(view3d::skyParts(true, false, false), 1);
    CHECK_EQ(view3d::skyParts(false, false, false), 0);
    const EffectSettings off = parseConfig("[view3d]\nstars = 0\nmoon = 0\ndawn = 0\n").effects;
    CHECK(!off.skyStars && !off.skyMoon && !off.skyDawn);
    for (const char* key : {"view3d.stars", "view3d.moon", "view3d.dawn"}) {
        const settings::IniSetting* row = settings::findSetting(key);
        CHECK(row != nullptr);
        if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
        CHECK(appliesNow(key));
    }
    CHECK(!(parseConfig("").effects == parseConfig("[view3d]\nmoon = 0\n").effects));
}

// 3.0.20-beta.72 (the owner's check list, 5: «горизонт есть, но сделать бы его красивее», then «да, нужны
// подробности чтобы было красивее»): far ranges along the horizon in the 3D view's sky (shaders.cpp skyAlong) -
// which kind a map takes, and how the kind goes to the shaders with the sky's other parts
TEST(view3d_far_ranges_by_the_map) {
    using view3d::Ranges;
    // (wanted, outdoors, hung in the sky, the sea past its edges, sand, snow)
    CHECK(view3d::rangesFor(true, true, false, false, false, false) == Ranges::Mountains);
    CHECK(view3d::rangesFor(true, true, false, false, true, false) == Ranges::Dunes);
    CHECK(view3d::rangesFor(true, true, false, false, false, true) == Ranges::Snow);
    CHECK(view3d::rangesFor(true, true, false, true, false, false) == Ranges::Low);  // islands past the water
    CHECK(view3d::rangesFor(true, true, false, true, true, false) == Ranges::Low);   // a desert's coast: the sea's
    // none: switched off, under a roof, a map its makers hung in the sky (no land round it)
    CHECK(view3d::rangesFor(false, true, false, false, false, false) == Ranges::None);
    CHECK(view3d::rangesFor(true, false, false, false, false, false) == Ranges::None);
    CHECK(view3d::rangesFor(true, true, true, false, false, false) == Ranges::None);
    // the sky's parts: 16 with ranges, their kind in the next two bits; the old three as they were
    CHECK_EQ(view3d::skyParts(true, true, true, Ranges::None), 7);
    CHECK_EQ(view3d::skyParts(false, false, false, Ranges::Mountains), 16);
    CHECK_EQ(view3d::skyParts(true, false, true, Ranges::Dunes), 5 + 16 + 32);
    CHECK_EQ(view3d::skyParts(false, false, false, Ranges::Snow), 16 + 64);
    CHECK_EQ(view3d::skyParts(false, false, false, Ranges::Low), 16 + 96);
    // the maps hung in the sky by their names (where the ground's file is not read to tell)
    CHECK(view3d::hungMap("yuno"));
    CHECK(view3d::hungMap("gonryun"));
    CHECK(!view3d::hungMap("prontera"));
    CHECK(!view3d::hungMap("yuno_fild01"));  // Yuno's fields lie on the ground
    CHECK(!view3d::hungMap(""));
}
