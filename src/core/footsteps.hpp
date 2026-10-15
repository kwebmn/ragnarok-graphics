#pragma once
#include <deque>
#include <vector>

#include "core/map_axes.hpp"

namespace uagfx::footsteps {

// 0.80: footprints in the snow and the sand, dust where they run on dry ground (effects.footprints). The characters'
// feet each frame (their round shadows' middles, in the world) are followed from frame to frame; one that has gone
// far enough since its last step leaves another - left and right by turns, a little aside of its way.
struct Step {
    float x = 0, y = 0, z = 0;  // the world (up is -y)
    float angle = 0;             // its way along the ground: atan2(dz, dx)
    float born = 0;              // the time it was left, seconds
};

class Tracker {
public:
    static constexpr size_t kMax = 64;   // the newest kept
    static constexpr float kStride = 4.5f;  // world units between steps
    void update(const std::vector<map::Point>& feet, float time, float life);
    const std::deque<Step>& steps() const { return steps_; }
    void clear();

private:
    struct Walker {
        float x, y, z;          // where it was last seen
        float lastX, lastZ;     // where it left its last step
        float seen;
        bool left;
    };
    std::vector<Walker> walkers_;
    std::deque<Step> steps_;
};

}  // namespace uagfx::footsteps
