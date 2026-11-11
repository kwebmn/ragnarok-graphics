#include "core/part_clock.hpp"

#include <algorithm>
#include <cstdio>

namespace uagfx::partclock {

void Clock::begin(double nowMs) { last_ = nowMs; }

void Clock::mark(std::string_view name, double nowMs) {
    if (last_ < 0) return;  // no present begun
    ms_[name] += std::max(0.0, nowMs - last_);
    last_ = nowMs;
}

std::string Clock::take(int top) {
    std::string line;
    if (frames_) {
        const double f = double(frames_);
        std::vector<std::pair<std::string_view, double>> parts(ms_.begin(), ms_.end());
        std::sort(parts.begin(), parts.end(), [](const auto& a, const auto& b) {
            return a.second != b.second ? a.second > b.second : a.first < b.first;
        });
        double all = 0, rest = 0;
        for (size_t i = 0; i < parts.size(); ++i) {
            all += parts[i].second;
            if (int(i) >= top) rest += parts[i].second;
        }
        char head[96];
        std::snprintf(head, sizeof head, "the driver's parts: %.2f ms a frame in %d", all / f, int(parts.size()));
        line = head;
        const char* sep = " - ";
        for (int i = 0; i < top && i < int(parts.size()); ++i) {
            char part[96];
            std::snprintf(part, sizeof part, "%s%.*s %.2f", sep, int(parts[size_t(i)].first.size()),
                          parts[size_t(i)].first.data(), parts[size_t(i)].second / f);
            line += part;
            sep = ", ";
        }
        if (rest > 0) {
            char part[48];
            std::snprintf(part, sizeof part, "%sthe rest %.2f", sep, rest / f);
            line += part;
        }
    }
    ms_.clear();
    last_ = -1;
    frames_ = 0;
    return line;
}

}  // namespace uagfx::partclock
