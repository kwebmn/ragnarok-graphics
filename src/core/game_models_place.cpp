#include "core/game_models_place.hpp"

namespace uagfx::gamemodels {

void Placed::clear() {
    for (size_t i = 0; i < used_; ++i) verts_[i].clear();
    used_ = last_ = 0;
    triangles = nodes = unknown = 0;
}

std::vector<own_ground::Tl1>& Placed::of(uint32_t texture) {
    if (last_ < used_ && ids_[last_] == texture) return verts_[last_];
    for (size_t i = 0; i < used_; ++i)
        if (ids_[i] == texture) return verts_[last_ = i];
    if (used_ == ids_.size()) {
        ids_.push_back(texture);
        verts_.emplace_back();
    }
    ids_[used_] = texture;
    last_ = used_++;
    return verts_[last_];
}

void place(const Node& n, const Picture& pic, std::vector<Corner>& scratch, Placed& out) {
    const size_t vertices = n.vertices.size() / 3;
    scratch.resize(vertices);
    for (size_t v = 0; v < vertices; ++v) {
        float seen[3];
        own_ground::toView(pic.view, &n.vertices[3 * v], seen);
        own_ground::Tl2 placed;
        Corner& c = scratch[v];
        c.ok = own_ground::placeView(pic.p, seen, placed);
        if (!c.ok) continue;
        c.x = placed.x, c.y = placed.y, c.z = placed.z, c.rhw = placed.rhw;
        c.specular = pic.fog ? pic.fog->specularAt(seen[2]) : pic.plain;
    }
    ++out.nodes;
    for (const Face& f : n.faces) {
        const uint32_t texture = f.texture < n.textures.size() ? n.textures[f.texture] : 0;
        if (!texture) continue;
        const Corner* c[3] = {&scratch[f.v[0]], &scratch[f.v[1]], &scratch[f.v[2]]};
        if (!c[0]->ok || !c[1]->ok || !c[2]->ok) continue;  // a corner behind the eye: as the game, not drawn
        const float s0[2] = {c[0]->x, c[0]->y}, s1[2] = {c[1]->x, c[1]->y}, s2[2] = {c[2]->x, c[2]->y};
        if (!shown(s0, s1, s2, (f.flags & kTwoSided) != 0, n.flipped)) continue;
        std::vector<own_ground::Tl1>& verts = out.of(texture);
        for (int k = 0; k < 3; ++k) {
            own_ground::Tl1 t;
            t.x = c[k]->x, t.y = c[k]->y, t.z = c[k]->z, t.rhw = c[k]->rhw;
            t.diffuse = modelcensus::expected(pic.how, f.colour[k], &f.lit[3 * k]);
            t.specular = c[k]->specular;
            t.u = f.uv[2 * k], t.v = f.uv[2 * k + 1];
            verts.push_back(t);
        }
        ++out.triangles;
    }
}

}  // namespace uagfx::gamemodels
