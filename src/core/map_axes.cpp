#include "core/map_axes.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::map {
namespace {

// The median distance along the ground from each seen point to the nearest placed one turned by (x, z).
float medianDistance(const std::vector<Point>& placed, const std::vector<Point>& seen, int x, int z) {
    std::vector<float> d;
    d.reserve(seen.size());
    for (const Point& s : seen) {
        float best = 1e30f;
        for (const Point& p : placed) {
            const float dx = s.x - x * p.x, dz = s.z - z * p.z;
            best = std::min(best, dx * dx + dz * dz);
        }
        d.push_back(std::sqrt(best));
    }
    std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
    return d[d.size() / 2];
}

}  // namespace

Axes fitAxes(const std::vector<Point>& placed, const std::vector<Point>& seen, float* spread) {
    if (spread) *spread = 0.0f;
    if (placed.empty() || seen.size() < 3) return Axes{};
    float best = 1e30f, second = 1e30f;
    Axes found;
    for (int x : {1, -1})
        for (int z : {1, -1}) {
            const float m = medianDistance(placed, seen, x, z);
            if (m < best) {
                second = best;
                best = m;
                found = Axes{int8_t(x), 1, int8_t(z)};
            } else {
                second = std::min(second, m);
            }
        }
    if (spread) *spread = best;
    // Close (the flames are measured from the screen) and clearly closer than any other turn.
    if (best > 8.0f || second < std::max(15.0f, 2.5f * best)) return Axes{};
    return found;
}

}  // namespace uagfx::map
