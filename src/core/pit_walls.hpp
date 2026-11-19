#pragma once
#include <string>

#include "core/gnd.hpp"
#include "core/terrain.hpp"

// 3.0.22-beta.22 (the owner's R-GK3ZCT, moc_pryd03 «тут тоже походу стен нету»; his «делать» 19.11): the makers'
// pits standing for walls in a house or a hall (cave_vault::ceilingFor) - the Pyramids' mazes are walkways with pits
// between them, cells with no top (or the makers' filler on top) lower than the floor beside them, their sides the
// walls' pictures going down: moc_pryd03 4560 of its 10000 cells, 20 under the walkways. From above they read as walls;
// in the 3D view the camera looked over them and across the whole maze. Upright faces on each floor cell's edge to such
// a pit (kLeastDepth under it at least), `height` up, facing the floor - in the picture of the pit's own side there as
// the makers laid it going down (its upper edge, the floor's, on top), else the map's commonest side picture; the
// floor's own lightmap along the edge; the colour as the outskirts' (the renderer's third mesh: the floor's corner,
// in Vertex::blend, times the wall's light against the floor's by the map's sun, in the colour's low byte, 127.5 the
// floor's own). For the 3D view alone - the own ground draws them as its third mesh there (no land past a house's
// edges) and never from above
namespace uagfx::pitwalls {

constexpr float kLeastDepth = 8.0f;
// The maps whose pits are a maze's walls. By the census of the 1246 maps' grounds (the 125 a ceiling covers, the
// Pyramids, the Sphinx): moc_pryd03 alone - its walls pits 4 cells thick between the walkways (1376 of its 2016 pit
// edges), the maze's outer wall the void past it. Elsewhere a pit is no wall: the void a house's rooms float in (their
// open sides made for the camera from above), a tower's drops round its platforms (1 to 8 cells), a hall's outer side
// - walls there would shut rooms and fence platforms (94 of the 125 maps would have had them)
bool mazeMap(const std::string& map);
// The walls' mesh in chunks of terrain::kChunk cells; empty where the map has no such pits
terrain::Mesh build(const gnd::Ground& g, const terrain::Atlas& atlas, const terrain::Sun& sun, float height);
// Their tops `height` over their floor (the room's ceiling: the models' walls' height, told after the ground is read)
void raise(terrain::Mesh& m, float height);

}  // namespace uagfx::pitwalls
