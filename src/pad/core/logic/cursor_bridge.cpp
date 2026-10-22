#include "pad/core/logic/cursor_bridge.hpp"

#include <algorithm>
#include <cstdint>

namespace uapad {

CursorBridge::Step CursorBridge::step(int targetX, int targetY, int width, int height) {
    Step s;
    if (width <= 0 || height <= 0) return s;
    if (width != width_ || height != height_) {
        synced_ = false;
        width_ = width;
        height_ = height;
    }
    const int x = std::clamp(targetX, 0, width - 1);
    const int y = std::clamp(targetY, 0, height - 1);
    if (!synced_) {
        synced_ = true;
        x_ = 0;
        y_ = 0;
        s.dx = -kSlam;
        s.dy = -kSlam;
        s.settling = true;
        return s;
    }
    s.dx = x - x_;
    s.dy = y - y_;
    x_ = x;
    y_ = y;
    return s;
}

CursorBridge::Step CursorBridge::stepExact(int targetX, int targetY, int width, int height, int gameW, int gameH,
                                           int cursorX, int cursorY) {
    Step s;
    if (width <= 0 || height <= 0 || gameW <= 0 || gameH <= 0) return s;
    const int x = int(int64_t(std::clamp(targetX, 0, width - 1)) * gameW / width);
    const int y = int(int64_t(std::clamp(targetY, 0, height - 1)) * gameH / height);
    s.dx = x - cursorX;
    s.dy = y - cursorY;
    synced_ = true;  // in the game's pixels: step() resyncs by itself when the window has other ones
    x_ = x;
    y_ = y;
    width_ = gameW;
    height_ = gameH;
    return s;
}

}  // namespace uapad
