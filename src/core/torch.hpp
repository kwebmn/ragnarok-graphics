#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/frame.hpp"

namespace uagfx::torch {

// 0.23, the torches of uaRO's maps (map effect 47): the sprite effect\torch_01, two layers a frame (its .act) - a
// 64x64 glow (32-bit frame 0) the game blows up 7.5 to 8.9 times, turned and tinted yellow, and one of seven 128x128
// flame frames (1-7) at twice their size, the flame some 26 x 60 texels of it. Magnified that much on a large screen
// they are a red blotch with edges and a blurred drop; the driver draws them anew (win/gpu/torches.cpp).
enum class Layer : uint8_t { None, Glow, Flame };

// Where the flame is in its frames (torch_01's frames 1-7: the base at row 103, the tip at rows 43-54, columns 52-78),
// in texels of the 128x128 frame, which the sprite cache puts in its cell's top left corner.
constexpr float kBaseX = 65.0f, kBaseY = 103.0f;
constexpr float kHeight = 58.0f;  // base to tip
constexpr float kHalfWidth = 14.0f;

// The cells of the game's sprite caches that hold a torch frame now, by texture - from the uploads' cells: a cell
// written with another frame is no longer one.
class Cells {
public:
    void note(uint32_t texture, const SpriteCell* cells, uint32_t count);
    void forget(uint32_t texture);
    bool empty() const { return byTexture_.empty(); }
    // The layer whose cell holds texel (x, y) of the texture, and that cell.
    Layer at(uint32_t texture, float x, float y, Rect* cell = nullptr) const;

private:
    struct Cell {
        Rect rect;
        Layer layer;
    };
    std::unordered_map<uint32_t, std::vector<Cell>> byTexture_;
};

// A corner of the quad the game draws a layer with: D3D7 pixels, its depth and RHW, the texture's coordinates.
struct Corner {
    float x, y, z, rhw, u, v;
};

// A flame the game drew, in the game's pixels.
struct Flame {
    float baseX = 0, baseY = 0;  // the middle of its base
    float upX = 0, upY = 0;      // from the base to the tip
    float halfWidth = 0;
    float z = 0, rhw = 0;  // the quad's
    float alpha = 1;       // the game's fade (the quad's vertex alpha, 1 when it shows the layer as the .act has it)
};

// The flame of a quad drawn from the frame in `cell` of a texture of width x height texels; false when the corners
// do not map the texture onto the screen.
bool flameOf(const Corner q[4], int width, int height, const Rect& cell, Flame& out);

// How bright a flame burns at time t (seconds): about 1, within 1 +- amount, smooth; each seed its own way.
float flicker(double t, uint32_t seed, float amount);
// A torch's seed from where it stands on the map (world x and z, ten units a cell): the same while the camera moves.
uint32_t seedAt(float x, float z);

}  // namespace uagfx::torch
