#pragma once
#include <cstdint>

namespace uagfx {

// A value that changes on every frame while something is dragged - the window's scale while its edge is - is taken
// only once it has stayed the same for settleMs; the first value at once (0.5.0 review M2: the twins were rebuilt and
// a log line written on every frame of a resize).
class Settled {
public:
    explicit Settled(uint64_t settleMs) : settle_(settleMs) {}
    // The value as seen at now: true when it is to be taken now.
    bool take(double value, uint64_t now) {
        if (!has_) {
            has_ = true;
            taken_ = value;
            return true;
        }
        if (value == taken_) {
            pending_ = false;
            return false;
        }
        if (!pending_ || value != candidate_) {
            pending_ = true;
            candidate_ = value;
            since_ = now;
        }
        if (now - since_ < settle_) return false;
        taken_ = value;
        pending_ = false;
        return true;
    }
    double taken() const { return taken_; }

private:
    uint64_t settle_;
    bool has_ = false;
    double taken_ = 0.0;
    bool pending_ = false;
    double candidate_ = 0.0;
    uint64_t since_ = 0;
};

}  // namespace uagfx
