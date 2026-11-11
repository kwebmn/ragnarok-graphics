#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/pe_exports.hpp"

// 3.0.18-beta.161 (the owner: «делай 1 и 2» - the costs measured before the game's access is redone): the game's
// memory the driver reads on the game's thread. Each read is a call into Windows (ReadProcessMemory on ourselves);
// this counts them, sums their time and keeps the places that asked for them apart - return addresses, told as
// offsets from the driver's module and found in the build's link map afterwards.
namespace uagfx::readtally {

// the places kept apart; the reads of the rest summed as «elsewhere» (3.0.18-beta.163: 1024 - R-MBCDBU's 64 left
// «elsewhere 756» of 929: the reads come from hundreds of places)
constexpr int kPlaces = 1024;

class Tally {
public:
    void add(uint32_t from, double ms);  // one read asked from `from` (0: not known), `ms` its time
    void frame() { ++frames_; }          // a frame shown
    // «game memory: 812 reads a frame (0.35 ms) - the most from ddraw+0x1a2b30 310 (0.12 ms), ...»: a frame's
    // average each, the `top` places that asked the most first; `base` the driver's module. "" without a frame. The
    // tally starts again either way. 3.0.18-beta.163: with the driver's `functions` (sorted, pe::functionsOf) the
    // places are summed by the function they are in, told by its start.
    std::string take(uint32_t base, int top = 6, const std::vector<pe::Function>* functions = nullptr);

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
