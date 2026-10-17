#pragma once

namespace uagfx::wind {

// 1.22, one wind for the whole world (effects.shared_wind): its way over the ground turns slowly (minutes), its
// strength comes and goes; the grass, the leaves, the sand, the rain, the snow and the clouds all go with it.
struct Wind {
    float x = 1.0f, z = 0.0f;  // its way in the world (a unit vector over the ground)
    float strength = 0.5f;     // 0.2-1
};

Wind at(double seconds);

// What the wind has carried a thing so far: added up frame by frame, so that a change of the wind's way or of the
// camera's turns its path, not its place.
struct Drift {
    float x = 0.0f, y = 0.0f;
    void advance(float vx, float vy, float dt);  // speed (units a second) over dt seconds; wraps far off
};

}  // namespace uagfx::wind
