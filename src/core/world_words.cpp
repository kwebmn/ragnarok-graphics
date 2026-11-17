#include "core/world_words.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::worldwords {
namespace {

int ftol(float v) { return std::isfinite(v) ? int(v) : 0; }  // the game's __ftol: toward zero

}  // namespace

Point wordsAt(float fx, float fy, float scale, int w, int h, Screen s) {
    const int sx = int(std::lround(fx)), sy = int(std::lround(fy));
    const float over = float(s.height * 81 / 480);  // FUN_0054ac00's 0x76D2AC
    Point p{sx - w / 2, ftol(float(sy) - over * scale - float(h))};
    if (p.x < -3) p.x = -3;
    p.x = std::min(p.x, s.width - w + 3);
    if (p.y < -3) p.y = -3;
    p.y = std::min(p.y, s.height - h + 3);
    return p;
}

Point nameAt(int mode, float fx, float fy, float scale, int w, int h, Screen s) {
    const int sx = int(std::lround(fx)), sy = int(std::lround(fy));
    int dx = -(w / 2), dy = 0;
    switch (mode) {
        case 1:  // to the left
            dx = ftol(float(-30 * s.width / 640) * scale - float(w / 2));
            dy = ftol(float(-25 * s.height / 480) * scale - float(h / 2));
            break;
        case 2:  // to the right
            dx = ftol(float(30 * s.width / 640) * scale - float(w / 2));
            dy = ftol(float(-25 * s.height / 480) * scale - float(h / 2));
            break;
        case 3:  // over the head
            dy = ftol(float(-100 * s.height / 480) * scale);
            break;
        default:  // under the feet
            dy = ftol(float(20 * s.height / 480) * scale);
            break;
    }
    Point p{sx + dx, sy + dy};
    if (p.x < -2) p.x = -3;
    p.x = std::min(p.x, s.width - w + 3);
    if (p.y < -2) p.y = -3;
    p.y = std::min(p.y, s.height - h + 3);
    return p;
}

float scaleOf(float rhw, float f0, float k, float norm) {
    if (!(norm > 0.0f) || !std::isfinite(rhw) || !std::isfinite(f0) || !std::isfinite(k)) return 0.0f;
    return rhw * f0 * k / norm;
}

}  // namespace uagfx::worldwords
