#pragma once
#include <cstdint>

namespace uagfx {

// Something that failed is tried again after a pause: not on every frame, and not never (stage 3 review M1 - a swap
// chain size that once failed was never tried again, and the window froze until its size changed).
class RetryTimer {
public:
    explicit RetryTimer(uint64_t pauseMs) : pause_(pauseMs) {}
    void failed(uint64_t now) {
        failing_ = true;
        failedAt_ = now;
    }
    void succeeded() { failing_ = false; }
    bool mayTry(uint64_t now) const { return !failing_ || now - failedAt_ >= pause_; }

private:
    uint64_t pause_;
    uint64_t failedAt_ = 0;
    bool failing_ = false;
};

}  // namespace uagfx
