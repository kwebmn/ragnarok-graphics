#include "core/footsteps.hpp"

#include <cmath>

namespace uagfx::footsteps {

void Tracker::update(const std::vector<map::Point>& feet, float time, float life) {
    std::vector<bool> taken(walkers_.size(), false);
    for (const map::Point& f : feet) {
        // The walker it is: the nearest seen last within 8 units (a character runs about 3 a frame at worst).
        int best = -1;
        float bestD = 8.0f;
        for (size_t i = 0; i < walkers_.size(); ++i) {
            if (taken[i]) continue;
            const float d = std::hypot(walkers_[i].x - f.x, walkers_[i].z - f.z);
            if (d < bestD) {
                bestD = d;
                best = int(i);
            }
        }
        if (best < 0) {
            walkers_.push_back(Walker{f.x, f.y, f.z, f.x, f.z, time, false});
            taken.push_back(true);
            continue;
        }
        taken[size_t(best)] = true;
        Walker& w = walkers_[size_t(best)];
        w.x = f.x;
        w.y = f.y;
        w.z = f.z;
        w.seen = time;
        const float dx = f.x - w.lastX, dz = f.z - w.lastZ, d = std::hypot(dx, dz);
        if (d < kStride) continue;
        if (d > kStride * 6.0f) {  // a warp, a jump of the camera: no trail across
            w.lastX = f.x;
            w.lastZ = f.z;
            continue;
        }
        const float side = (w.left ? -0.9f : 0.9f) / d;  // aside of its way, left and right by turns
        Step s;
        s.x = f.x - dz * side;
        s.y = f.y;
        s.z = f.z + dx * side;
        s.angle = std::atan2(dz, dx);
        s.born = time;
        steps_.push_back(s);
        w.left = !w.left;
        w.lastX = f.x;
        w.lastZ = f.z;
    }
    for (size_t i = walkers_.size(); i-- > 0;)
        if (time - walkers_[i].seen > 1.0f) walkers_.erase(walkers_.begin() + long(i));
    while (!steps_.empty() && (time - steps_.front().born > life || steps_.size() > kMax)) steps_.pop_front();
}

void Tracker::clear() {
    walkers_.clear();
    steps_.clear();
}

}  // namespace uagfx::footsteps
