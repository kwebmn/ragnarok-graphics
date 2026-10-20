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
    // 2.6.19: «От сервера» - the minute the server clock made of the server's night; none yet: day
    CHECK_EQ(daylight::minuteOf(daylight::kClockServer, 777, 100), 100);
    CHECK_EQ(daylight::minuteOf(daylight::kClockServer, 777, -1), 13 * 60);
}

TEST(daylight_server_clock_eases_into_the_servers_night_and_out) {
    // 2.6.19: the server's night (its status on the player) as the time of day - day 13:00, night 0:00; when it
    // changes the day goes down through the sunset, and comes back up through the dawn, in about a minute
    daylight::ServerClock s;
    CHECK_EQ(s.minute(false, 0.0), 13 * 60);  // first heard: as it is, at once
    CHECK_EQ(s.minute(true, 0.0), 13 * 60);   // the night comes: still noon this moment
    int m = s.minute(true, 20.0);
    CHECK(m > 13 * 60 && m < 24 * 60);  // the sun going down
    CHECK_EQ(s.minute(true, 90.0), 0);  // night
    m = s.minute(false, 110.0);
    CHECK(m > 0 && m < 13 * 60);  // the dawn
    CHECK_EQ(s.minute(false, 200.0), 13 * 60);
    // the night again halfway through the dawn: back the way it came, not round through the evening
    s.minute(true, 300.0);
    s.minute(true, 400.0);
    s.minute(false, 401.0);
    const int dawn = s.minute(false, 430.0);
    CHECK(dawn > 0 && dawn < 13 * 60);
    m = s.minute(true, 440.0);
    CHECK(m >= 0 && m < dawn);
    CHECK_EQ(s.minute(true, 600.0), 0);
}

TEST(daylight_server_clock_starts_as_the_server_says) {
    daylight::ServerClock s;
    CHECK_EQ(s.minute(true, 5.0), 0);  // the game came up in the server's night: night at once
}
