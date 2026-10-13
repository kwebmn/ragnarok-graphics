#pragma once
#include <cstdint>

namespace uagfx {

// A log line about something that may change every frame (a window being resized) is written at most once per
// interval, and the last state still once things settle (stage 3 review M7).
class Throttle {
public:
    explicit Throttle(uint64_t intervalMs) : interval_(intervalMs) {}
    // A change at now: true = write it now; false = due() writes the state later.
    bool changed(uint64_t now) {
        if (!wrote_ || now - last_ >= interval_) {
            mark(now);
            return true;
        }
        pending_ = true;
        return false;
    }
    // On every frame: true = a change was held back and its time has come; write the current state.
    bool due(uint64_t now) {
        if (!pending_ || now - last_ < interval_) return false;
        mark(now);
        return true;
    }
    // A change is held back: written at close if the presents stop before its time (0.4.1 review).
    bool pending() const { return pending_; }

private:
    void mark(uint64_t now) {
        last_ = now;
        wrote_ = true;
        pending_ = false;
    }
    uint64_t interval_;
    uint64_t last_ = 0;
    bool wrote_ = false;
    bool pending_ = false;
};

}  // namespace uagfx
