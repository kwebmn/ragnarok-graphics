#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/ground_cells.hpp"
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
    // 3.0.20-beta.41 (the audit of the 3D view's grass): the whole map's cells for the grass (ground_cells::ofMeshes
    // of the two above, the file's axes) - over the own ground in the 3D view none of the game's ground is noted
    ground_cells::Grid cells;
    // 3.0.20-beta.30 (the owner's «6,7,8,9», 6): the land past its edges (core/outskirts.hpp) - the 3D view's horizon
    terrain::Mesh outskirts;
    std::vector<float> seaFrame;  // 3.0.20-beta.31: the sea past its wet edges (outskirts::seaFrame), x y z a corner
    // 3.0.20-beta.42: 1 a corner of the ground (row by row of width + 1) some vertex of the outskirts takes its colour
    // from (its blend) - the land's chunks are coloured again when the game gives one of them another colour
    std::vector<uint8_t> edgeCorners;
    // 3.0.20-beta.48: 1 a texture of the .gnd's that is the makers' filler (outskirts::fillerTexture) - where the land
    // past the edges is drawn its batches are left out: the land lies there
    std::vector<uint8_t> fillerTextures;
    terrain::Atlas atlas;
    std::vector<float> cornerNormals;  // 3.0.4-beta.26: the cells' corners' (own_ground::cornerNormals) - its light
    unsigned restingCells = 0;  // 3.0.6-beta.3: the cells a model rests on (terrain::restingOn), for the log
    // 3.0.7-beta.3 (R-YXZM7Y «посмотри насчет освещения стен»): the light the game gives its walls (terrain::wallLight,
    // by the map's sun), 2 a cell - its right side's, its far side's (0 none): the game's own walls checked against it
    std::vector<uint32_t> wallLight;
    struct Picture {
        int width = 0, height = 0;
        std::vector<std::vector<uint32_t>> levels;  // ARGB, the whole first, then each half
        bool keyed = false;  // 3.0.9-beta.3: it had the game's see-through magenta, made clear (keyMagenta)
    };
    std::vector<Picture> textures;  // the .gnd's, by index; 0 x 0 not found
    // 3.0.4-beta.14 (the user chose «Дома и деревья вдали»): the map's models placed (core/own_models.hpp), their
    // textures (the magenta keyed away, as the game does), the way to the map's light (file places, up along -y)
    terrain::Mesh models;
    std::vector<Picture> modelTextures;
    float toLight[3] = {0.0f, -1.0f, 0.0f};
    unsigned modelFiles = 0, modelFilesRead = 0;
    // 3.0.4-beta.20 (the own ground on every map): its water as the game lays it (core/terrain.hpp water) - drawn past
    // the game's far plane, where the game lays none; its kind (4 lava) and level
    terrain::Mesh water;
    int waterType = -1;
    float waterLevel = 0.0f;
    // 3.0.4-beta.23: its waves as the game lifts them (core/water.hpp waveAt): their height, pitch and speed
    float waveHeight = 0.0f;
    int wavePitch = 0, waveSpeed = 0;
    // its pictures as the game names them, one after another, and their pace (game ticks of 16 ms a picture) - the far
    // water's while none of the game's own water is near (the game's then: the same picture as its own)
    std::vector<Picture> waterPictures;
    int waterAnim = 3;
};

}  // namespace uagfx::gpu
