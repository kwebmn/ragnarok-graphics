#pragma once
#include <cstdint>

namespace uagfx::ai {

// 2.3, uaRO Studio (T3): numbers of what a network did to a game texture - its picture (pw x ph, any size: x4, x2, a
// lower detail's) against the original (w x h) - and the flags they raise in the studio's list. The thresholds are
// starting points, set later on real pictures.
struct Metrics {
    float detail = 1;   // detail kept: the Laplacian energy of the picture shrunk to the original's size / its own
    float tone = 0;     // the mean colour's OKLab distance
    float banding = 0;  // of the steps between flat runs in gentle gradients, the share two levels or more apart
    float ringing = 0;  // the mean overshoot past the original's range beside its edges (0..1)
    float fringe = 0;   // the share of the texels beside the colour key gone magenta
    float alpha = 0;    // the share of texels whose shrunk alpha is not the original's
    float seams = 0;    // the difference across the wrap against the one inside (world textures; 0 others)
};
enum Flag : uint16_t { Smoothed = 1, Washed = 2, Banded = 4, Ringing = 8, Fringe = 16, AlphaSpoiled = 32, Seams = 64 };
constexpr int kFlags = 7;

// wraps: a texture of the world (its edges repeat on the map) - its seams measured.
Metrics measure(const uint32_t* original, int w, int h, const uint32_t* picture, int pw, int ph, bool wraps);
// detail < 0.45, tone > 0.04, banding > 0.15, ringing > 0.08, fringe > 0.05, alpha > 0.03, seams > 2.0
uint16_t flagsOf(const Metrics& m);
const char* flagName(Flag f);  // "smoothed", "washed", "banded", "ringing", "fringe", "alpha", "seams"

// OKLab of an sRGB colour (its alpha left out), and back - the chain's tone match (core/ai/chain.hpp) too.
void oklabOf(uint32_t argb, float lab[3]);
uint32_t argbOfOklab(const float lab[3], uint32_t alpha);

}  // namespace uagfx::ai
