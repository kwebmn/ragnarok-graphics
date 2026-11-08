#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace uagfx::afterimage {

// 3.0.16-beta.13 (the owner's pick 15 of «20 идей»: afterimages and body tints): the character's own path (the
// camera's target in the world, each frame) - moving faster than a plain walk (150 ms a cell of 5 units: about 33 a
// second; Increase AGI, Snap and Body Relocation, a Peco) its sprite drawn again a few times behind it along the
// path, fading; a while after it slows the trail goes out
constexpr float kFastUnits = 40.0f;   // a second
constexpr uint64_t kStepMs = 60;      // between the ghosts along the path
constexpr int kGhosts = 3;
constexpr uint64_t kTailMs = 250;     // the trail's going out after slowing
struct Ghost {
    std::array<float, 3> at{};
    float alpha = 0;
};
class Trail {
public:
    void note(const float at[3], uint64_t ms);
    float speed(uint64_t ms) const;           // units a second over the last 200 ms
    std::vector<Ghost> ghosts(uint64_t ms);   // the places behind and their strength (none: empty)
    void clear() { samples_.clear(); }

private:
    struct Sample {
        uint64_t ms;
        std::array<float, 3> at;
    };
    std::array<float, 3> at(uint64_t ms) const;  // the path's place then (interpolated)
    std::deque<Sample> samples_;
    uint64_t fastUntil_ = 0;
};

// The body's tint (added over the sprite): Fury a warm red, Steel Body cool steel; 0 strength none
struct Tint {
    uint8_t r = 0, g = 0, b = 0;
    float strength = 0;
};
Tint tintOf(bool fury, bool steelBody);

}  // namespace uagfx::afterimage
