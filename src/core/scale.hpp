#pragma once
#include "core/config.hpp"
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

// Interface draws at k > 1 (stage 3 spec §6, review C1): target pixel p belongs to game pixel floor(p / k) on each
// axis - exactly the pixels a quad with game edges [x0, x1) covers, [ceil(x0 k), ceil(x1 k)) - and shows the game
// pixels it overlaps, each with the texture value Direct3D 7 sampled there. So a picture never reaches past its quad
// (no seams, no edge lines) and pictures the game stretches keep their smooth look. sharp: k x k blocks, with a
// one-pixel area blend where a game pixel edge crosses the target pixel; smooth: bilinear between game pixel centres,
// as Windows stretched the whole frame in 0.2. first, last: the quad's game pixels on this axis. The pixel shader's
// uiTaps() does the same.
struct UiTaps {
    int g0 = 0;      // the game pixels blended
    int g1 = 0;
    double w = 0.0;  // weight of g1
};
UiTaps uiTaps(int p, double k, int first, int last, bool sharp);

// The interface filter for a draw: 0 = none (plain sampling, as 0.2), 1 = sharp, 2 = smooth. Only interface draws, and
// only above k = 1 (review I2: at k = 1 the picture is exactly 0.2's).
int uiFilterMode(bool ui, double k, UiFilter filter);

}  // namespace uagfx
