#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/terrain.hpp"

namespace uagfx::gpu {

// 3.0.4-beta.10 (the user: «сделай 1»): the own world's prototype - a map's ground from its .gnd for the renderer
// (own_ground.cpp, Renderer::Impl::drawOwnGround), read with the map by MapWorld's thread: its mesh in chunks
// (core/terrain.hpp), its lightmaps in one atlas, its textures in full colour with their mipmaps.
struct OwnGround {
    std::string map;
    uint32_t serial = 0;  // MapWorld's count: another read, another ground
    int width = 0, height = 0;  // its cells
    float zoom = 10.0f;
    terrain::Mesh mesh;  // smooth
    terrain::Mesh distant;  // the game's own cells, for the ground far off - the same chunks, one for one (not "far":
                            // windef.h makes it nothing)
    terrain::Atlas atlas;
    struct Picture {
        int width = 0, height = 0;
        std::vector<std::vector<uint32_t>> levels;  // ARGB, the whole first, then each half
    };
    std::vector<Picture> textures;  // the .gnd's, by index; 0 x 0 not found
    // 3.0.4-beta.14 (the user chose «Дома и деревья вдали»): the map's models placed (core/own_models.hpp), their
    // textures (the magenta keyed away, as the game does), the way to the map's light (file places, up along -y)
    terrain::Mesh models;
    std::vector<Picture> modelTextures;
    float toLight[3] = {0.0f, -1.0f, 0.0f};
    unsigned modelFiles = 0, modelFilesRead = 0;
};

}  // namespace uagfx::gpu
