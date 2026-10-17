#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::rsm {

// 2.0, the new trees: a map model's .rsm file - what the forest needs of it: its textures and, for each, the box its
// faces fill once the model stands on the ground as the game places it (its middle over the map object's place, its
// bottom at the place's height; up along -y). Nodes, their matrices, offsets, turns and scales applied; the
// animation's keys left out (trees have none).
struct Part {
    std::string texture;  // "나무잡초꽃\newtree_01.bmp" (as the file names it, cp949 bytes)
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
    int faces = 0;
};

struct Model {
    int version = 0;
    std::vector<Part> parts;  // one a texture that has faces
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};  // the whole
};

// false (and why) when it is not a model file or is cut short.
bool parse(const uint8_t* data, size_t size, Model& out, std::string& error);

}  // namespace uagfx::rsm
