#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::ai {

// What upscaled.grf holds for a texture: found again by the game's own pixels (the same texture in every client with
// the same data), so the cache needs no file names from the game.
struct TextureKey {
    uint64_t hash = 0;  // FNV-1a over width, height and the ARGB values
    int width = 0;
    int height = 0;
};
TextureKey textureKey(const uint32_t* argb, int width, int height);
// The entry's name in upscaled.grf: the model's name, the size and the hash, e.g.
// "ai\realesr-general-x4v3\256x256\0123456789abcdef.tex".
std::string entryName(const std::string& model, const TextureKey& k);
std::string entryFolder(const std::string& model);  // the start of every entryName of the model: "ai\<model>\"

// An upscaled texture as stored: "UATX", version 1, width, height, then width * height ARGB values.
std::vector<uint8_t> encodeTexture(const uint32_t* argb, int width, int height);
bool decodeTexture(const std::vector<uint8_t>& blob, std::vector<uint32_t>& argb, int& width, int& height);

// The network's input for a game texture: planar R, G, B (0..1) and A (0 or 1). Texels the game keyed away (alpha 0,
// its magenta) take their visible neighbours' average first, so the network does not smear magenta into the picture;
// the alpha plane is enlarged on its own (enlargeAlpha) and decides which texels stay transparent.
void prepareInput(const uint32_t* argb, int width, int height, std::vector<float>& planes);
bool hasTransparency(const uint32_t* argb, int width, int height);
// Alpha s times as large: bilinear between texel centres, then cut at one half - smooth edges, still all or nothing
// like the colour key.
void enlargeAlpha(const float* alpha, int width, int height, int scale, std::vector<uint8_t>& out);

}  // namespace uagfx::ai
