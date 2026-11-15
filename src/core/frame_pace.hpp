#pragma once
#include <cstdint>
#include <map>
#include <string>

// 3.0.19-beta.51 (R-EWK4E4: moc_fild10's 3D view at 56.6 fps under its limit of 60 while the game's thread and the
// renderer each waited some 7 ms of a frame - frames lost in jerks, not to a steady load; the 30-s averages cannot tell
// which frames or why): the long frames counted on each side, the longest kept with its parts, for the log.
namespace uagfx::framepace {

constexpr double kLongMs = 25.0;      // seen as a jerk at 60 fps: one and a half of its 16.7 ms
constexpr double kVeryLongMs = 50.0;  // three of them

// 3.0.19-beta.52 (R-GHP9SJ: on .51 the renderer's long frames had every part of their drawing slower at once - the
// thread kept from the processor, or a view of many more draws?): how much of a span of `spanMs` the thread ran - its
// cycles (QueryThreadCycleTime) against the time stamp counter's ticks over the span, both the counter's; never past
// the span; below 0: unknown
double ranOf(double spanMs, uint64_t threadCycles, uint64_t tscTicks);

// The game's side, a present to the next
struct GameFrame {
    double made = 0.0;      // the game's own frame: from the last present's end to this one's start (our hooks in it)
    double own = 0.0;       // our work at the present, the renderer's present apart
    double renderer = 0.0;  // the renderer's present: the bridge's waits and sends, or Direct3D 11's drawing
    uint32_t reads = 0;     // the game's archive reads on its thread in the frame
    double readMs = 0.0;
    std::string slowest;  // the slowest of them
    double slowestMs = 0.0;
    uint32_t draws = 0;  // 3.0.19-beta.52: the game's draws, clears and writes in the frame
    double ran = -1.0;   // how much of `made` its thread ran (ranOf); below 0 unknown
    double ms() const { return made + own + renderer; }
};

class GameTally {
public:
    void add(const GameFrame& f);
    // "frames' pace, the game's side: 3 of 1698 over 25 ms, 1 over 50; the longest 66.0 ms - the game's own frame 5.0,
    // our present 1.0, the renderer's 60.0; the longest part of the long ones - the game's own frame 2, the renderer's
    // 1, ours 0; archive reads in 1 of them (2, 30.0 ms; the slowest x.spr 25.0 ms)"; "" before a frame
    std::string line() const;
    void clear() { *this = GameTally{}; }
    uint32_t frames() const { return frames_; }
    uint32_t longOnes() const { return long_; }

private:
    uint32_t frames_ = 0, long_ = 0, veryLong_ = 0;
    uint32_t byMade_ = 0, byOwn_ = 0, byRenderer_ = 0;
    uint32_t withReads_ = 0, reads_ = 0;
    double readMs_ = 0.0;
    std::string slowest_;
    double slowestMs_ = 0.0;
    GameFrame longest_;
    // 3.0.19-beta.52: the draws in all and in the long ones; the made time with its share known and what ran of it
    uint64_t draws_ = 0, longDraws_ = 0;
    double ran_[2] = {}, made_[2] = {};  // the others, the long ones
};

// The renderer's side, a present to the next
struct DrawnFrame {
    double ms = 0.0;
    double drawing = 0.0;  // its own work: the game's draws, the passes, the present's before its waits
    double card = 0.0;     // 3.0.19-beta.52: of it waiting for the card (a list's slot)
    double ran = -1.0;     // how much of the drawing its thread ran (ranOf); below 0 unknown
    uint32_t draws = 0;    // the game's draws in it
    double pace = 0.0;     // the frame limit's wait
    double present = 0.0;  // the swap chain's present and Reflex's wait
    const char* part = nullptr;  // the dearest part of its drawing in the frame
    double partMs = 0.0;
    // the rest: waiting for the game's next frame (and reading it)
    double waiting() const {
        const double left = ms - drawing - pace - present;
        return left > 0.0 ? left : 0.0;
    }
};

class DrawnTally {
public:
    void add(const DrawnFrame& f);
    // "frames' pace, the renderer's side: 2 of 52 over 25 ms, 1 over 50; the longest 70.0 ms - waiting for the game's
    // frame 61.5, its drawing 8.0, the frame limit's wait 0.0, the present 0.5; the longest part of the long ones -
    // waiting for the game's frame 1, its drawing 1, the frame limit's wait 0, the present 0; the dearest of the
    // drawing in them - the own ground 1 (20.0 ms at most)"; "" before a frame
    std::string line() const;
    void clear() { *this = DrawnTally{}; }
    uint32_t frames() const { return frames_; }
    uint32_t longOnes() const { return long_; }

private:
    struct Dearest {
        uint32_t times = 0;
        double most = 0.0;
    };
    uint32_t frames_ = 0, long_ = 0, veryLong_ = 0;
    uint32_t by_[4] = {};  // waiting, drawing, pace, present
    std::map<std::string, Dearest> dearest_;
    DrawnFrame longest_;
    // 3.0.19-beta.52: the draws in all and in the long ones; the drawing with its share known (the card's waits
    // apart) and what ran of it - the others, the long ones; the card's waits in the long ones
    uint64_t draws_ = 0, longDraws_ = 0;
    double ran_[2] = {}, worked_[2] = {};
    double longCard_ = 0.0;
};

}  // namespace uagfx::framepace
