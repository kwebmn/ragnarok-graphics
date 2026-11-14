#include "core/present_watch.hpp"

namespace uagfx::presentwatch {

bool slow(const Sample& s) {
    return s.presentMs > kSlowMs && (s.gpuMs < 0.0 || s.gpuMs < 0.5 * s.presentMs);
}

Step Watch::feed(const Sample& s, bool d3d12, const std::string& screenCard) {
    if (taken_) return Step::None;
    run_ = slow(s) ? run_ + 1 : 0;
    if (run_ < kRun) return Step::None;
    taken_ = true;  // once a run of the game: the next start shows whether it helped
    if (d3d12) return Step::ToD3d11;
    return screenCard.empty() ? Step::None : Step::ToScreenCard;
}

}  // namespace uagfx::presentwatch
