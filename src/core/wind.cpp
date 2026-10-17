#include "core/wind.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::wind {

Wind at(double seconds) {
    const double t = seconds;
    const double angle = 0.35 + 0.9 * std::sin(t / 400.0) + 0.3 * std::sin(t / 97.0 + 1.3);
    Wind w;
    w.x = float(std::cos(angle));
    w.z = float(std::sin(angle));
    w.strength = float(std::clamp(0.55 + 0.25 * std::sin(t / 53.0) + 0.15 * std::sin(t / 17.3 + 0.7), 0.2, 1.0));
    return w;
}

void Drift::advance(float vx, float vy, float dt) {
    dt = std::clamp(dt, 0.0f, 0.25f);  // a long pause (a loading screen) is not a long flight
    x += vx * dt;
    y += vy * dt;
    // far off the pattern repeats anyway: kept small for the floats' precision
    if (std::fabs(x) > 100000.0f) x = std::fmod(x, 100000.0f);
    if (std::fabs(y) > 100000.0f) y = std::fmod(y, 100000.0f);
}

}  // namespace uagfx::wind
