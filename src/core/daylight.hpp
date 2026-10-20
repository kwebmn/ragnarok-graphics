#pragma once

namespace uagfx::daylight {

// 0.74, the day going round (effects.day_cycle): by the computer's clock the sun rises at 6:00 on the right of the
// screen and sets at 20:00 on its left (the night has the hours between) - the characters' shadows fall the other way,
// long in the morning and the evening, short at noon; the light pink in the morning, white at noon, golden in the
// evening.
struct Light {
    float shadow[2] = {0.83f, 0.56f};  // the way the shadows fall on the screen, a unit vector (x right, y down)
    float length = 1.0f;               // the shadows' length, times the usual
    float tint[3] = {1.0f, 1.0f, 1.0f};
};

// minute: of the day, 0-1439; below 0 the cycle is off - the usual sun (as from behind the camera's left), no tint.
Light at(int minute);

// 2.6.11, [effects] clock: the time of day all that goes by the hour takes (the day's sun, the night, the creatures
// of the day, the dew, the morning fog) - the computer's clock, or a moment the player chose, kept fixed: dawn 6:30,
// morning 9:00, day 13:00, sunset 18:30, night 0:00 (in the order of the file's words). 2.6.19: the master of the
// time of day, by the Quality and the Mode (the user: «Часы главные»), and «От сервера» - the server's night
// (ServerClock's minute).
enum Clock { kClockAuto, kClockServer, kClockDawn, kClockMorning, kClockDay, kClockSunset, kClockNight, kClocks };
// localMinute: the computer's, 0-1439; serverMinute: ServerClock's (below 0: nothing heard - day)
int minuteOf(int clock, int localMinute, int serverMinute = -1);

// 2.6.19: the server's night as the time of day - its status SI_NIGHT on the player (core/server_night.hpp: the maps
// that have nights): day 13:00, night 0:00. When it comes the day goes down through the sunset, when it goes it comes
// up through the dawn, kMinutesPerSecond (about a minute either way); turned halfway, back the way it came. First
// heard: as it is, at once.
class ServerClock {
public:
    static constexpr double kMinutesPerSecond = 13.0;
    int minute(bool night, double now);  // now: seconds, any steady clock

private:
    double minute_ = 13 * 60.0;  // the evening runs from 13:00 to 24:00 (the night), the morning from 0:00 to 13:00
    double last_ = -1.0;
};

}  // namespace uagfx::daylight
