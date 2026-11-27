#include "core/frame_stats.hpp"

#include <algorithm>

namespace uagfx::framestats {

void Window::present(uint64_t nowUs) {
    if (last_ && nowUs > last_) {
        const double ms = double(nowUs - last_) / 1000.0;
        if (ms < kPauseMs) frames_.push_back(Frame{nowUs, float(ms)});
    }
    last_ = nowUs;
    while (!frames_.empty() && nowUs - frames_.front().at > kKeepUs) frames_.pop_front();
}

Stats Window::stats(uint64_t nowUs, double seconds) const {
    Stats s;
    const uint64_t span = uint64_t(std::max(0.0, seconds) * 1000000.0);
    std::vector<float> ms;
    std::vector<Long> all;
    uint64_t first = 0;
    for (const Frame& f : frames_) {
        if (nowUs < f.at || nowUs - f.at > span) continue;
        if (!first) first = f.at - uint64_t(double(f.ms) * 1000.0);
        ms.push_back(f.ms);
        all.push_back(Long{double(nowUs - f.at) / 1000000.0, double(f.ms)});
        s.over25 += f.ms > 25.0f, s.over50 += f.ms > 50.0f, s.over100 += f.ms > 100.0f;
    }
    s.frames = int(ms.size());
    if (ms.empty()) return s;
    s.seconds = double(nowUs - first) / 1000000.0;
    s.fps = s.seconds > 0 ? double(ms.size()) / s.seconds : 0;
    std::sort(ms.begin(), ms.end());
    const auto at = [&](double q) { return double(ms[std::min(ms.size() - 1, size_t(q * double(ms.size())))]); };
    s.p50 = at(0.50), s.p95 = at(0.95), s.p99 = at(0.99), s.worst = double(ms.back());
    std::sort(all.begin(), all.end(), [](const Long& a, const Long& b) { return a.ms > b.ms; });
    all.resize(std::min<size_t>(all.size(), 5));
    s.longest = std::move(all);
    return s;
}

}  // namespace uagfx::framestats
