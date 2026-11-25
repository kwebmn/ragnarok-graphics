// 3.0.23-beta.8 (the owner: «стены комнат на Direct3D 11» - the room walls Direct3D 12 draws as the own ground's third
// mesh): Direct3D 11 has no own ground - the walls' mesh read with the map (MapWorld::flatWalls, core/pit_walls)
// placed by the processor each picture (core/flat_walls), flat in the room's walls' colour as the 3D view's ceiling,
// and drawn through the world's path with the game's model state, at the world's first blended draw
#include <d3d11.h>

#include <algorithm>

#include "core/cave_vault.hpp"
#include "core/flat_walls.hpp"
#include "win/gpu/renderer11.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

void Renderer11::drawFlatWalls11() {
    const bool want = effects_.roomWalls3d && camera::wideView();
    mapWorld_.setFlatWalls(want);  // read with the map from now (again for this one when wanted anew)
    if (!want || !modelLikeKnown_ || !cameraValid_) return;
    const terrain::Mesh& walls = mapWorld_.flatWalls();
    if (walls.indices.empty()) return;
    own_ground::View view;
    if (!own_ground::viewOf(viewToWorld_, view)) return;
    const camera::Projection p = camera::projectionFor(gameWidth_, gameHeight_);
    const map::Axes axes = mapWorld_.axes().known() ? mapWorld_.axes() : map::Axes{1, 1, 1};
    // the room's walls' colour by the map's light, as the ceiling's (cave_vault::ceilingColour)
    const rsw::World* rw = mapWorld_.world();
    const float none[3] = {0, 0, 0};
    float colour[4] = {0.5f, 0.45f, 0.4f, 0.0f}, known[3];
    const bool wallsKnown = mapWorld_.wallsColour(known);
    cave_vault::ceilingColour(wallsKnown, known, rw && rw->lightKnown, rw ? rw->lightAmbient : none,
                              rw ? rw->lightDiffuse : none, colour);
    flatwalls::Look look;
    std::copy(colour, colour + 3, look.rgb);
    wallTl_.clear();
    flatwalls::place(walls, view, p, axes, look, wallTl_);
    if (wallsTold_ != map_) {
        wallsTold_ = map_;
        LOG_INFO("3D view: %s's room walls drawn flat - %u faces, their colour %.2f %.2f %.2f%s (Direct3D 11)",
                 map_.c_str(), unsigned(walls.indices.size() / 6), double(look.rgb[0]), double(look.rgb[1]),
                 double(look.rgb[2]), wallsKnown ? "" : " (the walls' own not known: plain stone)");
    }
    if (wallTl_.empty()) return;
    wallRec_.reset();
    DrawSetup st = modelLike_;
    st.cull = d3d7::CULL_NONE;  // both sides: a wall seen from the room or past it
    st.blend = false;
    const uint32_t state = wallRec_.addState(st);
    for (size_t at = 0; at < wallTl_.size(); at += 65535 / 3 * 3) {
        const size_t n = std::min<size_t>(65535 / 3 * 3, wallTl_.size() - at);
        DrawCmd cmd;
        cmd.state = state;
        cmd.texture[0] = 0;  // none: the white one - the vertices' colour alone
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        wallRec_.draw(cmd, wallTl_.data() + at, uint32_t(n), nullptr, 0);
    }
    flushBatch();  // the game's draws gathered so far are of its own vertices
    const std::vector<uint8_t>& v = wallRec_.vertexBytes();
    if (!fillBuffer(wallVertices_, wallVertexBytes_, D3D11_BIND_VERTEX_BUFFER, v.data(), v.size(),
                    bgraVertices_ ? nullptr : &wallRec_))
        return;
    std::swap(vertices_, wallVertices_);
    for (const DrawCmd& cmd : wallRec_.draws()) draw(wallRec_, cmd);
    flushBatch();
    std::swap(vertices_, wallVertices_);
}

}  // namespace uagfx::gpu
