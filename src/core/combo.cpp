#include "core/combo.hpp"

#include <algorithm>

namespace uagfx::combo {

std::vector<uint16_t> nextOf(uint16_t skill) {
    switch (skill) {
        case 263: return {272};            // Triple Attack -> Chain Combo
        case 272: return {273};            // Chain Combo -> Combo Finish
        case 273: return {371, 372, 271};  // Combo Finish -> Tiger Fist, Chain Crush, Asura
        case 371: return {372, 271};       // Tiger Fist -> Chain Crush, Asura
        case 372: return {271};            // Chain Crush -> Asura
        case 46: return {499};             // Double Strafe -> Beast Strafing
        default: return {};
    }
}

void Window::opened(uint32_t waitMs, uint64_t now) {
    openedAt_ = now;
    until_ = now + waitMs;
    total_ = std::max<uint32_t>(1, waitMs);
    after_ = 0;
}

void Window::skillDone(uint16_t skill, uint64_t now) {
    const bool pending = openedAt_ && after_ == 0 && now >= openedAt_ && now - openedAt_ <= kPairMs;
    if (pending) {
        after_ = skill;
    } else if (now < until_ && after_) {
        const std::vector<uint16_t> next = nextOf(after_);
        if (std::find(next.begin(), next.end(), skill) != next.end()) until_ = now;  // chained (a new 0x01d2 opens on)
    }
    last_ = skill;
    lastAt_ = now;
}

uint16_t Window::after(uint64_t now) const {
    if (now >= until_) return 0;
    if (after_) return after_;
    // no damage of its own came after it yet: past the pairing time the last skill before it
    return now - openedAt_ > kPairMs && lastAt_ + kPairMs >= openedAt_ && lastAt_ <= openedAt_ ? last_ : 0;
}

float Window::left(uint64_t now) const {
    return now >= until_ ? 0.0f : float(until_ - now) / float(total_);
}

}  // namespace uagfx::combo
