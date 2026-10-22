#include <cmath>

#include "core/config.hpp"
#include "core/game_camera.hpp"
#include "core/portal.hpp"
#include "core/report_upload.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(portal_new_on_by_default_the_games_own_draws_shaded_anew) {
    // 2.7.48: «Новий портал» - a part of «Живі портали». 2.7.53: off while the game's portal was recorded. 2.7.54: on
    // again - the game's own draws of the warp shaded anew where it draws them (nothing of the driver's own to place:
    // the five builds' two portals came of placing one); Pause and the set «off» give the game's own
    CHECK(parseConfig("").effects.portalNew);
    CHECK(!parseConfig("[effects]\nportal_new = 0\n").effects.portalNew);
    CHECK(!effectsOff().portalNew);
}

TEST(portal_warp_parts_known_by_their_draws) {
    // 2.7.54, the driver's record of the map warp (effect 321, R-FP2F4H): its rings added (SRCALPHA, ONE) in
    // triangles, its floor blended over (SRCALPHA, INVSRCALPHA) in triangles, its sparks blended over in quads
    CHECK(portal::partOf(321, true, 5, 2, 3) == portal::Part::Ring);
    CHECK(portal::partOf(321, true, 5, 6, 3) == portal::Part::Floor);
    CHECK(portal::partOf(321, true, 5, 6, 4) == portal::Part::Spark);
    // anything else as the game draws it: another effect taken for a portal, a state the record has not shown
    CHECK(portal::partOf(61, true, 5, 2, 3) == portal::Part::None);
    CHECK(portal::partOf(36, true, 5, 6, 4) == portal::Part::None);
    CHECK(portal::partOf(321, false, 5, 6, 3) == portal::Part::None);
    CHECK(portal::partOf(321, true, 2, 2, 3) == portal::Part::None);
    CHECK(portal::partOf(321, true, 5, 2, 4) == portal::Part::None);
    CHECK(portal::partOf(321, true, 5, 6, 6) == portal::Part::None);
}

TEST(portal_warp_rings_and_floor_shaded_anew_sparks_the_games) {
    // 2.7.55 (the tester on 2.7.54: «аккуратнее ... более похожим на тот что в оригинале»): the rings and the floor
    // from the game's own pictures, cleaned; the sparks the game's own
    CHECK(portal::reshaded(portal::Part::Ring));
    CHECK(portal::reshaded(portal::Part::Floor));
    CHECK(!portal::reshaded(portal::Part::Spark));
    CHECK(!portal::reshaded(portal::Part::None));
}

TEST(portal_warp_picture_told_in_lines_the_tools_read) {
    // 2.7.55: the warp's pictures into the log once a run - their alpha is in no copy on the server; the lines as
    // 2.7.53's record wrote them ($S/portal/capture_tool.py reads them): a head, then base64 of ARGB, 160 a line
    std::vector<uint32_t> argb(64 * 64);
    for (size_t k = 0; k < argb.size(); ++k) argb[k] = uint32_t(k * 2654435761u);
    const std::vector<std::string> lines = portal::pictureLines(1073742059u, 64, 64, argb.data());
    CHECK(!lines.empty() && lines[0] == "portal capture: texture 1073742059 64x64 (ARGB, base64)");
    std::string text;
    for (size_t k = 1; k < lines.size(); ++k) {
        CHECK(lines[k].rfind("  t ", 0) == 0 && lines[k].size() <= 164);
        text += lines[k].substr(4);
    }
    CHECK_EQ(text.size(), size_t((64 * 64 * 4 + 2) / 3 * 4));
    CHECK(text == report::base64(std::string(reinterpret_cast<const char*>(argb.data()), argb.size() * 4)));
}

TEST(portal_warp_floor_middle_is_the_corner_all_its_triangles_share) {
    // the floor: 20 triangles round its middle, v 1 there and 0 on its rim (half of them have the middle twice)
    const portal::Corner rim[3] = {{1062.8f, 307.2f, 0.002251f, 0.0f}, {1147.7f, 317.1f, 0.002256f, 0.0f},
                                   {1086.7f, 407.6f, 0.002301f, 1.0f}};
    portal::Corner m;
    CHECK(portal::floorMiddle(rim, m));
    CHECK(m.x == 1086.7f && m.y == 407.6f && m.rhw == 0.002301f);
    const portal::Corner twice[3] = {{1086.7f, 407.6f, 0.002301f, 1.0f}, {1086.7f, 407.6f, 0.002301f, 1.0f},
                                     {1210.5f, 361.3f, 0.002278f, 0.0f}};
    CHECK(portal::floorMiddle(twice, m));
    CHECK(m.x == 1086.7f && m.y == 407.6f);
    const portal::Corner none[3] = {
        {0.0f, 0.0f, 0.002f, 0.0f}, {9.0f, 0.0f, 0.002f, 0.25f}, {0.0f, 9.0f, 0.002f, 0.5f}};
    CHECK(!portal::floorMiddle(none, m));
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
