#pragma once
#include "core/image.hpp"

namespace uagfx {

// Stage 3 (docs/superpowers/specs/2026-10-13-stage3-resolution-design.md): the game keeps drawing at its own size L;
// the driver draws k times larger and shows the result in the window's real pixels.
struct ScalePlan {
    double s = 1.0;       // output / game, along the axis that limits the picture
    double r = 1.0;       // render scale (supersampling)
    double k = 1.0;       // game pixels -> target pixels
    int targetWidth = 0;  // the target and its depth buffer: the game's size times k, rounded
    int targetHeight = 0;
    Rect picture;         // where the whole frame goes in the output
};

constexpr int kMaxTarget = 16384;  // the largest Direct3D 12 texture side

// game: the client's resolution; output: the window's client area (real pixels when highResolution is on);
// renderScale: percent, 100-200. Never below the game's size: a smaller window shrinks a full-size frame. With
// highResolution off the frame is drawn at the game's size (k = 1), as in version 0.2. The target is size * k rounded
// in whole numbers, as fitPicture rounds: at 100 % it is exactly the picture, shown pixel for pixel.
ScalePlan planScale(int gameWidth, int gameHeight, int outputWidth, int outputHeight, int renderScale,
                    bool highResolution);

// Game pixels [x0, x1) in the target: [ceil(x0 k), ceil(x1 k)) - the pixels a quad with those edges covers once
// its vertices sit at x k + 0.5 (D3D12 pixel centres are on half-integers).
Rect scaleRect(const Rect& r, double k);

// Where a vertex at game coordinate x lands in the target. Not (x + 0.5) k: that shifts the picture by half a game
// pixel and leaves the first column and row uncovered (stage 3 spec §4). At k = 1 both are calibration.md §3.
inline double targetCoord(double x, double k) { return x * k + 0.5; }

// "Sharp bilinear" for interface pictures: a texel coordinate (uv * size) remapped so every texel is a flat square
// with a linear step one screen pixel wide between neighbours. magnification: screen pixels per texel; at 1 or less
// the coordinate is unchanged. The pixel shader's sharpUv() does the same.
double sharpCoord(double texel, double magnification);

}  // namespace uagfx
