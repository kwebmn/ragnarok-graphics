#include <cmath>
#include <vector>

#include "core/game_camera.hpp"
#include "core/trees.hpp"
#include "test.hpp"

using namespace uagfx;
using trees::Season;

TEST(trees_seasons_by_the_map_and_the_month) {
    // 1.7: rime in Lutie all year, autumn in September to November where the trees turn
    CHECK(trees::seasonOf("xmas_fild01", 6) == Season::Rime);
    CHECK(trees::seasonOf("prt_fild08", 10) == Season::Autumn);
    CHECK(trees::seasonOf("prt_fild08", 8) == Season::Green);
    CHECK(trees::seasonOf("prt_fild08", 12) == Season::Green);
    CHECK(trees::seasonOf("moc_fild12", 10) == Season::Green);  // the desert's palms do not turn
    CHECK(trees::evergreen("um_fild01") && trees::evergreen("niflheim") && trees::evergreen("gl_church"));
    CHECK(!trees::evergreen("pay_fild01") && !trees::evergreen("prontera"));
    CHECK(trees::monthNow() >= 1 && trees::monthNow() <= 12);
}

TEST(trees_shade_is_smooth_and_each_tree_its_own) {
    const trees::Shade a = trees::shadeAt(100.0f, 200.0f, Season::Green);
    const trees::Shade near = trees::shadeAt(100.5f, 200.0f, Season::Green);  // the same crown: nearly the same
    CHECK(std::fabs(a.tint[0] - near.tint[0]) < 0.02f && std::fabs(a.tint[2] - near.tint[2]) < 0.02f);
    bool differ = false;  // trees apart differ
    for (int i = 1; i < 20 && !differ; ++i) {
        const trees::Shade far = trees::shadeAt(100.0f + 37.0f * float(i), 200.0f, Season::Green);
        differ = std::fabs(far.tint[1] - a.tint[1]) > 0.01f;
    }
    CHECK(differ);
    int turned = 0;
    for (int i = 0; i < 400; ++i) {
        const trees::Shade s = trees::shadeAt(13.0f * float(i), 7.0f * float(i % 37), Season::Autumn);
        for (const float t : s.tint) CHECK(t > 0.8f && t < 1.25f);  // a shade, not another colour
        CHECK(s.autumnAmount >= 0.0f && s.autumnAmount <= 0.85f + 1e-6f && s.rime == 0.0f);
        turned += s.autumnAmount > 0.0f ? 1 : 0;
    }
    CHECK(turned > 0 && turned < 400);  // some trees turned, not all
    CHECK(a.autumnAmount == 0.0f && a.rime == 0.0f);  // green: none of it
    CHECK(trees::shadeAt(0.0f, 0.0f, Season::Rime).rime >= 0.45f);
}

TEST(trees_light_through_the_leaves_low_sun_most_none_at_night) {
    CHECK(std::fabs(trees::throughLight(0.55f, 0.0f) - 0.45f) < 1e-5f);  // noon
    CHECK(std::fabs(trees::throughLight(1.6f, 0.0f) - 1.0f) < 1e-5f);    // sunset
    CHECK(trees::throughLight(1.0f, 0.0f) > trees::throughLight(0.6f, 0.0f));
    CHECK(trees::throughLight(1.6f, 1.0f) == 0.0f);
}

TEST(trees_a_crown_s_place_does_not_turn_with_the_camera) {
    // 3.0.4-beta.18 (the user: «когда кручу камеру деревья меняют текст[уру]»): the place a crown's shade is looked up
    // at was its leaves' box on the screen, its middle at their mean depth - it went round the crown as the camera
    // turned, up to 6 units, and the shade with it (0.87-1.01 on prt_fild08). Now their mean place in the world: the
    // same from every side.
    const float leaves[][3] = {{550, -40, 890}, {562, -55, 904}, {541, -48, 911}, {556, -70, 897}, {547, -62, 905}};
    const camera::Projection p = camera::projectionFor(1920, 1080);
    float first[2] = {0, 0};
    for (int yawDeg = 0; yawDeg < 360; yawDeg += 60) {
        const float yaw = float(yawDeg) * 3.14159265f / 180.0f, tilt = 0.785398f, dist = 600.0f;
        const float at[3] = {550, 0, 900};
        const float eye[3] = {at[0] - std::sin(yaw) * std::cos(tilt) * dist, at[1] - std::sin(tilt) * dist,
                              at[2] - std::cos(yaw) * std::cos(tilt) * dist};
        float z[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
        float l = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        for (float& c : z) c /= l;
        float x[3] = {-z[2], 0.0f, z[0]};  // (0, -1, 0) x z: level, the screen's right
        l = std::sqrt(x[0] * x[0] + x[2] * x[2]);
        x[0] /= l, x[2] /= l;
        const float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
        const float m[12] = {x[0], x[1], x[2], y[0], y[1], y[2], z[0], z[1], z[2], eye[0], eye[1], eye[2]};
        // the leaves as the game hands them over: on the screen, x y z rhw
        std::vector<float> screen;
        for (const auto& w : leaves) {
            const float d[3] = {w[0] - eye[0], w[1] - eye[1], w[2] - eye[2]};
            const float vx = d[0] * x[0] + d[1] * x[1] + d[2] * x[2], vy = d[0] * y[0] + d[1] * y[1] + d[2] * y[2];
            const float vz = d[0] * z[0] + d[1] * z[1] + d[2] * z[2];
            screen.insert(screen.end(), {vx / vz * p.f0 + p.cx, vy / vz * p.f1 + p.cy, 0.5f, 1.0f / vz});
        }
        float wx = 0, wz = 0;
        CHECK(trees::crownPlace(reinterpret_cast<const uint8_t*>(screen.data()), 4 * sizeof(float),
                                uint32_t(screen.size() / 4), m, p, wx, wz));
        if (yawDeg == 0) first[0] = wx, first[1] = wz;
        CHECK(std::fabs(wx - first[0]) < 0.05f && std::fabs(wz - first[1]) < 0.05f);
        CHECK(std::fabs(wx - 551.2f) < 0.05f && std::fabs(wz - 901.4f) < 0.05f);  // their mean
    }
}
