#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace uagfx::ai {

// What a texture is to the game, as the driver tells it from the way the game draws it (no file name reaches the
// driver): the [ai] page chooses which kinds the network works on. Monsters, players, NPCs and items on the ground
// are drawn alike - one kind, sprites.
enum class Kind : uint8_t { Ground, Objects, Sprites, Effects, Lightmap, None };
constexpr int kKinds = 4;  // the ones a setting chooses; a lightmap or a texture of no world draw is never enlarged

const char* kindName(Kind k);  // "ground", "objects", "sprites", "effects"; "" for the others
bool parseKind(const std::string& name, Kind& out);  // one of the four

// What a draw shows of its texture.
struct DrawFacts {
    int uvSets = 1;          // texture coordinate sets per vertex: the ground has two, the second for its lightmap
    int stage = 0;           // the texture's stage
    bool additive = false;   // blending adds to the picture (destination factor ONE): light, sparks, glows
    bool flatQuad = false;   // one quad facing the screen (flatQuad below): a sprite
};
// The ground's own texture, its lightmap, light and sparks (effects), a picture facing the screen (sprites), the rest
// of the world (buildings, trees, water: objects).
Kind drawKind(const DrawFacts& f);

// Whether a draw is one quad that faces the screen: 4 vertices (XYZRHW first, `stride` bytes apart; a strip, or a
// list through 6 indices) at one depth, or with their corners on a rectangle along the screen's axes. The ground and
// the models are drawn in perspective: their depth changes across a face.
bool flatQuad(const uint8_t* vertices, size_t stride, uint32_t vertexCount, uint32_t indexCount);

}  // namespace uagfx::ai
