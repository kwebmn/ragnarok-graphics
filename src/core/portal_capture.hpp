#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/states.hpp"

namespace uagfx::portal_capture {

// 2.7.53 (the tester after five builds of the new portal: «посмотри с самого начала и проведи анализ почему так
// получается, и давай сделаем порталы обдуманно и правильно»): the facts first - what the game draws its portals
// with, into the log: each of their draws for 120 frames (two seconds) from the first one seen on a map, three maps at
// most; each texture of theirs once, its texels in base64.
struct Vertex {
    float x, y, z, rhw;  // the game's: the screen's pixels, the depth buffer's z, 1 / the view's depth
    uint32_t diffuse, specular;
    float u, v;
};

class Recorder {
public:
    // Whether this draw of a portal on `map` is recorded: the first one of a map starts its 120 frames.
    bool take(const std::string& map);
    void endFrame();  // a frame drawn (EndScene)
    int frame() const { return frame_; }

private:
    std::string map_;
    int maps_ = 0;
    int left_ = 0;   // frames still recorded
    int frame_ = 0;  // of the current recording
};

// A draw's line - its frame, effect, states, texture - then a line each of its vertices (and its indices, if any).
std::vector<std::string> drawLines(int frame, int effectId, const DrawSetup& s, uint32_t texture, int width,
                                   int height, const Vertex* v, int count, const uint16_t* indices, int indexCount);

// A texture's line, then its texels (ARGB, little-endian, rows from the top) in base64, 160 characters a line.
std::vector<std::string> textureLines(uint32_t texture, int width, int height, const uint32_t* argb);

}  // namespace uagfx::portal_capture
