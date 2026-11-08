#include "core/afterimage.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::afterimage {

void Trail::note(const float at[3], uint64_t ms) {
    if (!samples_.empty()) {
        const Sample& last = samples_.back();
        const float d = std::hypot(at[0] - last.at[0], at[2] - last.at[2]);
        if (d > 60.0f) samples_.clear();  // a warp or a new map: no trail across it
    }
    samples_.push_back(Sample{ms, {at[0], at[1], at[2]}});
    while (samples_.size() > 2 && ms - samples_.front().ms > 800) samples_.pop_front();
}

std::array<float, 3> Trail::at(uint64_t ms) const {
    if (samples_.empty()) return {};
    if (ms <= samples_.front().ms) return samples_.front().at;
    for (size_t i = 1; i < samples_.size(); ++i)
        if (samples_[i].ms >= ms) {
            const Sample& a = samples_[i - 1];
            const Sample& b = samples_[i];
            const float t = b.ms > a.ms ? float(ms - a.ms) / float(b.ms - a.ms) : 1.0f;
            return {a.at[0] + (b.at[0] - a.at[0]) * t, a.at[1] + (b.at[1] - a.at[1]) * t,
                    a.at[2] + (b.at[2] - a.at[2]) * t};
        }
    return samples_.back().at;
}

float Trail::speed(uint64_t ms) const {
    if (samples_.size() < 2 || ms < 200 || samples_.front().ms > ms - 200) return 0;
    const std::array<float, 3> a = at(ms - 200), b = at(ms);
    return std::hypot(b[0] - a[0], b[2] - a[2]) / 0.2f;
}

std::vector<Ghost> Trail::ghosts(uint64_t ms) {
    if (speed(ms) > kFastUnits) fastUntil_ = ms + kTailMs;
    std::vector<Ghost> out;
    if (ms >= fastUntil_) return out;
    const float fade = std::min(1.0f, float(fastUntil_ - ms) / float(kTailMs));
    const std::array<float, 3> now = at(ms);
    for (int k = 1; k <= kGhosts; ++k) {
        Ghost g;
        g.at = at(ms - uint64_t(k) * kStepMs);
        if (std::hypot(g.at[0] - now[0], g.at[2] - now[2]) < 1.0f) continue;  // not apart yet
        g.alpha = fade * (0.42f - 0.11f * float(k));
        out.push_back(g);
    }
    return out;
}

Tint tintOf(bool fury, bool steelBody) {
    if (steelBody) return Tint{150, 175, 205, 0.22f};
    if (fury) return Tint{255, 96, 48, 0.18f};
    return Tint{};
}

}  // namespace uagfx::afterimage
