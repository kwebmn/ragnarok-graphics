#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ai/kinds.hpp"

namespace uagfx::ai {

// What upscaled.grf holds for a texture: found again by the game's own pixels (the same texture in every client with
// the same data), so the cache needs no file names from the game.
struct TextureKey {
    uint64_t hash = 0;  // FNV-1a over width, height and the ARGB values
    int width = 0;
    int height = 0;
};
TextureKey textureKey(const uint32_t* argb, int width, int height);
// The game texture's part of every name below: its size and hash, "256x256\0123456789abcdef".
std::string keyName(const TextureKey& k);
// The entry's name in upscaled.grf: the folder of the network's settings (netFolder), the size and the hash, e.g.
// "ai\realesr-general-x4v3-dn25-wrap\256x256\0123456789abcdef.tex".
std::string entryName(const std::string& folder, const TextureKey& k);
std::string entryName(const std::string& folder, const std::string& key);
std::string entryFolder(const std::string& folder);  // the start of every entryName of the folder: "ai\<folder>\"
// The folder of a network's pictures: the model ("general" with its denoise strength, "anime"), what it reads past a
// texture's edge (wrapped since 0.11.2) and the result's size, "-x2" for twice the game's instead of 4 times:
// "realesr-general-x4v3-dn25-wrap", "realesr-animevideov3-wrap-x2".
std::string netFolder(const std::string& model, int denoise, int scale);
// 0.12: what a texture was drawn as and the maps it was seen on, as empty entries next to the pictures - the settings
// app shows and deletes the pictures by map and kind with them, whatever settings made them:
// "ai\@kind\ground\256x256\0123456789abcdef", "ai\@map\prontera\256x256\0123456789abcdef".
std::string kindTag(Kind kind, const std::string& key);
std::string mapTag(const std::string& map, const std::string& key);
struct ParsedEntry {
    enum Type { Other, Picture, KindTag, MapTag } type = Other;
    std::string folder;  // Picture
    std::string key;     // "256x256\0123456789abcdef"
    Kind kind = Kind::None;  // KindTag
    std::string map;     // MapTag
};
ParsedEntry parseEntry(const std::string& name);
// The map the game loads, as it names it ("prontera.rsw", "PRT_FILD08.RSW", "1@tower.rsw") without its extension and
// in lower case; "" when the bytes are no map name (a stale pointer).
std::string mapName(const char* raw, size_t size);

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
