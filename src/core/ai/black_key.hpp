#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::ai {

// 2.5.18: the skills' big effects - uaRO.exe's .str animations - take their textures with black as a key (FUN_0042c850:
// the texture manager's FUN_0040c590("effect\..", 1)): CTexture::Update (FUN_004176c0) leaves every black texel of the
// picture transparent. The texture pack's builder made their pictures as the rest of the game takes them, black seen:
// by its own key such a texture finds none (60 of a player's report, R-HNEYBN).

// The texels of a texture keyed so as the plain one has them (black seen) into plain, and which the key took away into
// shown (0): false when it is no such texture - none taken away, a black texel seen (the key would have taken it), or
// an alpha between none and all (an A4R4G4B4 texture: its texels' alpha is lost with the key).
bool plainOfBlackKey(const uint32_t* argb, int width, int height, std::vector<uint32_t>& plain,
                     std::vector<uint8_t>& shown);
// The key on the plain texture's picture (width x height, the texture's size times a whole factor): the texels it took
// away transparent there, enlarged as the game's key is (enlargeAlpha).
void keyAway(std::vector<uint32_t>& picture, int width, int height, const std::vector<uint8_t>& shown,
             int textureWidth, int textureHeight);

}  // namespace uagfx::ai
