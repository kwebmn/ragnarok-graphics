#include "core/exp_rate.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::exprate {

void Track::feed(uint64_t nowMs, int level, long long exp, long long next) {
    if (level <= 0 || exp < 0) return;
    if (!known_) {
        known_ = true;
        first_ = nowMs;
        level_ = level;
        exp_ = exp;
        next_ = next;
        return;
    }
    long long gain = 0;
    if (level == level_) {
        if (exp == 0 && exp_ > 0) return;  // the game's numbers not come yet
        if (exp > exp_) gain = exp - exp_;
    } else if (level > level_) {
        gain = std::max(0LL, next_ - exp_) + exp;
    }
    level_ = level;
    exp_ = exp;
    next_ = next;
    if (gain > 0) {
        session_ += gain;
        last_ = gain;
        gains_.push_back(Gain{nowMs, gain});
    }
    while (!gains_.empty() && gains_.front().ms + kWindowMs < nowMs) gains_.pop_front();
}

void Track::reset() { *this = Track{}; }

double Track::perHour(uint64_t nowMs) const {
    if (!known_ || nowMs < first_) return -1;
    const uint64_t span = std::min(kWindowMs, nowMs - first_);
    if (span < kLeastMs) return -1;
    long long sum = 0;
    for (const Gain& g : gains_)
        if (g.ms + kWindowMs >= nowMs && g.ms <= nowMs) sum += g.exp;
    return double(sum) * 3600000.0 / double(span);
}

long long minutesTo(long long exp, long long next, double perHour) {
    if (next <= 0 || perHour <= 0) return -1;
    const double minutes = double(std::max(0LL, next - exp)) / perHour * 60.0;
    return (long long)std::ceil(minutes - 1e-9);
}

}  // namespace uagfx::exprate
