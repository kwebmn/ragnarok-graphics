#pragma once
#include <cstdint>
#include <vector>

#include "core/gnd.hpp"
#include "core/terrain.hpp"

namespace uagfx::outskirts {

// 3.0.20-beta.30 (the owner's «6,7,8,9», 6 - «Горизонт за краем карты в 3D-режиме»: past a map's edge nothing, the
// world an island in the sky): the land round a map past its ground's edges, `ring` cells off. Its heights: at the edge
// the map's corners, a few cells on the edge's own smoothed along it (its cells within kWindow), going over into hills
// and, from some 40 cells off, mountains rising to the horizon (the 3D view's haze takes them). Its texture the edge's
// most common one there, as many cells a picture as there; its light a lightmap cell of the map's mean
// (terrain::meanLightCell); its colour the renderer's - the edge's (own_ground::Shading) times its own slope's light
// by the map's sun, kept in each vertex's colour (grey, 127.5 the edge's own). Chunks of kChunkCells a side, their
// quads the larger the further off; each chunk's edges hang a skirt down (no crack between two sizes), the map's edge
// a curtain.
// 3.0.20-beta.31: where the edge lies under the map's water its bed, flat under the level (seaFrame's sea over it)
constexpr int kRing = 320;        // cells off the edges (the 3D view's farthest sight some 3200 units)
constexpr int kChunkCells = 32;
constexpr int kWindow = 6;        // cells round a point of the edge it is smoothed over
struct Options {
    int ring = kRing;
    bool water = false;  // the map's water: its level (up along -y, under it greater)
    float waterLevel = 0.0f;
    uint32_t seed = 0;  // the hills' own (the map's name hashed)
};
// 3.0.20-beta.42 (the audit of the horizon): the land begins where the map's ground ends, not at its rectangle - the
// open cells (no top) reached from outside are the land's too (most maps keep one to three rings of them: a trench of
// nothing lay between the ground and beta.30's land); a hole the ground closes round stays the map's. A map hung in
// the sky by its makers (nine tenths of its outer band of cells open: yuno, gonryun) has no land round it at all. A
// vertex's `blend`: the corner of the ground it lies nearest to (its number row by row of width + 1, the low 24 bits)
// and how far off in sixteenths of a cell (the high 8, 255 at most) - the renderer colours it by that corner.
// sea: seaFrame's of the same map, made with it (one plan for both)
terrain::Mesh build(const gnd::Ground& g, const terrain::Atlas& atlas, const terrain::Sun& sun, const Options& o,
                    std::vector<float>* sea = nullptr);
// the land's height (up along -y) at the cells' corner (cx, cy) - the map's first corner 0, 0
float heightAt(const gnd::Ground& g, const Options& o, float cx, float cy);
// 3.0.20-beta.31 (6 and 7: the sea past a map's wet edges): the sea round the map at its water's level, as far as the
// outskirts - four quads (two triangles each, x y z a corner, the world's places as the meshes') with the map's own
// water inside them; where the outskirts' land stands over it, it hides it. None for a map without water or whose
// edge lies under it at fewer than a quarter of its corners - a river's or a lake's at the edge, no sea (its land all
// dry past the edge). 3.0.20-beta.42: and over the open cells within the map the land fills (the map's own water ends
// with its ground), their rows' runs joined into boxes
std::vector<float> seaFrame(const gnd::Ground& g, const Options& o);

}  // namespace uagfx::outskirts
