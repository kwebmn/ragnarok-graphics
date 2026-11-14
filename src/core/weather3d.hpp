#pragma once
#include <cstdint>

namespace uagfx::weather3d {

// 3.0.19-beta.40 (the owner's «делаем все по порядку»: in the 3D view the weather fell as flat layers over the
// picture - the screen's three, made for the game's own camera far up): the 3D view's weather - particles in the
// world round the eye (weather3d_vs; weather.cpp applyWeather3d, renderer11 applyWeather). For a kind (weather_ps's:
// 1 snow, 2 rain, 3 leaves, 5 pollen, 6 dust; 4 sand and the rest none - the screen's air as it is) at `amount`
// (1 = 100 %, at most 2): how many in the box round the eye (the world's own lattice they repeat in), its side and
// height (world units, a walk cell 5), their fall (units a second, y down), how much the shared wind blows them, their
// half size and the rain's streak's length
constexpr float kUnder = 15.0f;  // the box from this far under the eye (the feet 13) up
constexpr float kWindUnits = 6.0f;  // the shared wind at its strongest blows a particle (blown 1) this far a second
// the drift kept within this (its fields' boxes' sides go into it whole: no jump as it goes round)
constexpr float kDriftRound = 8400.0f;
struct Field {
    uint32_t count = 0;
    float side = 0.0f, high = 0.0f;
    float fall = 0.0f, blown = 0.0f;
    float half = 0.0f, streak = 0.0f;
    uint32_t vertices() const { return count * 6; }
};
Field fieldOf(int kind, float amount);

}  // namespace uagfx::weather3d
