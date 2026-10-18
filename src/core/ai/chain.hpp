#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/atlas_tiles.hpp"

namespace uagfx::ai {

// 2.3, uaRO Studio (T5): a texture's picture made by a chain of steps about the network - a JPEG's blocks and its
// colour key repaired before it; after it the tone matched to the original's, the original's grain given back,
// sharpened, the alpha cleaned; two models mixed by the original's edges, or the original mixed in. The steps run on
// the processor; the network is the engine's (Enlarge). Plain data: the same chain is a pack's '+' line and a rule.
struct Chain {
    bool deblock = false, repairKey = false;  // before the network
    std::string model = "general";
    int scale = 4;     // 4, or 2: the network's picture halved
    int denoise = -1;  // the general model's denoise (0-100); -1 the settings'
    bool tiles = false;  // an atlas's tiles each on its own (core/atlas_tiles.hpp), when the catalog found a grid
    bool matchTone = false;
    float grain = 0, sharpen = 0;  // 0..1, 0 off
    bool cleanAlpha = false;
    std::string blendModel;  // "" none: a second model's picture where the original has edges
    float original = 0;      // the original (bicubic) mixed in, 0..1
};
// "general x4", then the steps that are on in this order: "deblock key tiles tone grain=0.30 sharpen=0.40 alpha
// blend=anime original=0.20 denoise=50".
std::string chainText(const Chain& c);
bool parseChain(const std::string& text, Chain& out);  // false: an unknown word or scale

// The steps. A picture is W x H, its original w x h (any ratio); texels of alpha below 128 are the colour key's.
void deblock(std::vector<uint32_t>& argb, int w, int h);  // 8x8 block edges eased where both sides are flat
void repairKey(std::vector<uint32_t>& argb, int w, int h);  // near magenta (a JPEG's) made the key
void matchTone(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h);  // OKLab mean, spread
void restoreGrain(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h, float strength);
void sharpenAdaptive(std::vector<uint32_t>& pic, int W, int H, float amount);  // contrast adaptive (core/sharpen)
// The original's colour key enlarged (enlargeAlpha) as the picture's alpha; the texels beside it gone magenta take
// the original's colour.
void cleanAlpha(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h);
// other where the original has edges (its Sobel), pic elsewhere
void blendByEdges(std::vector<uint32_t>& pic, const std::vector<uint32_t>& other, int W, int H, const uint32_t* orig,
                  int w, int h);
void mixOriginal(std::vector<uint32_t>& pic, int W, int H, const uint32_t* orig, int w, int h, float amount);

// The network: argb (w x h) 4 times as large by `model`.
using Enlarge = std::function<bool(const std::string& model, int denoise, const std::vector<uint32_t>& argb, int w,
                                   int h, std::vector<uint32_t>& out, std::string& error)>;
// The whole chain on a texture: out is W x H (4 or 2 times its size). false: the network failed (error says why).
bool runChain(const Chain& c, const uint32_t* argb, int w, int h, atlas_tiles::Grid grid, const Enlarge& enlarge,
              std::vector<uint32_t>& out, int& W, int& H, std::string& error);

}  // namespace uagfx::ai
