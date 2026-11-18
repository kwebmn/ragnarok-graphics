#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/image.hpp"

// 3.0.20-beta.82 (R-9VCE45 «В готелі такий силует»: under Assumptio a whitened rectangle of the hotel's bricks round
// the merchant, Direct3D 11): Assumptio's glow - the character's body and head drawn again, added as light - takes the
// game's own picture of the frame since 3.0.19-beta.5 (core/ai_copy_blend.hpp), and the card's copy of that picture
// held a wall where the frame is see-through. Which side is wrong: the card's texels of a frame against the game's
// own, the frame's texels by its draw, a picture of each for the report.
namespace uagfx::texelcheck {

constexpr int kNear = 16;  // two channels no further apart than this are the same
struct Compare {
    size_t texels = 0;
    size_t differ = 0;          // their alpha apart, or both seen (alpha >= kNear) and a colour apart
    size_t cardOnly = 0;        // seen on the card (alpha >= 128) where the game's texel is see-through (< kNear)
    size_t gameOnly = 0;        // seen in the game's memory (alpha >= 128) where the card's is see-through
    uint32_t cardOnlyMean = 0;  // the mean ARGB of the cardOnly texels (0: none)
};
// `n` texels of the game's (ARGB, as the card is sent them) against the card's
Compare compare(const uint32_t* game, const uint32_t* card, size_t n);
// The texels a draw samples of a texture width x height: its `count` vertices' texture coordinates (u v 24 bytes into
// a transformed vertex) from bytes[offset], their bounds out to whole texels and kept inside; empty when the vertices
// are not all there
Rect rectOf(const std::vector<uint8_t>& bytes, size_t offset, size_t stride, uint32_t count, int width, int height);
// A 32-bit BMP of width x height ARGB texels, rows top-down (B G R A bytes each); "" for none
std::string bmpOf(const uint32_t* argb, int width, int height);

// 3.0.22-beta.8 (R-QVKRKE: the same square on Direct3D 12, where the renderer - uaro-render64.exe - has no game memory
// to set the card against): the card's texels of the glow's frame read alone. Seen: alpha >= 128. A character's frame
// is seen in its middle, its outer ring little; a filled rectangle - what the square looks like - all of it
struct Seen {
    size_t texels = 0, seen = 0;
    size_t edge = 0, edgeSeen = 0;  // the outer ring of texels, how many of them seen
    Rect bounds;                    // of the seen ones (empty: none)
    uint32_t mean = 0;              // their mean ARGB (0: none)
};
Seen seenOf(const uint32_t* argb, int width, int height);

}  // namespace uagfx::texelcheck
