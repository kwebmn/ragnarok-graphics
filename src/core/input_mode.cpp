#include "core/input_mode.hpp"

namespace uagfx::inputmode {

constexpr uint64_t kSteadyMs = 1500;
constexpr float kHalf = 0.5f;

bool intentional(uint16_t down, float lx, float ly, float rx, float ry) {
    return down != 0 || lx * lx + ly * ly > kHalf * kHalf || rx * rx + ry * ry > kHalf * kHalf;
}

bool mouseMoved(int dx, int dy) { return dx * dx + dy * dy > 64; }

bool Mode::padShown(uint64_t nowMs) {
    const bool want = padAt_ > mouseAt_;
    if (want != shown_ && (!changed_ || nowMs - changedAt_ >= kSteadyMs)) {
        shown_ = want;
        changed_ = true;
        changedAt_ = nowMs;
    }
    return shown_;
}

}  // namespace uagfx::inputmode
