#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/states.hpp"

namespace uagfx::effect_capture {

// 3.0.15-beta.1 (the owner: «давай» - the skills' effects redrawn after nostalRE's recipes, the casting, Heal, Magnus
// and Storm Gust first): the facts before any redraw (feedback: the game's real draws first, 2.7.53's portal record
// its model) - each skill effect's draws the first time its number shows in a game, a few frames of its life, into
// uaro-graphics\logs\uaro-effects.txt (sent with a report); its textures once, made small. Betas only, kMostBytes.
constexpr int kFrames[] = {0, 4, 10, 20, 35, 60, 90, 130};  // frames after its first draw that are kept
constexpr int kMostEffects = 24;
constexpr int kMostDrawsAFrame = 300;
constexpr size_t kMostBytes = 1200000;  // the site takes 1.6 M characters a file
constexpr int kTextureSide = 64;        // a texture written at most this wide and high (its shape, not its texels)

struct Vertex {
    float x, y, z, rhw;  // the screen's pixels, the depth buffer's z, 1 / the view's depth
    uint32_t diffuse, specular;
    float u, v;
};

class Recorder {
public:
    // Whether this draw of effect `id` in `frame` (a count of frames the driver keeps) is written: its first draw
    // starts its record; the frames kFrames after it are written, kMostDrawsAFrame draws each.
    bool take(int id, uint32_t frame);
    // How far into its record the effect is (the frame's offset from its first draw); -1 not recorded
    int offset(int id, uint32_t frame) const;
    bool full() const { return bytes_ >= kMostBytes; }
    void wrote(size_t bytes) { bytes_ += bytes; }
    bool textureTold(uint32_t id);  // true the first time a texture is asked about

private:
    struct Effect {
        uint32_t first = 0, frame = 0;
        int draws = 0;  // in `frame`
    };
    std::map<int, Effect> effects_;
    std::vector<uint32_t> textures_;
    size_t bytes_ = 0;
};

// A draw's line - the effect, its frame offset, states, texture - then a line each of its vertices and its indices.
std::string drawText(int id, int offset, const DrawSetup& s, uint32_t texture, int width, int height,
                     const Vertex* v, int count, const uint16_t* indices, int indexCount);

// A texture's line and its texels made at most kTextureSide (box-averaged ARGB, little-endian, rows from the top) in
// base64
std::string textureText(uint32_t texture, int width, int height, const uint32_t* argb);

}  // namespace uagfx::effect_capture
