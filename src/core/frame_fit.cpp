#include "core/frame_fit.hpp"

namespace uagfx::frame_fit {

int gameFps(int refreshHz, int shownPerGameFrame) {
    if (refreshHz <= 1 || shownPerGameFrame <= 1) return 0;
    const double shown = refreshHz - double(refreshHz) * refreshHz / 3600.0;
    const int fps = int(shown / shownPerGameFrame);
    return fps > 0 ? fps : 1;
}

int pacedFps(int fpsLimit, int refreshHz, int shownPerGameFrame) {
    const int fit = gameFps(refreshHz, shownPerGameFrame);
    if (fit <= 0) return fpsLimit;
    return fpsLimit > 0 && fpsLimit < fit ? fpsLimit : fit;
}

}  // namespace uagfx::frame_fit
