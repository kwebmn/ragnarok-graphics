#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace uagfx::itempick {

// 3.0.18-beta.155 (the owner: things picked with Ctrl and moved together, and so everywhere):
// the things picked in a window of things, by their places (the game's item index) - Ctrl+click toggles one,
// Shift+click all from the last clicked to this one in the order shown, Ctrl+A all shown
class Picked {
public:
    void toggle(int index);
    void range(const std::vector<int>& shown, int index);  // the last clicked .. index (both in `shown`)
    void all(const std::vector<int>& shown);
    void keepOnly(const std::vector<int>& there);  // those gone from the window let go
    void clear();
    bool has(int index) const;
    size_t count() const { return picked_.size(); }
    const std::vector<int>& list() const { return picked_; }

private:
    std::vector<int> picked_;
    int last_ = -1;
};

// 3.0.22-beta.7 (a player: Alt+right click moves a thing without dragging; the owner: «делай» - a stack whole, no
// «how many»): a batch move's pace - one thing a step kStepMs apart. A stack's move brings the game's «how many»: it is
// answered «all of it» as soon as it is up - the last one's too: after the last move the steps go on until a «how
// many» is answered or kLastWaitMs pass (a thing that is not a stack brings none). Before, the last one's was left to
// the player.
constexpr uint64_t kStepMs = 250, kLastWaitMs = 1500;
struct Pace {
    uint64_t next = 0;       // the next move's time (0: at once)
    uint64_t lastUntil = 0;  // after the last move: a «how many» answered until then
};
// A frame of a batch at `now` with `left` things still queued: a «how many» shown answered (`answer` true), else the
// next thing moved when it is its time (`move` takes it off the queue). False: no batch runs (nothing asked).
bool step(Pace& p, uint64_t now, size_t left, const std::function<bool()>& answer, const std::function<void()>& move);

}  // namespace uagfx::itempick
