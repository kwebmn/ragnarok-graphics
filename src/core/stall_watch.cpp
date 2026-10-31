#include "core/stall_watch.hpp"

namespace uagfx::stallwatch {

Event Watch::look(uint32_t beats, uint64_t nowMs) {
    if (!seen_ || beats != beats_) {
        const bool ended = still_;
        if (ended) stillMs_ = nowMs - changedMs_;
        seen_ = true;
        still_ = false;
        beats_ = beats;
        changedMs_ = nowMs;
        return ended ? Event::Ended : Event::None;
    }
    if (still_ || nowMs - changedMs_ < kStillMs) return Event::None;
    still_ = true;
    return Event::Began;
}

}  // namespace uagfx::stallwatch
