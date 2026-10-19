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

// 2.6.11, [effects] clock: the time of day all that goes by the hour takes (the day's sun, the night by the clock, the
// creatures of the day, the dew, the morning fog) - the computer's clock, or a moment the player chose, kept fixed:
// dawn 6:30, morning 9:00, day 13:00, sunset 18:30, night 0:00 (in the order of the file's words).
enum Clock { kClockAuto, kClockDawn, kClockMorning, kClockDay, kClockSunset, kClockNight, kClocks };
int minuteOf(int clock, int localMinute);  // localMinute: the computer's, 0-1439

}  // namespace uagfx::daylight
