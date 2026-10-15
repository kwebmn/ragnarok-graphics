#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ai/kinds.hpp"

namespace uagfx::ai {

// A game texture's key - the texture pack names its pictures by it: found again by the game's own pixels (the same
// texture in every client with the same data), so the pack needs no file names from the game.
struct TextureKey {
    uint64_t hash = 0;  // FNV-1a over width, height and the ARGB values
    int width = 0;
    int height = 0;
};
TextureKey textureKey(const uint32_t* argb, int width, int height);
// The game texture's part of every name below: its size and hash, "256x256\0123456789abcdef".
std::string keyName(const TextureKey& k);
// The map the game loads, as it names it ("prontera.rsw", "PRT_FILD08.RSW", "1@tower.rsw") without its extension and
// in lower case; "" when the bytes are no map name (a stale pointer).
std::string mapName(const char* raw, size_t size);

// 0.42: half the size, each pixel the average of four (premultiplied: a transparent one lends no colour) - the
// texture pack's pictures at [ai] scale = 2.
void halve(std::vector<uint32_t>& argb, int& width, int& height);

// The network's input for a game texture: planar R, G, B (0..1) and A (0 or 1). Texels the game keyed away (alpha 0,
// its magenta) take their visible neighbours' average first, so the network does not smear magenta into the picture;
// the alpha plane is enlarged on its own (enlargeAlpha) and decides which texels stay transparent.
void prepareInput(const uint32_t* argb, int width, int height, std::vector<float>& planes);
bool hasTransparency(const uint32_t* argb, int width, int height);
// 0.22.2: a picture of soft light alone - glows, halos, gradients: under 2 % of its texels by an edge or a grain
// (the premultiplied colour's Laplacian). The network adds nothing to such a picture and turns its gradients into
// bands (the drawn model most), so it is drawn as the game's, filtered.
bool softPicture(const uint32_t* argb, int width, int height);
// 0.39.2: a picture whose shape is in a soft alpha - more than 5 % of its texels part-transparent (16-239): the
// chimneys' smoke, a skill's glowing particle. The network's alpha is all or nothing (made for the colour key), so it
// stays the game's: smoke became a hard white blob, Soul Strike's particles small squares.
bool softAlpha(const uint32_t* argb, int width, int height);
// Alpha s times as large: bilinear between texel centres, then cut at one half - smooth edges, still all or nothing
// like the colour key.
void enlargeAlpha(const float* alpha, int width, int height, int scale, std::vector<uint8_t>& out);

// 0.13, a cell of the game's sprite cache until the network has done it: the colour bilinear between texel centres
// (hidden texels take their visible neighbours' colour first, as for the network), the alpha as enlargeAlpha - the
// look of the game's own texture drawn larger.
void enlargeCell(const uint32_t* argb, int width, int height, int scale, std::vector<uint32_t>& out);
// 0.16: the same cell exactly as the game has it, every texel scale x scale: what the enlarged copy of the sprite
// cache holds for a frame the network does not take (a kind that is off), and until it is done.
void enlargeNearest(const uint32_t* argb, int width, int height, int scale, std::vector<uint32_t>& out);
// The next mip level of an ARGB picture, as the GPU's ai_mip makes it: the average of up to four texels, the alpha all
// or nothing.
void halve(const std::vector<uint32_t>& argb, int width, int height, std::vector<uint32_t>& out, int& outWidth,
           int& outHeight);

}  // namespace uagfx::ai
