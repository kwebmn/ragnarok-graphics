#pragma once
#include <cstdint>
#include <deque>
#include <vector>

#include "core/footsteps.hpp"

// 3.0.5-beta.9 (R-VMPQKX: the footprints 4.1 ms of a Radeon 610M's frame on «Низькі» - steps_ps went through all 64
// steps for every pixel of the screen; the user: «делай 1-3»): the screen in tiles of kTile pixels, each with the steps
// whose reach (steps_ps's: kRise up or down, kReach across) may show in it, worked out on the processor from each
// step's bounds on the screen - a pixel looks at its tile's steps alone, the picture the same as before.
namespace uagfx::step_tiles {

constexpr int kTile = 64;          // pixels a side
constexpr int kMaxColumns = 128;   // the steps' texture's width (footsteps::Tracker::kMax * 2): a texel a tile
constexpr int kMaxRows = 64;       // its rows under the steps' own
constexpr float kReach = 6.3246f;  // sqrt(40): steps_ps's across
constexpr float kRise = 3.0f;      // and up or down

// The pass's view, as its pixel shader turns pixels back (viewFrom: game = (pixel - jitter) / scale)
struct View {
    float viewToWorld[12] = {};  // the view's x, y and z ways in the world, then the camera's place (reproject)
    float f0 = 1, f1 = 1, cx = 0, cy = 0, nearZ = 10;  // camera::Projection's
    float scale = 1, jitterX = 0, jitterY = 0;
    int width = 0, height = 0;  // the pass's target
};

struct Tiles {
    int columns = 0, rows = 0;  // a picture wider or taller than the texture's: its last column or row takes the rest
    std::vector<uint16_t> masks;  // rows x columns x 4 parts - the steps 0-15, 16-31, 32-47, 48-63
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // the steps' bounds together (pixels, x1 and y1 past the last)
    bool empty() const { return x1 <= x0 || y1 <= y0; }  // no step seen
    uint64_t at(int column, int row) const;              // the tile's steps: bit k the step k
};

// The steps in the order steps_ps reads them (the first footsteps::Tracker::kMax)
Tiles build(const std::deque<footsteps::Step>& steps, const View& v);

// steps_ps's t1, both renderers': row 0 two texels a step - its place and way, then its age (0-1, by `now` and its
// `life`) -, the rows under it a texel a tile, its four masks as whole numbers. `out`: (1 + tiles.rows) rows of
// kTexelsPerRow texels of 4 floats, nothing else written; the steps written back.
constexpr int kTexelsPerRow = kMaxColumns;
static_assert(kTexelsPerRow == int(footsteps::Tracker::kMax) * 2, "a row holds the steps' two texels each");
size_t fill(const std::deque<footsteps::Step>& steps, float now, float life, const Tiles& tiles, float* out);

}  // namespace uagfx::step_tiles
