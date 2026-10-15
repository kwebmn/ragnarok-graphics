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

}  // namespace uagfx::daylight
