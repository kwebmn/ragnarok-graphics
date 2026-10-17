#include <cmath>

#include "core/wind.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(wind_turns_slowly_and_stays_a_unit_way) {
    const wind::Wind a = wind::at(100.0), b = wind::at(101.0);
    CHECK(std::fabs(a.x * a.x + a.z * a.z - 1.0f) < 1e-4f);
    CHECK(std::fabs(a.x - b.x) < 0.02f && std::fabs(a.z - b.z) < 0.02f);  // a second on: hardly turned
    CHECK(a.strength >= 0.2f && a.strength <= 1.0f);
}

TEST(wind_drift_adds_up_and_skips_long_pauses) {
    wind::Drift d;
    d.advance(10.0f, -2.0f, 0.1f);
    d.advance(10.0f, -2.0f, 0.1f);
    CHECK(std::fabs(d.x - 2.0f) < 1e-4f && std::fabs(d.y + 0.4f) < 1e-4f);
    d.advance(10.0f, 0.0f, 30.0f);  // a loading screen
    CHECK(d.x < 5.0f);
}
