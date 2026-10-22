#include "pad/core/logic/vbutton.hpp"

#include <algorithm>

namespace uapad {

bool VirtualButton::set(bool down, uint32_t nowMs) {
    if (down == reported_) return false;
    reported_ = down;
    queue_.push_back(Event{down, nowMs});
    while (queue_.size() > kMaxQueued) {  // nobody reads the buffer: drop the oldest press/release pair
        queue_.pop_front();
        queue_.pop_front();
    }
    return true;
}

std::vector<VirtualButton::Event> VirtualButton::take(size_t room, bool peek) {
    const size_t n = std::min(room, queue_.size());
    std::vector<Event> out(queue_.begin(), queue_.begin() + long(n));
    if (!peek) queue_.erase(queue_.begin(), queue_.begin() + long(n));
    return out;
}

void VirtualButton::flush() {
    if (queue_.empty()) return;
    reported_ = !queue_.front().down;
    queue_.clear();
}

}  // namespace uapad
