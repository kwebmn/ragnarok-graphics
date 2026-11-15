#pragma once
#include <cstdint>

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
// Where the edge lies under the map's water none of it - the sea past it is the next step's
constexpr int kRing = 320;        // cells off the edges (the 3D view's farthest sight some 3200 units)
constexpr int kChunkCells = 32;
constexpr int kWindow = 6;        // cells round a point of the edge it is smoothed over
struct Options {
    int ring = kRing;
    bool water = false;  // the map's water: its level (up along -y, under it greater)
    float waterLevel = 0.0f;
    uint32_t seed = 0;  // the hills' own (the map's name hashed)
};
terrain::Mesh build(const gnd::Ground& g, const terrain::Atlas& atlas, const terrain::Sun& sun, const Options& o);
// the height (up along -y) at the cells' place (cx, cy) - its first corner 0, 0 - off the map's ground
float heightAt(const gnd::Ground& g, const Options& o, float cx, float cy);

}  // namespace uagfx::outskirts
