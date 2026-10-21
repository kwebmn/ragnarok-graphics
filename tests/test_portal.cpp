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

TEST(portal_new_on_by_default_and_off_in_the_effects_off_set) {
    // 2.7.48: «Новий портал» - a part of «Портали», on as the user asked; Pause and the set «off» give the game's own
    CHECK(parseConfig("").effects.portalNew);
    CHECK(!parseConfig("[effects]\nportal_new = 0\n").effects.portalNew);
    CHECK(!effectsOff().portalNew);
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
