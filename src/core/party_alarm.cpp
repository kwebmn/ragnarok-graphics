#include "core/party_alarm.hpp"

#include <cmath>

namespace uagfx::partyalarm {

bool Watch::step(const std::map<uint32_t, int>& percents, uint64_t now) {
    bool fresh = false;
    for (const auto& [account, pct] : percents) {
        const auto was = last_.find(account);
        if (was != last_.end() && pct >= 0 && pct < kLow && was->second >= kLow) {
            fell_[account] = now;
            fresh = true;
        }
    }
    last_ = percents;
    for (auto it = fell_.begin(); it != fell_.end();)
        it = now - it->second > kPulseMs ? fell_.erase(it) : std::next(it);
    return fresh;
}

float Watch::pulse(uint32_t account, uint64_t now) const {
    const auto it = fell_.find(account);
    if (it == fell_.end() || now - it->second > kPulseMs) return 0;
    const float t = float(now - it->second) / 1000.0f;
    return std::round((0.5f + 0.5f * std::cos(t * 2 * 3.14159265f * 1.5f)) * 8) / 8;  // 1.5 waves a second
}

}  // namespace uagfx::partyalarm
