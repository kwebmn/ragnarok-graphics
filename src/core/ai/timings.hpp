#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::ai {

// 3.0.2-beta.7 (the client build session: the game waited 12-15 s for the AI textures' start, a sprite frame up to
// 79 ms for its picture): where the start and the pack's pictures spend their time, for the log - milliseconds, one
// decimal under 10.

// A start's steps in their order, a step's parts in its brackets: "shaders 9512 ms (ai_conv 8650, ai_mip 3.2), the
// pack opened 312 ms".
class Steps {
public:
    void add(const std::string& name, double ms);
    void addPart(const std::string& name, double ms);  // of the last step
    std::string line() const;                          // "" when none
    double total() const;                              // the steps'

private:
    struct Step {
        std::string name;
        double ms = 0.0;
        std::vector<std::pair<std::string, double>> parts;
    };
    std::vector<Step> steps_;
};

// Where a picture of the pack spends its time from its request to the frame that takes it: in the I/O thread's queue,
// read from the pack, decoded (WebP), fitted (halved, its key, its dye), then until the next frame looks.
enum class LoadPart : uint8_t { Queue, Read, Decode, Fit, Frame };
constexpr int kLoadParts = 5;

class LoadTimes {
public:
    void add(const double (&ms)[kLoadParts]);
    uint32_t count() const { return count_; }
    // "412: in the queue 0.4 (12), read 0.3 (2.1), WebP 1.2 (9.8), fitted 0.1 (0.6), until its frame 7.9 (16), in
    // all 9.9 (31)" - each part's average (most); "" when none
    std::string line() const;

private:
    uint32_t count_ = 0;
    double sum_[kLoadParts + 1] = {};  // the parts, then the whole
    double most_[kLoadParts + 1] = {};
};

}  // namespace uagfx::ai
