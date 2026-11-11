#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// 3.0.18-beta.169 (R-45Z47V: the driver's own work at a present grew from 2.1 to 4.7 ms in five minutes of
// hunting, the game's memory read cheaply all along): each part of a present (win/context: the panels' and windows'
// frames, their draws, uaUI's) timed from the mark before it to its own, summed by its name, told every 30 s - a
// frame's average each, the dearest first.
namespace uagfx::partclock {

class Clock {
public:
    void begin(double nowMs);                        // a present begins: its parts measured from here
    void mark(std::string_view name, double nowMs);  // the part `name` ended now (a literal: kept as it is)
    void frame() { ++frames_; }                      // a present done
    // «the driver's parts: 2.31 ms a frame in 54 - chat 0.80, hotbar 0.31, ..., the rest 0.40»; "" without a frame.
    // The tally starts again either way.
    std::string take(int top = 10);

private:
    std::unordered_map<std::string_view, double> ms_;
    double last_ = -1;
    uint32_t frames_ = 0;
};

}  // namespace uagfx::partclock
