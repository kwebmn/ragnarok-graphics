#pragma once

// 3.0.20-beta.2 (R-JJE8HA: the tester's «Склад» on the release 3.0.19 - the icons of hundreds of things read one after
// another on the game's thread, each from the game's archive and the AI texture pack, from a slow disk: the game still
// for 15 s): a share of time a window (about a frame) for such first reads; past it the rest wait for the next window
// (drawn without their picture until then)
namespace uagfx::readbudget {

class Budget {
public:
    Budget(double shareMs, double windowMs) : share_(shareMs), window_(windowMs) {}
    // whether a read may go now (`nowMs` any running clock in ms): a new window - its share whole again
    bool allowed(double nowMs) {
        if (nowMs - start_ >= window_ || nowMs < start_) {
            start_ = nowMs;
            spent_ = 0.0;
        }
        return spent_ < share_;
    }
    void spend(double ms) { spent_ += ms; }

private:
    double share_, window_;
    double start_ = -1e300, spent_ = 0.0;
};

}  // namespace uagfx::readbudget
