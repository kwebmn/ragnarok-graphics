#pragma once
#include <cstdint>
#include <functional>

#include "core/game_camera.hpp"
#include "core/game_models.hpp"
#include "core/map_axes.hpp"
#include "core/rsw.hpp"

namespace uagfx::gpu {

// 3.0.20-beta.22 (R-8UPTJW: Prontera's south gate not drawn; R-7VAPER: the warehouse's front walls missing): the
// renderer's side of the console's «Записати моделі навколо» - the frame's nodes we draw within kDumpReach of the
// player (still or moving, their faces kept on the card, those the card's rules show now, their corners in front of the
// eye, mirrored or not, where they stand in the world) and the map's own models there by their files, into the log;
// both renderers, at the frame's present
using NodeOf = std::function<const gamemodels::Node*(uint32_t id)>;
using CardFacesOf = std::function<uint32_t(uint32_t id)>;
void dumpModelsAround(const gamemodels::Frame& frame, const NodeOf& nodeOf, const CardFacesOf& cardFaces,
                      const float viewToWorld[12], const camera::Projection& p, const rsw::World* map,
                      const map::Axes& axes);

}  // namespace uagfx::gpu
