#pragma once
#include <cstdint>
#include <string>

namespace uagfx::presentwatch {

// 3.0.19-beta.8 (R-T8FGMM; the owner: «делай оба шага»): a laptop drew the game on its AMD card while its screen hung
// off its Intel - the swap chain's present took 201 ms every frame, 5 frames a second whatever the effects. The
// presents watched while the game's world is in front: a run of slow ones while the card's own work is light turns the
// drawing over - Direct3D 12 first through the card the screen hangs off (3.0.19-beta.9, at once), then Direct3D 11
// (once a run, from the next start), on Direct3D 11 the screen's card itself.
constexpr double kSlowMs = 100.0;  // a present this long on average over the half second
constexpr int kRun = 20;           // half seconds in a row: 10 s

enum class Step : uint8_t { None, ToD3d11, ToScreenCard, ToCrossPresent };

struct Sample {
    double presentMs = 0.0;
    double gpuMs = -1.0;  // < 0: not measured
    double cpuMs = 0.0;
};

// The half second counts as slow: its present over kSlowMs while the card's work is under half of it (a card that is
// itself too slow makes the present wait for it - that is no fault of the way the frames go)
bool slow(const Sample& s);

class Watch {
public:
    // A new half second's sample, the game's world in front; d3d12: drawn with Direct3D 12 now; crossOn: its frames
    // shown through the screen's card already ([renderer] cross_present); screenCard: the card the game's monitor
    // hangs off when it is another than the one drawing ("": none known) - the step to take
    Step feed(const Sample& s, bool d3d12, bool crossOn, const std::string& screenCard);
    void reset() { run_ = 0; }  // the world left, the window behind: a run starts again
    bool taken() const { return taken_; }
    int run() const { return run_; }

private:
    int run_ = 0;
    bool taken_ = false;
};

}  // namespace uagfx::presentwatch
