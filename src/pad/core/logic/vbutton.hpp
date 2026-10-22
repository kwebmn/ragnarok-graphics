#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace uapad {

// The virtual left mouse button as a client reading buffered DirectInput sees it: edges are queued
// until the client reads them, and `reported()` is what the client will believe once it has.
class VirtualButton {
public:
    struct Event {
        bool down = false;
        uint32_t timeMs = 0;
    };

    // Desired state for this frame. Queues an edge and returns true when it differs from what was reported.
    bool set(bool down, uint32_t nowMs);
    // Queues a release if the client believes the button is held (used when the module fails).
    bool release(uint32_t nowMs) { return reported_ && set(false, nowMs); }
    bool reported() const { return reported_; }
    size_t pending() const { return queue_.size(); }
    // Hands out up to `room` queued events; with `peek` they stay queued (DIGDD_PEEK).
    std::vector<Event> take(size_t room, bool peek);
    // The client flushed its buffer: unread events are gone, so it still believes the state before them.
    void flush();

private:
    static constexpr size_t kMaxQueued = 16;

    bool reported_ = false;
    std::deque<Event> queue_;
};

}  // namespace uapad
