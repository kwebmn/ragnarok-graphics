#pragma once
#include <map>
#include <string>
#include <vector>

// 3.0.5-beta.2 (the user: «давай сделаем анализ, где именно идет больше всего расчетов, что вызывают "тормоза"»):
// [debug] frame_profile - each frame cut into spans by the marks the renderers put down (the game's world, its
// interface, each pass, the clears, the world made larger, the picture into the window): the card's time from a mark
// to the next by its time stamps, the processor's by its clock; summed by name over a window and written to the log,
// a frame's average each, the dearest first.
namespace uagfx::frame_profile {

constexpr double kWindowMs = 10000.0;  // the log's lines every 10 seconds

class Window {
public:
    // a span of a frame: its name, the card's ms (below 0: not timed by it) and the processor's
    void span(const std::string& name, double gpuMs, double cpuMs);
    // a part of the processor's work counted apart - inside the spans (Direct3D's calls) or outside them (the waits)
    void cpu(const std::string& name, double ms);
    void frame(double ms);  // a frame shown: the time since the last one
    int frames() const { return int(frameMs_.size()); }
    // the log's lines (none without a frame): the frames and their pace; the card's spans; the processor's; its parts
    // counted apart (when there are any) - `who` the renderer, `seconds` the window's length
    std::vector<std::string> lines(const std::string& who, double seconds) const;
    void clear();

private:
    struct Sum {
        double gpu = 0.0, cpu = 0.0;
        bool timed = false;  // the card timed it at least once
    };
    std::map<std::string, Sum> spans_;
    std::map<std::string, double> apart_;
    std::vector<double> frameMs_;
};

}  // namespace uagfx::frame_profile
