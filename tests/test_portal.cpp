#include <cmath>

#include "core/config.hpp"
#include "core/game_camera.hpp"
#include "core/portal.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(portal_rings_close_in_from_14_to_2_over_their_life) {
    // 2.7.48 (the user's pick «Б» for the new portal): the game's warp zone (uaRO.exe FUN_005ce290) - a ring every 28
    // ticks of 60 a second, each 80 ticks long, standing on a circle of 14 that closes in by 0.15 a tick
    const auto born = portal::ringsAt(0.0);
    CHECK(std::fabs(born[0].radius - 14.0f) < 1e-3f);
    CHECK(born[0].alpha == 0.0f);  // it comes in
    const double life = 80.0 / 60.0;
    const auto old = portal::ringsAt(life * 0.999);
    float smallest = 99.0f;
    for (const portal::Ring& r : old) smallest = std::min(smallest, r.radius);
    CHECK(smallest > 1.9f && smallest < 2.2f);
}

TEST(portal_rings_two_or_three_shown_at_any_moment) {
    for (int k = 0; k < 600; ++k) {
        const double t = 10.0 + k * 0.01;
        int shown = 0;
        for (const portal::Ring& r : portal::ringsAt(t)) {
            if (r.alpha > 0.0f) CHECK(r.radius >= 2.0f - 1e-3f && r.radius <= 14.0f + 1e-3f);  // a gone one: 0
            CHECK(r.alpha >= 0.0f && r.alpha <= 0.8f);
            shown += r.alpha > 0.05f ? 1 : 0;
        }
        CHECK(shown >= 2 && shown <= 3);
    }
}

TEST(portal_rings_fade_in_and_out) {
    // a ring's alpha rises over its first eighth and falls over its last fifth (FUN_005ce290: 0x198 - 0x198 / 5)
    const double tick = 1.0 / 60.0;
    const auto mid = portal::ringsAt(40.0 * tick);
    CHECK(std::fabs(mid[0].alpha - 200.0f / 255.0f) < 1e-3f);
    const auto late = portal::ringsAt(79.0 * tick);
    CHECK(late[2].alpha > 0.0f && late[2].alpha < 0.1f);  // the oldest, born at 0
}

TEST(portal_new_off_by_default_while_the_games_is_recorded) {
    // 2.7.48: «Новий портал» - a part of «Портали». 2.7.53 (five builds and still two portals on the screen): off by
    // default - the game's own portal while its draws are recorded (core/portal_capture.hpp) and the new one is made
    // anew on the facts; one who wants it still ticks it. Pause and the set «off» give the game's own
    CHECK(!parseConfig("").effects.portalNew);
    CHECK(parseConfig("[effects]\nportal_new = 1\n").effects.portalNew);
    CHECK(!effectsOff().portalNew);
}

TEST(portal_game_portal_hidden_only_while_ours_was_drawn) {
    // 2.7.49 (the tester on 2.7.48: «порталы не нарисовались» - the game's hidden, ours never drawn): the game's
    // portal is left out only while the new one was drawn the frame before - never no portal at all
    CHECK(portal::hidesGame(true, true));
    CHECK(!portal::hidesGame(true, false));
    CHECK(!portal::hidesGame(false, true));
}

namespace {
portal::Seen seenAt(float gx, float gy, float z = 300.0f) {
    return portal::Seen{{gx * 0.1f, gy * 0.1f, z}, gx, gy, 90.0f, gx - 40.0f, gx + 40.0f, gy + 25.0f};
}
}  // namespace

TEST(portal_seen_once_is_followed) {
    // 2.7.50 (the tester on 2.7.49: still the game's portal - ours never drawn): 0.39.1 started a track at strength 0
    // and dropped every track at 0 or under in the same frame - no portal was ever followed (nor its 0.38 blue glow)
    std::vector<portal::Track> tracks;
    portal::follow(tracks, {seenAt(500.0f, 300.0f)}, 1.0f / 60.0f);
    CHECK(tracks.size() == 1);
    CHECK(!tracks.empty() && tracks[0].strength > 0.0f);
    std::vector<portal::Track> first;
    portal::follow(first, {seenAt(500.0f, 300.0f)}, 0.0f);  // the very first frame: no time gone yet
    CHECK(first.size() == 1);
}

TEST(portal_followed_comes_in_and_goes_out) {
    std::vector<portal::Track> tracks;
    for (int f = 0; f < 30; ++f) portal::follow(tracks, {seenAt(500.0f, 300.0f)}, 1.0f / 60.0f);  // half a second
    CHECK(tracks.size() == 1 && tracks[0].strength == 1.0f);
    for (int f = 0; f < 60; ++f) portal::follow(tracks, {}, 1.0f / 60.0f);  // a second unseen: fading
    CHECK(tracks.size() == 1 && tracks[0].strength > 0.0f && tracks[0].strength < 0.3f);
    for (int f = 0; f < 30; ++f) portal::follow(tracks, {}, 1.0f / 60.0f);
    CHECK(tracks.empty());
}

TEST(portal_draws_near_each_other_one_portal) {
    // its rings, its column and its sparks are many draws: within 80 game pixels (or 0.8 of their size) one portal
    std::vector<portal::Track> tracks;
    portal::follow(tracks, {seenAt(500.0f, 300.0f), seenAt(530.0f, 280.0f), seenAt(900.0f, 300.0f)}, 1.0f / 60.0f);
    CHECK(tracks.size() == 2);
    float near = 0.0f;
    for (const portal::Track& t : tracks)
        if (t.gx < 700.0f) near = t.gx;
    CHECK(std::fabs(near - 515.0f) < 0.01f);  // their middle
}

TEST(portal_stands_where_its_draws_span_on_the_screen) {
    // 2.7.51 (the tester on 2.7.50: «два портала, один портал двигается»; the log: its middle 299 deep, off
    // the screen): the game draws the warp NPC's portal (321) flat over the screen - its depth is not the
    // world's; where it stands is the middle of its draws' span, up from their bottom by half the span
    // squashed as a circle on the ground
    std::vector<portal::Track> tracks;
    portal::Seen a = seenAt(490.0f, 300.0f), b = seenAt(510.0f, 310.0f);
    a.left = 460.0f, a.right = 520.0f, a.bottom = 320.0f;
    b.left = 480.0f, b.right = 540.0f, b.bottom = 340.0f;
    portal::follow(tracks, {a, b}, 1.0f / 60.0f);
    CHECK(tracks.size() == 1);
    if (tracks.empty()) return;
    CHECK(tracks[0].left == 460.0f && tracks[0].right == 540.0f && tracks[0].bottom == 340.0f);
    float at[2];
    portal::anchorOf(tracks[0], 0.7f, at);
    CHECK(std::fabs(at[0] - 500.0f) < 1e-3f);
    CHECK(std::fabs(at[1] - (340.0f - 40.0f * 0.7f)) < 1e-3f);
}

TEST(portal_draws_too_big_to_be_one_left_out) {
    // a draw thousands of pixels across (its corners behind the camera) is no part of a portal: it would gather all
    std::vector<portal::Track> tracks;
    portal::Seen huge = seenAt(500.0f, 300.0f);
    huge.size = 2500.0f;
    huge.left = -900.0f;
    huge.right = 1600.0f;
    portal::follow(tracks, {huge}, 1.0f / 60.0f);
    CHECK(tracks.empty());
}

TEST(portal_screen_box_round_its_column) {
    // 2.7.48: the pass runs over the screen box round the portal's column (kColumnRadius, the height given) only
    const camera::Projection p = camera::projectionFor(1920, 1080);
    const float foot[3] = {0.0f, 0.0f, 100.0f}, up[3] = {0.0f, -0.7071f, -0.7071f};  // the camera 45 degrees down
    float box[4];
    CHECK(portal::screenBox(foot, up, p, 45.0f, box));
    CHECK(std::fabs((box[0] + box[2]) * 0.5f - p.cx) < 1.0f);  // straight ahead: as wide either way
    const float half = portal::kColumnRadius * p.f0 / 100.0f;
    CHECK(box[2] - box[0] > 2.0f * half && box[2] - box[0] < 4.5f * half);  // its top nearer the camera: wider
    CHECK(box[1] < p.cy - 45.0f * p.f1 / 100.0f * 0.9f);  // its top over it
    CHECK(box[3] > p.cy);                                 // its foot's circle below the middle
    const float behind[3] = {0.0f, 0.0f, -10.0f};
    CHECK(!portal::screenBox(behind, up, p, 45.0f, box));
}

TEST(portal_foot_where_its_draws_are_at_their_depth) {
    // 2.7.52 (the tester on 2.7.51: one portal on a tower's roof - the ground looked for under it on the screen was
    // the tower in front of it): its foot is where it stands on the screen at its draws' own depth (the game's
    // vertices of the warp NPC's portal lie on the ground: a fan 465-474 deep round its middle)
    const camera::Projection p = camera::projectionFor(1920, 1080);
    std::vector<portal::Track> tracks;
    portal::Seen s = seenAt(565.0f, 559.0f, 470.0f);
    s.left = 434.0f, s.right = 697.0f, s.bottom = 655.0f;
    portal::follow(tracks, {s}, 1.0f / 60.0f);
    CHECK(tracks.size() == 1);
    if (tracks.empty()) return;
    float foot[3], at[2];
    portal::footOf(tracks[0], 0.72f, p, foot);
    portal::anchorOf(tracks[0], 0.72f, at);
    CHECK(std::fabs(foot[2] - 470.0f) < 1e-3f);
    CHECK(std::fabs(foot[0] - (at[0] - p.cx) / p.f0 * 470.0f) < 1e-3f);
    CHECK(std::fabs(foot[1] - (at[1] - p.cy) / p.f1 * 470.0f) < 1e-3f);
}
