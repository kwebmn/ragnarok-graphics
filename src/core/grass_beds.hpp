#pragma once
#include <string>
#include <vector>

#include "core/ground_cells.hpp"
#include "core/map_axes.hpp"
#include "core/rsm.hpp"
#include "core/rsw.hpp"

namespace uagfx::grass_beds {

// 3.0.1-beta.3 (R-SA3W7Y, «клумба в пронтері чомусь плоска»): the flower beds the maps' makers set as models. The
// round bed (85 maps; four round prontera's fountain) has a green top of weed 1.3 over the map's ground, and the grass
// grows on the map's own ground only (core/ground_cells.hpp: umbala's decks) - the bed stayed flat. Its top counts as
// the ground. Found over the client's 4201 models (a flat green part at most 4 over its model's bottom): this bed
// alone - the rest are lotus leaves on the water, fallen leaves and vines.
bool bedModel(const std::string& file);       // its file as the map names it (cp949 bytes), any case
bool bedTexture(const std::string& texture);  // a bed's soil: 프론테라\weed.bmp

struct Box {
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};  // up along -y
};
// The soil's parts of a bed (rsm::parse's: its middle over the place, its bottom at its height) where the map stands
// it - its scale, its turns (about y, x, z as the game turns a model), its place - in the file's places
std::vector<Box> place(const rsm::Model& model, const rsw::Model& at);
// Those in the game's world: x and z by the axes' signs (core/map_axes.hpp); none while the axes are not known
std::vector<Box> inWorld(const std::vector<Box>& file, const map::Axes& axes);
// The cells under each box take its height as their ground's too
void addTo(ground_cells::Grid& g, const std::vector<Box>& beds);

}  // namespace uagfx::grass_beds
