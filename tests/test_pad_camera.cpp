#include <cmath>

#include "pad/core/logic/camera.hpp"
#include "test.hpp"

using namespace uapad;

namespace {

bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
double deg(double r) { return r * 180.0 / 3.14159265358979323846; }

}  // namespace

TEST(camera_default_is_north_up) {
    CameraYaw c;
    CHECK_EQ(c.samples(), 0u);
    CHECK(near(c.radians(), 0));
    double wx = 0, wy = 0;
    c.toWorld(0, -1, wx, wy);  // screen up
    CHECK(near(wx, 0) && near(wy, 1));  // north
    c.toWorld(3, 0, wx, wy);  // screen right, any length
    CHECK(near(wx, 1) && near(wy, 0));  // east, unit length
}

TEST(camera_learns_rotation) {
    CameraYaw c;
    c.addSample(132, 0, 0, 7);  // the cursor was right of the centre and the client walked north
    CHECK_EQ(c.samples(), 1u);
    CHECK(near(deg(c.radians()), 90));
    double wx = 0, wy = 0;
    c.toWorld(1, 0, wx, wy);  // screen right -> north
    CHECK(near(wx, 0) && near(wy, 1));
    c.toWorld(0, -1, wx, wy);  // screen up -> west
    CHECK(near(wx, -1) && near(wy, 0));
    CameraYaw d;
    d.addSample(93, -93, 5, 5);  // up-right on screen, north-east on the map: not turned
    CHECK(near(deg(d.radians()), 0));
}

TEST(camera_ignores_tiny_samples) {
    CameraYaw c;
    c.addSample(132, 0, 1, 1);  // the destination is too close to tell a direction
    c.addSample(5, 3, 0, 7);    // the cursor sat almost on the centre
    CHECK_EQ(c.samples(), 0u);
    CHECK(near(c.radians(), 0));
}

TEST(camera_trims_outlier) {
    CameraYaw c;
    for (int i = 0; i < 7; ++i) c.addSample(0, -132, 0, 7);  // up -> north
    c.addSample(132, 0, 0, 7);  // a stale cursor: looks like a 90 degree turn
    CHECK(near(deg(c.radians()), 0, 1e-6));
}

TEST(camera_follows_a_turn) {
    CameraYaw c;
    for (int i = 0; i < 8; ++i) c.addSample(0, -132, 0, 7);
    for (int i = 0; i < 6; ++i) c.addSample(0, -132, -7, 0);  // the camera turned: up is west now
    CHECK_EQ(c.samples(), 8u);
    CHECK(near(deg(c.radians()), 90, 1e-6));  // 6 new against 2 old samples
}

TEST(camera_reset_forgets_the_turn) {
    CameraYaw c;
    c.addSample(132, 0, 0, 7);
    c.reset();
    CHECK_EQ(c.samples(), 0u);
    CHECK(near(c.radians(), 0));
}

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180.0;

// A walk sample of a camera turned so that screen right is map bearing `yawDeg` (counter-clockwise from east).
void sampleAt(CameraYaw& c, double yawDeg) {
    c.addSample(100, 0, int(std::lround(8 * std::cos(yawDeg * kDeg))), int(std::lround(8 * std::sin(yawDeg * kDeg))));
}

bool nearDeg(double a, double b, double tolDeg) {
    return std::fabs(std::remainder(a - b, 2 * 3.14159265358979323846)) < tolDeg * kDeg;
}

}  // namespace

TEST(camera_follows_the_clients_turn_once_it_knows_the_sign) {
    CameraYaw c;
    for (double client : {0.0, 50.0, 100.0}) {  // the client's turn grows while the screen turns the other way
        c.setClientTurn(client * kDeg);
        sampleAt(c, -client);
        sampleAt(c, -client);
    }
    CHECK(c.calibrated());
    c.setClientTurn(30 * kDeg);  // turned again: no walking needed to follow it
    CHECK(nearDeg(c.radians(), -30 * kDeg, 6));
    double wx = 0, wy = 0;
    c.toWorld(1, 0, wx, wy);  // screen right
    CHECK(nearDeg(std::atan2(wy, wx), -30 * kDeg, 6));
}

TEST(camera_learns_a_plus_sign_and_an_offset) {
    CameraYaw c;
    for (double client : {10.0, 70.0, 140.0}) {
        c.setClientTurn(client * kDeg);
        sampleAt(c, client + 90);
        sampleAt(c, client + 90);
    }
    CHECK(c.calibrated());
    c.setClientTurn(200 * kDeg);
    CHECK(nearDeg(c.radians(), 290 * kDeg, 6));
}

TEST(camera_needs_turns_far_apart_to_calibrate) {
    CameraYaw c;
    c.setClientTurn(10 * kDeg);
    for (int i = 0; i < 6; ++i) sampleAt(c, 40);
    CHECK(!c.calibrated());
    CHECK(nearDeg(c.radians(), 40 * kDeg, 6));  // what the walk samples say, as before
    c.clearClientTurn();
    CHECK(nearDeg(c.radians(), 40 * kDeg, 6));
}
