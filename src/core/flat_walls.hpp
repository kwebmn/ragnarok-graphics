#pragma once
#include <vector>

#include "core/game_camera.hpp"
#include "core/map_axes.hpp"
#include "core/own_ground.hpp"
#include "core/terrain.hpp"

namespace uagfx::flatwalls {

// 3.0.23-beta.8 (the owner: «стены комнат на Direct3D 11»): Direct3D 11 draws no own ground - the 3D view's room
// walls there (core/pit_walls' mesh, raised to the ceiling) placed by the processor each picture as the game's
// one-texture vertices (FVF_TL1), flat in the room's walls' colour (the ceiling's, cave_vault::ceilingColour) lit by
// their side, the triangles cut at the near plane
struct Look {
    float rgb[3] = {0.5f, 0.45f, 0.4f};  // the walls' colour, 0..1
    float ambient = 0.6f, diffuse = 0.4f;  // the light: ambient + diffuse x the side's turn to kSide
};
void place(const terrain::Mesh& walls, const own_ground::View& v, const camera::Projection& p, const map::Axes& axes,
           const Look& look, std::vector<own_ground::Tl1>& out);

}  // namespace uagfx::flatwalls
