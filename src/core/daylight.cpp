#include "core/daylight.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::daylight {

int minuteOf(int clock, int localMinute, int serverMinute) {
    static const int kMinutes[kClocks] = {-1, -1, 6 * 60 + 30, 9 * 60, 13 * 60, 18 * 60 + 30, 0, 3 * 60};
    if (clock == kClockServer) return serverMinute >= 0 ? serverMinute % (24 * 60) : 13 * 60;
    return clock > kClockServer && clock < kClocks ? kMinutes[clock] : localMinute;
}

int ServerClock::minute(bool night, double now) {
    constexpr double kDay = 13 * 60.0, kMidnight = 24 * 60.0;
    if (last_ < 0.0) {
        minute_ = night ? 0.0 : kDay;
        last_ = now;
        return int(minute_);
    }
    const double step = std::max(0.0, now - last_) * kMinutesPerSecond;
    last_ = now;
    const bool evening = minute_ >= kDay;
    if (night)
        minute_ = evening ? std::min(kMidnight, minute_ + step) : std::max(0.0, minute_ - step);
    else
        minute_ = evening ? std::max(kDay, minute_ - step) : std::min(kDay, minute_ + step);
    if (minute_ >= kMidnight) minute_ = 0.0;
    return int(minute_);
}

Light at(int minute) {
    Light l;
    if (minute < 0) return l;
    const float hours = float(minute % 1440) / 60.0f;
    const float day = std::clamp((hours - 6.0f) / 14.0f, 0.0f, 1.0f);  // 0 at sunrise, 1 at sunset
    const float pi = 3.14159265f;
    const float height = std::sin(day * pi);  // 0 on the horizon, 1 at noon
    const float turn = (day - 0.5f) * 2.0f * 0.98f;  // -0.98 in the morning (the sun on the right) to 0.98
    l.shadow[0] = std::sin(turn);
    l.shadow[1] = std::cos(turn);
    l.length = 0.55f + 1.05f * (1.0f - height);
    const float low = (1.0f - height) * (1.0f - height);
    const float morning[3] = {1.07f, 0.95f, 0.92f}, evening[3] = {1.10f, 0.96f, 0.80f};
    const float* warm = day < 0.5f ? morning : evening;
    for (int k = 0; k < 3; ++k) l.tint[k] = 1.0f + (warm[k] - 1.0f) * low;
    return l;
}

}  // namespace uagfx::daylight
