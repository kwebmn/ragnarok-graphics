#include <cmath>

#include "core/daylight.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(daylight_off_is_the_usual_sun) {
    const daylight::Light l = daylight::at(-1);
    CHECK(l.shadow[0] == 0.83f && l.shadow[1] == 0.56f && l.length == 1.0f && l.tint[2] == 1.0f);
}

TEST(daylight_turns_the_shadows_and_warms_the_ends_of_the_day) {
    // 0.74: the morning's shadows fall left, the evening's right, noon's short and straight down the screen.
    const daylight::Light morning = daylight::at(7 * 60), noon = daylight::at(13 * 60), evening = daylight::at(19 * 60);
    CHECK(morning.shadow[0] < -0.5f && evening.shadow[0] > 0.5f);
    CHECK(std::fabs(noon.shadow[0]) < 0.05f && noon.shadow[1] > 0.99f);
    CHECK(noon.length < 0.6f && morning.length > 1.0f && evening.length > 1.0f);
    for (const daylight::Light& l : {morning, noon, evening})
        CHECK(std::fabs(l.shadow[0] * l.shadow[0] + l.shadow[1] * l.shadow[1] - 1.0f) < 1e-4f);
    CHECK(evening.tint[0] > 1.03f && evening.tint[2] < 0.9f);  // golden
    CHECK(morning.tint[0] > 1.02f);                            // pink
    CHECK(std::fabs(noon.tint[0] - 1.0f) < 0.01f);
}

TEST(daylight_clock_gives_the_minute_of_the_day) {
    // 2.6.11, [effects] clock: the computer's minute, or a moment the player chose - fixed, for all that goes by
    // the hour
    CHECK_EQ(daylight::minuteOf(daylight::kClockAuto, 777), 777);
    CHECK_EQ(daylight::minuteOf(daylight::kClockDawn, 777), 6 * 60 + 30);
    CHECK_EQ(daylight::minuteOf(daylight::kClockMorning, 777), 9 * 60);
    CHECK_EQ(daylight::minuteOf(daylight::kClockDay, 777), 13 * 60);
    CHECK_EQ(daylight::minuteOf(daylight::kClockSunset, 777), 18 * 60 + 30);
    CHECK_EQ(daylight::minuteOf(daylight::kClockNight, 777), 0);
    CHECK_EQ(daylight::minuteOf(42, 777), 777);  // none of them: the computer's
}
