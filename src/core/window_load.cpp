#include "core/window_load.hpp"

#include <cmath>
#include <cstdio>

namespace uagfx::windowload {
namespace {

constexpr double kBefore = 5.0;  // the seconds of the game alone compared with
constexpr double kLeast = 2.0;   // a window open for less is not measured
constexpr double kPause = 0.5;   // a frame this long is a pause (a map's loading, another window in front): skipped

}  // namespace

void Watch::Sum::add(const Frame& f) {
    seconds += f.took;
    ownMs += f.ownMs;
    ++frames;
    if (f.gpuMs >= 0) gpuMs += f.gpuMs, ++gpuFrames;
}

void Watch::Sum::take(const Frame& f) {
    seconds -= f.took;
    ownMs -= f.ownMs;
    --frames;
    if (f.gpuMs >= 0) gpuMs -= f.gpuMs, --gpuFrames;
}

std::string Watch::Sum::say(bool first) const {
    const double fps = seconds > 0 ? double(frames) / seconds : 0;
    char text[160];
    std::snprintf(text, sizeof text, "%.0f frames a second", fps);
    std::string out = text;
    if (gpuFrames > 0) {
        const double ms = gpuMs / double(gpuFrames);
        std::snprintf(text, sizeof text, ", the card %.1f ms a frame (%.0f %%%s)", ms, ms * fps / 10.0,
                      first ? " of its time" : "");
        out += text;
    }
    const double own = frames > 0 ? ownMs / double(frames) : 0;
    std::snprintf(text, sizeof text, ", the driver's own work %.2f ms a frame", own);
    return out + text;
}

bool Watch::frame(double now, bool open, double gpuMs, double ownMs) {
    const double took = last_ < 0 ? 0 : now - last_;
    last_ = now;
    bool closed = false;
    if (wasOpen_ && !open) {  // closed: told when it was open long enough
        if (open_.seconds >= kLeast && open_.frames > 0) {
            char text[64];
            std::snprintf(text, sizeof text, "open %.1f s - ", open_.seconds);
            told_ = text + open_.say(true);
            if (before_.seconds >= kBefore * 0.9 && before_.frames > 0) {
                std::snprintf(text, sizeof text, "; in the %.0f s before it ", kBefore);
                told_ += text + before_.say(false);
            } else {
                told_ += "; opened too soon to compare with the game alone";
            }
            closed = true;
        }
        alone_.clear();
        shut_ = Sum{};
    }
    if (!wasOpen_ && open) {  // opened: the game alone as it was just now
        before_ = shut_;
        open_ = Sum{};
    }
    wasOpen_ = open;
    if (took <= 0 || took > kPause) return closed;
    const Frame f{took, gpuMs, ownMs};
    if (open) {
        open_.add(f);
    } else {
        alone_.push_back(f);
        shut_.add(f);
        while (shut_.seconds - alone_.front().took >= kBefore) {
            shut_.take(alone_.front());
            alone_.pop_front();
        }
    }
    return closed;
}

}  // namespace uagfx::windowload
