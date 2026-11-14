#include "core/present_watch.hpp"

namespace uagfx::presentwatch {

bool slow(const Sample& s) {
    return s.presentMs > kSlowMs && (s.gpuMs < 0.0 || s.gpuMs < 0.5 * s.presentMs);
}

Step Watch::feed(const Sample& s, bool d3d12, bool crossOn, const std::string& screenCard) {
    if (taken_) return Step::None;
    run_ = slow(s) ? run_ + 1 : 0;
    if (run_ < kRun) return Step::None;
    if (d3d12 && !crossOn && !screenCard.empty()) {  // at once, no restart: watched again with it on
        run_ = 0;
        return Step::ToCrossPresent;
    }
    taken_ = true;  // once a run of the game: the next start shows whether it helped
    if (d3d12) return Step::ToD3d11;
    return screenCard.empty() ? Step::None : Step::ToScreenCard;
}

}  // namespace uagfx::presentwatch
