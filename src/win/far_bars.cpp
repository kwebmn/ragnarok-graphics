#include "win/far_bars.hpp"

#include <cmath>

#include "core/gauge_scale.hpp"
#include "win/log.hpp"
#include "win/game_camera.hpp"
#include "win/view3d.hpp"

namespace uagfx::win::farbars {
namespace {

bool g_on = true;
int g_told = 0;
float g_toldScale = 1.0f;

}  // namespace

void setOn(bool on) {
    if (on != g_on) LOG_INFO("far bars: [interface] far_bars = %d", on ? 1 : 0);
    g_on = on;
}

float scaleNow() {
    float distance = 0.0f;
    if (!g_on || view3dwin::active() || !readCameraDistance(distance)) return 1.0f;
    const float scale = gaugescale::scaleAt(distance);
    // the first changes told: the bars' size with the camera's distance
    if (g_told < 6 && std::fabs(scale - g_toldScale) >= 0.1f) {
        ++g_told;
        g_toldScale = scale;
        LOG_INFO("far bars: the camera at %.0f - the HP bars at %.0f %% of the game's size", distance, scale * 100.0f);
    }
    return scale;
}

}  // namespace uagfx::win::farbars
