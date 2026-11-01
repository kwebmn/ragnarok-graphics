#include "core/map_entry.hpp"

#include "core/login_exe.hpp"

namespace uagfx::map_entry {

Step Guard::frame(const Seen& s, uint64_t now, bool patient) {
    if (s.loginMode && s.state == login_exe::kStateEntering) {
        if (!entering_) {
            entering_ = true;
            since_ = now;
            came_ = false;
            reconnects_ = 0;
        }
        came_ = came_ || s.connected;
        if (s.failed && patient && reconnects_ < kMaxReconnects) {
            ++reconnects_;
            return Step::Reconnect;
        }
        return Step::None;
    }
    if (entering_) {
        entering_ = false;
        // still on the login screens: the entry failed and its node lingers; else on the map
        holdUntil_ = s.loginMode ? since_ + (came_ ? kNodeOfLeftClientGoneMs : kNodeGoneMs) : 0;
    }
    return Step::None;
}

}  // namespace uagfx::map_entry
