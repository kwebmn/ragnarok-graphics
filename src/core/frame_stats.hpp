#pragma once
#include <cstdint>
#include <deque>
#include <vector>

// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 1): the game's presents of the last
// minute for the MCP's perf - each frame's length (from the present before), by a clock in microseconds (the
// performance counter: GetTickCount's 16 ms would hide a frame's length)
namespace uagfx::framestats {

constexpr uint64_t kKeepUs = 60000000;  // a minute kept
constexpr double kPauseMs = 2000;       // a gap this long: the game behind or paused - no frame, no stall

struct Long {
    double ago = 0, ms = 0;  // seconds before now; its length
};
struct Stats {
    int frames = 0;
    double seconds = 0, fps = 0;
    double p50 = 0, p95 = 0, p99 = 0, worst = 0;  // ms
    int over25 = 0, over50 = 0, over100 = 0;
    std::vector<Long> longest;  // the 5 longest, the longest first
};

class Window {
public:
    void present(uint64_t nowUs);
    Stats stats(uint64_t nowUs, double seconds) const;

private:
    struct Frame {
        uint64_t at = 0;
        float ms = 0;
    };
    std::deque<Frame> frames_;
    uint64_t last_ = 0;
};

}  // namespace uagfx::framestats
