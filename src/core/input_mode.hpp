#pragma once
#include <cstdint>

namespace uagfx::inputmode {

// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): what the player plays with now. The interface
// shows the controller's buttons after an intentional pad input - a press, a stick past half its way (a drifting stick
// does not count) - and the keys' names again after a click, a key or a mouse move past 8 px; the shown one changes at
// most once per 1.5 s, so the hints never blink between the two.

bool intentional(uint16_t down, float lx, float ly, float rx, float ry);
bool mouseMoved(int dx, int dy);

class Mode {
public:
    void pad(uint64_t nowMs) { padAt_ = nowMs; }
    void mouse(uint64_t nowMs) { mouseAt_ = nowMs; }
    bool padShown(uint64_t nowMs);  // the controller's buttons on the interface now

private:
    uint64_t padAt_ = 0, mouseAt_ = 0, changedAt_ = 0;
    bool shown_ = false, changed_ = false;
};

}  // namespace uagfx::inputmode
