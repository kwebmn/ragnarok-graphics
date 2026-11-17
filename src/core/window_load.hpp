#pragma once
#include <deque>
#include <string>

namespace uagfx::windowload {

// 3.0.21-beta.8 (R-SPPSF3 «когда открываю Alt-O начинает сильно гудеть на видеокарте вентилятор»): what an open
// window costs, told when it closes - the frames a second, the card's work a frame and the driver's own work at a
// present while it was open against the five seconds of the game alone before it opened. The card's share of its
// time is its work a frame times the frames a second: what its fan answers to. No Windows here.
class Watch {
public:
    // A frame at `now` (seconds): the window open or not, the card's work of the newest measured frame in
    // milliseconds (< 0: not measured), the driver's own work at this present in milliseconds. True: the window has
    // just closed after two seconds or more - told() says it
    bool frame(double now, bool open, double gpuMs, double ownMs);
    // «open 10.0 s - 100 frames a second, the card 6.0 ms a frame (60 % of its time), the driver's own work 3.75 ms a
    // frame; in the 5 s before it 200 frames a second, the card 4.0 ms a frame (80 %), the driver's own work 1.25 ms
    // a frame»
    const std::string& told() const { return told_; }

private:
    struct Frame {
        double took = 0, gpuMs = -1, ownMs = 0;  // the time since the frame before; the card's work; the driver's
    };
    struct Sum {
        double seconds = 0, gpuMs = 0, ownMs = 0;
        int frames = 0, gpuFrames = 0;
        void add(const Frame& f);
        void take(const Frame& f);
        std::string say(bool first) const;
    };
    std::deque<Frame> alone_;  // the last frames of the game alone, five seconds of them
    Sum before_, open_, shut_;  // those five seconds as the window opened; the window's own; the ring's sum
    double last_ = -1;
    bool wasOpen_ = false;
    std::string told_;
};

}  // namespace uagfx::windowload
