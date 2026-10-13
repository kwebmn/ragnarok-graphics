#include "core/scale.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/window_plan.hpp"

namespace uagfx {

ScalePlan planScale(int gameWidth, int gameHeight, int outputWidth, int outputHeight, int renderScale,
                    bool highResolution) {
    ScalePlan p;
    p.picture = fitPicture(gameWidth, gameHeight, outputWidth, outputHeight);
    p.targetWidth = gameWidth;
    p.targetHeight = gameHeight;
    if (!highResolution || p.picture.empty()) return p;
    const bool byWidth = p.picture.width() == outputWidth;  // the axis that limits the picture
    const int64_t game = byWidth ? gameWidth : gameHeight;
    const int64_t out = byWidth ? outputWidth : outputHeight;
    const int percent = std::clamp(renderScale, 100, 200);
    p.s = double(out) / double(game);
    p.r = percent / 100.0;
    p.k = p.s * p.r;
    if (p.k <= 1.0) {  // never below the game's size
        p.k = 1.0;
        return p;
    }
    const double most = double(kMaxTarget) / std::max(gameWidth, gameHeight);
    if (p.k > most) {
        p.k = most;
        p.targetWidth = int(std::lround(gameWidth * p.k));
        p.targetHeight = int(std::lround(gameHeight * p.k));
        return p;
    }
    // size * out / game * percent / 100, rounded half up in whole numbers - as fitPicture rounds the picture.
    const auto times = [&](int64_t size) { return int((size * out * percent + game * 50) / (game * 100)); };
    p.targetWidth = times(gameWidth);
    p.targetHeight = times(gameHeight);
    return p;
}

Rect scaleRect(const Rect& r, double k) {
    const auto edge = [k](int v) { return int(std::ceil(v * k - 1e-6)); };  // 1e-6: k = O / L is not exact
    return Rect{edge(r.left), edge(r.top), edge(r.right), edge(r.bottom)};
}

double sharpCoord(double texel, double magnification) {
    const double base = std::floor(texel - 0.5) + 0.5;  // the centre of the left texel of the bilinear pair
    const double f = texel - base;
    return base + std::clamp((f - 0.5) * std::max(magnification, 1.0) + 0.5, 0.0, 1.0);
}

}  // namespace uagfx
