#include "pad/core/logic/mouse_gate.hpp"

namespace uapad {

void MouseGate::watch(bool foreground, uint32_t nowMs) {
    if (foreground && !foreground_) frontSinceMs_ = nowMs;
    if (!foreground && foreground_) reset();
    foreground_ = foreground;
}

MouseGate::Out MouseGate::filter(uint8_t buttons, bool overGame, bool focusClick, uint32_t nowMs) {
    passing_ &= buttons;  // let go: forgotten
    blocked_ &= buttons;
    const uint8_t fresh = uint8_t(buttons & ~(passing_ | blocked_));
    const bool activating = frontSinceMs_ && nowMs - *frontSinceMs_ < kActivationMs;
    if ((activating && !focusClick) || !overGame)
        blocked_ |= fresh;
    else
        passing_ |= fresh;
    Out out;
    out.buttons = passing_;
    out.wheel = overGame;
    return out;
}

void MouseGate::claim(uint8_t buttons) {
    blocked_ |= buttons;
    passing_ &= uint8_t(~buttons);
}

void MouseGate::reset() {
    passing_ = 0;
    blocked_ = 0;
}

std::vector<MouseKeys::KeyEvent> MouseKeys::update(uint8_t held, const std::array<int, 3>& keys, bool allowed,
                                                   bool foreground) {
    std::vector<KeyEvent> out;
    const uint8_t pressed = uint8_t(held & ~prevHeld_);
    prevHeld_ = held;
    auto holders = [&](int key) {
        int n = 0;
        for (int h : holds_) n += h == key ? 1 : 0;
        return n;
    };
    for (int i = 0; i < 3; ++i) {  // let go: the button, the game window, or the key it is set to
        const int key = holds_[size_t(i)];
        if (key == 0 || (((held >> i) & 1) && foreground && keys[size_t(i)] == key)) continue;
        holds_[size_t(i)] = 0;
        if (holders(key) == 0) out.push_back(KeyEvent{key, false});
    }
    for (int i = 0; i < 3; ++i) {  // a new press
        const int key = keys[size_t(i)];
        if (!((pressed >> i) & 1) || !allowed || !foreground || key < 1 || key > 9) continue;
        const bool down = holders(key) > 0;  // another button holds it already
        holds_[size_t(i)] = key;
        if (!down) out.push_back(KeyEvent{key, true});
    }
    return out;
}

std::vector<MouseKeys::KeyEvent> MouseKeys::releaseAll() {
    std::vector<KeyEvent> out;
    for (size_t i = 0; i < holds_.size(); ++i) {
        const int key = holds_[i];
        if (key == 0) continue;
        holds_[i] = 0;
        bool still = false;
        for (int h : holds_) still = still || h == key;
        if (!still) out.push_back(KeyEvent{key, false});
    }
    return out;
}

int backgroundSleepMs(bool queueEmpty, bool foreground, bool minimized, int sleepMs) {
    if (!queueEmpty || sleepMs <= 0) return 0;
    return !foreground || minimized ? sleepMs : 0;
}

int blockedAt(const Blocked* parts, int count, int x, int y) {
    for (int i = 0; i < count; ++i) {
        const Blocked& b = parts[i];
        if (b.all || (b.on && x >= b.left && x < b.right && y >= b.top && y < b.bottom)) return i;
    }
    return -1;
}

int blockedAtOrdered(const Blocked* parts, int count, const std::vector<int>& first, int x, int y) {
    const auto over = [&](int i) {
        const Blocked& b = parts[i];
        return b.all || (b.on && x >= b.left && x < b.right && y >= b.top && y < b.bottom);
    };
    for (const int i : first)
        if (i >= 0 && i < count && over(i)) return i;
    return blockedAt(parts, count, x, y);
}

int blockedAtGame(const Blocked* parts, int count, int clientW, int clientH, int x, int y, int width, int height,
                  const std::vector<int>& first) {
    if (clientW <= 0 || clientH <= 0 || width <= 0 || height <= 0) return -1;
    return blockedAtOrdered(parts, count, first, int(int64_t(x) * clientW / width), int(int64_t(y) * clientH / height));
}

}  // namespace uapad
