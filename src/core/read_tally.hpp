#pragma once
#include <cstdint>
#include <string>

// 3.0.18-beta.161 (the owner: «делай 1 и 2» - the costs measured before the game's access is redone): the game's
// memory the driver reads on the game's thread. Each read is a call into Windows (ReadProcessMemory on ourselves);
// this counts them, sums their time and keeps the places that asked for them apart - return addresses, told as
// offsets from the driver's module and found in the build's link map afterwards.
namespace uagfx::readtally {

constexpr int kPlaces = 64;  // the places kept apart; the reads of the rest summed as «elsewhere»

class Tally {
public:
    void add(uint32_t from, double ms);  // one read asked from `from` (0: not known), `ms` its time
    void frame() { ++frames_; }          // a frame shown
    // «game memory: 812 reads a frame (0.35 ms) - the most from ddraw+0x1a2b30 310 (0.12 ms), ...»: a frame's
    // average each, the `top` places that asked the most first; `base` the driver's module. "" without a frame. The
    // tally starts again either way.
    std::string take(uint32_t base, int top = 6);

private:
    struct Place {
        uint32_t at = 0;
        uint32_t reads = 0;
        double ms = 0;
    };
    Place places_[kPlaces];
    uint32_t elsewhere_ = 0, unknown_ = 0, frames_ = 0;
    uint64_t reads_ = 0;
    double ms_ = 0;
};

}  // namespace uagfx::readtally
