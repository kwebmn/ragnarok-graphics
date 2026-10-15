#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/texture_pack.hpp"

namespace uagfx::pack {

// 0.43: the game's own files made into the pixels the driver sees in the game, so that a picture the pack builder
// makes of them has the key (ai::textureKey) the driver looks it up by (uaRO.exe as of 2007-05-07a):
// - a world texture (BMP, TGA): its pixels into the texture's 16-bit format (A1R5G5B5; A4R4G4B4 with alpha) as the
//   game converts them (FUN_004176c0: magenta transparent), then back to 8-bit channels as the driver does (toArgb8);
//   a whole texture of a power of two only (the game rounds a texture up: the driver takes no part of one);
// - an interface picture (BMP): as crisp text sees it copied into a window (5-bit magenta transparent);
// - a sprite's palette frames: each in the sprite cache's cell of its size (32-256 a side, FUN_0040dcb0), its colours
//   from the file's palette as the game makes them 16-bit (FUN_00404e60), nothing around it.
// They hold for the game's texture quality at its highest ([TEXTUREMODE] 2, the default) and full-sized sprites.
struct Picture {
    Group group = Group::World;
    int width = 0, height = 0;
    std::vector<uint32_t> argb;
    std::string key;  // ai::keyName
};

// The game's BMP reader: 8-bit (its palette at 0x36) or 24-bit, bottom-up; every pixel opaque.
bool decodeBmp(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height);
// TGA, truecolour (24 or 32 bits), raw or run-length; alpha: it has its own (32 bits).
bool decodeTga(const uint8_t* data, size_t size, std::vector<uint32_t>& argb, int& width, int& height, bool& alpha);

// The game's 16-bit pixel of an ARGB one (blackKey: black transparent too, as some of its textures are loaded).
uint32_t gameA1R5G5B5(uint32_t argb, bool blackKey);
uint32_t gameA4R4G4B4(uint32_t argb, bool blackKey);
// A sprite palette's colour (R, G, B, A bytes) as the game's sprite cache holds it.
uint32_t spritePaletteColour(const uint8_t* rgba);
int spriteCell(int side);  // the sprite cache's cell for a frame's side: 32, 64, 128 or 256; 0 past 256

bool worldTexture(const uint8_t* data, size_t size, bool tga, Picture& out);
bool interfacePicture(const uint8_t* data, size_t size, Picture& out);
// The palette frames of a sprite file (SPR), with its own palette or another (1024 bytes, a dye's).
bool spriteFrames(const uint8_t* data, size_t size, std::vector<Picture>& out, const uint8_t* palette = nullptr);

}  // namespace uagfx::pack
