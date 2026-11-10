#include "core/pad_nav.hpp"

#include <cmath>

namespace uagfx::padnav {

int first(const std::vector<panel::Box>& targets) {
    int best = -1;
    for (int i = 0; i < int(targets.size()); ++i) {
        const panel::Box& b = targets[size_t(i)];
        if (best < 0) {
            best = i;
            continue;
        }
        const panel::Box& o = targets[size_t(best)];
        if (b.y + b.h / 2 < o.y - 1 || (std::fabs(b.y - o.y) <= 1 && b.x < o.x)) best = i;
    }
    return best;
}

int next(const std::vector<panel::Box>& targets, int from, Dir d) {
    if (from < 0 || from >= int(targets.size())) return first(targets);
    const panel::Box& a = targets[size_t(from)];
    const float ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int best = from;
    float bestScore = 0;
    for (int i = 0; i < int(targets.size()); ++i) {
        if (i == from) continue;
        const panel::Box& b = targets[size_t(i)];
        const float bx = b.x + b.w / 2, by = b.y + b.h / 2;
        float along = 0, across = 0;
        switch (d) {
            case Dir::Up: along = a.y - (b.y + b.h), across = std::fabs(bx - ax); break;
            case Dir::Down: along = b.y - (a.y + a.h), across = std::fabs(bx - ax); break;
            case Dir::Left: along = a.x - (b.x + b.w), across = std::fabs(by - ay); break;
            case Dir::Right: along = b.x - (a.x + a.w), across = std::fabs(by - ay); break;
        }
        // ahead: its near edge past (or touching) ours, allowing a pixel of overlap
        if (along < -1) {
            const float centre = d == Dir::Up ? ay - by : d == Dir::Down ? by - ay : d == Dir::Left ? ax - bx : bx - ax;
            if (centre <= 0) continue;
            along = 0;
        }
        const float score = along + 2 * across;
        if (best == from || score < bestScore) best = i, bestScore = score;
    }
    return best;
}

}  // namespace uagfx::padnav
