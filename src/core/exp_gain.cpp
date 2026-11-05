#include "core/exp_gain.hpp"

#include <algorithm>
#include <cstdio>

#include "core/char_block.hpp"

namespace uagfx::expgain {

void Tracker::hit(uint32_t target, uint64_t shownAt) {
    if (hit_.size() > 256) hit_.clear();
    uint64_t& at = hit_[target];
    at = std::max(at, shownAt);
}

bool Tracker::died(uint32_t id, uint64_t* shownAt) {
    const auto it = hit_.find(id);
    if (it == hit_.end()) return false;
    if (shownAt) *shownAt = it->second;
    hit_.erase(it);
    return true;
}

void Tracker::killedAt(uint32_t, float x, float y, uint64_t ms, uint64_t shownAt) {
    kills_.push_back(Kill{x, y, ms, std::clamp(shownAt, ms, ms + kSwingMostMs)});
    if (kills_.size() > 16) kills_.erase(kills_.begin());
}

void Tracker::experience(int type, uint32_t value, uint64_t ms) {
    if (type != 1 && type != 2) return;
    if (!known_[type]) {  // the first: the whole as it was
        known_[type] = true;
        last_[type] = value;
        return;
    }
    const long long grew = (long long)value - (long long)last_[type];
    last_[type] = value;
    if (grew <= 0) return;  // a new level starts it over
    if (gain_.ms == 0) gain_.ms = ms;
    (type == 1 ? gain_.base : gain_.job) += grew;
}

void Tracker::show(const Gain& g, const Kill* k) {
    if (k) {
        floaters_.push_back(Floater{k->x, k->y, g.base, g.job, g.ms});  // g.ms: when it shows
        if (floaters_.size() > 8) floaters_.erase(floaters_.begin());
    }
    if (label_.ms && g.ms - label_.ms < 600) {  // one more right after: added up
        label_.base += g.base;
        label_.job += g.job;
        label_.ms = g.ms;
    } else {
        label_ = Label{g.base, g.job, g.ms};
    }
}

void Tracker::step(uint64_t ms) {
    if (gain_.ms) {
        const Kill* best = nullptr;  // the kill nearest the gain in time
        uint64_t bestGap = kPairMs + 1;
        for (const Kill& k : kills_) {
            const uint64_t gap = k.ms > gain_.ms ? k.ms - gain_.ms : gain_.ms - k.ms;
            if (gap <= kPairMs && gap < bestGap) {
                best = &k;
                bestGap = gap;
            }
        }
        if (best || ms - gain_.ms >= kGainWaitMs) {
            Due d{gain_, best ? *best : Kill{}, best != nullptr, gain_.ms};
            if (best) {
                d.at = std::max(d.at, best->at);
                kills_.erase(kills_.begin() + (best - kills_.data()));
            }
            due_.push_back(d);
            gain_ = Gain{};
        }
    }
    for (size_t i = 0; i < due_.size();) {  // 3.0.11-beta.246: when the game shows the death
        if (ms < due_[i].at) {
            ++i;
            continue;
        }
        Due d = due_[i];
        due_.erase(due_.begin() + long(i));
        d.gain.ms = d.at;
        show(d.gain, d.killed ? &d.kill : nullptr);
    }
    kills_.erase(std::remove_if(kills_.begin(), kills_.end(), [&](const Kill& k) { return ms - k.ms > kPairMs; }),
                 kills_.end());
    floaters_.erase(std::remove_if(floaters_.begin(), floaters_.end(),
                                   [&](const Floater& f) { return ms - f.ms >= kFloatMs; }),
                    floaters_.end());
    if (label_.ms && ms - label_.ms >= kLabelMs) label_ = Label{};
}

void Tracker::reset() { *this = Tracker{}; }

float rise(const Floater& f, uint64_t ms) {
    const float t = std::clamp(float(ms - f.ms) / float(kFloatMs), 0.0f, 1.0f);
    return kRise * (1 - (1 - t) * (1 - t));  // quick first, slowing
}

float opacity(uint64_t at, uint64_t ms, uint64_t whole, uint64_t fade) {
    if (ms < at) return 1;
    const uint64_t age = ms - at;
    if (age >= whole) return 0;
    if (age + fade <= whole) return 1;
    return float(whole - age) / float(fade);
}

std::string text(long long base, long long job, char decimal, double baseOfLevel, double jobOfLevel) {
    const auto part = [&](double share) -> std::string {
        if (share < 0) return {};
        char buf[32];
        std::snprintf(buf, sizeof buf, share * 100 >= 10 ? " (%.1f\xC2\xA0%%)" : " (%.2f\xC2\xA0%%)", share * 100);
        std::string out = buf;
        for (char& c : out)
            if (c == '.') c = decimal;
        return out;
    };
    std::string out;
    if (base > 0) out += "+" + charblock::grouped(base) + " Base" + part(baseOfLevel);
    if (job > 0) out += (out.empty() ? "" : " \xC2\xB7 ") + ("+" + charblock::grouped(job) + " Job" + part(jobOfLevel));
    return out;
}

}  // namespace uagfx::expgain
