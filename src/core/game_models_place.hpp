#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/game_camera.hpp"
#include "core/game_models.hpp"
#include "core/model_census.hpp"
#include "core/own_ground.hpp"

// 3.0.20-beta.8 (the owner's «давай по очереди», step 1 - R-XTEFYA: the renderer's CPU +2 ms a frame building the
// game's still models anew whenever the view moved): a node's faces placed for a picture as the game's draw would
// place and colour them, on a renderer's thread of its own; each vertex placed once a node
namespace uagfx::gamemodels {

// a picture's nodes as placed, by the renderer's texture id (the order first met) - three vertices a face, in the
// nodes' order and their faces'
class Placed {
public:
    void clear();  // keeps the room
    std::vector<own_ground::Tl1>& of(uint32_t texture);
    size_t textures() const { return used_; }
    uint32_t texture(size_t i) const { return ids_[i]; }
    const std::vector<own_ground::Tl1>& vertices(size_t i) const { return verts_[i]; }
    size_t triangles = 0, nodes = 0, unknown = 0;  // `unknown`: the renderer's count of ids it has no node for

private:
    std::vector<uint32_t> ids_;
    std::vector<std::vector<own_ground::Tl1>> verts_;
    size_t used_ = 0, last_ = 0;
};

// this picture's camera, the game's colouring and the fog
struct Picture {
    own_ground::View view{};
    camera::Projection p;
    modelcensus::Colouring how;
    const own_ground::Shading* fog = nullptr;  // null: no fog - `plain` the specular of every vertex
    uint32_t plain = 0;
};

// a vertex of the node placed: on the screen, its depth, its fog; `ok` false behind the eye
struct Corner {
    float x = 0, y = 0, z = 0, rhw = 0;
    uint32_t specular = 0;
    bool ok = false;
};

// the node's faces into `out`: each vertex into the view and onto the screen once (own_ground::toView, placeView); a
// face with a corner behind the eye left out (as the game), culled as the game's draw culls (shown), its corners
// coloured as the game colours them (modelcensus::expected), its texture's id the renderer's (0: unknown - left out).
// 3.0.20-beta.13: `only` - the node's textures wanted, by its texture index (the leaves alone); none: all.
// 3.0.20-beta.14: `toView` - a moving node's node-to-view matrix this frame (its own vertices by it, the camera aside)
void place(const Node& n, const Picture& pic, std::vector<Corner>& scratch, Placed& out,
           const std::vector<bool>* only = nullptr, const float* toView = nullptr);

}  // namespace uagfx::gamemodels
