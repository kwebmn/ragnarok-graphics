#include "core/game_models_place.hpp"

#include <algorithm>

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

void place(const Node& n, const Picture& pic, std::vector<Corner>& scratch, Placed& out,
           const std::vector<bool>* only, const float* toView) {
    const size_t vertices = n.vertices.size() / 3;
    scratch.resize(vertices);
    for (size_t v = 0; v < vertices; ++v) {
        float seen[3];
        if (toView)
            transform(toView, &n.vertices[3 * v], seen);  // 3.0.20-beta.14: a moving node's
        else
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
        if (!texture || (only && (f.texture >= only->size() || !(*only)[f.texture]))) continue;
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

void faceForLearning(const Node& n, const Face& f, const float* toWorld, const float sign[3],
                     const modelcensus::Colouring& how, float corners[3][3], uint32_t colours[3]) {
    for (int c = 0; c < 3; ++c) {
        const float* p = &n.vertices[size_t(f.v[c]) * 3];
        float w[3] = {p[0], p[1], p[2]};
        if (toWorld) transform(toWorld, p, w);
        for (int a = 0; a < 3; ++a) corners[c][a] = w[a] * sign[a];
        colours[c] = modelcensus::expected(how, f.colour[c], &f.lit[3 * c]);
    }
}

void Leaves::add(const Node& n, const float* toView, const std::function<bool(uint32_t)>& leafy) {
    if (wanted_.size() <= used_) wanted_.resize(used_ + 1);
    std::vector<bool>& wanted = wanted_[used_];
    wanted.assign(n.textures.size(), false);
    bool any = false;
    for (size_t t = 0; t < n.textures.size(); ++t)
        if (n.textures[t] && leafy(n.textures[t])) wanted[t] = any = true;
    if (!any) return;
    if (items_.size() <= used_) items_.resize(used_ + 1);
    items_[used_++] = Item{&n, toView};
}

void Leaves::place(const Picture& pic, const ParallelFor& run, size_t aJob) {
    if (!used_) return;
    aJob = std::max<size_t>(aJob, 1);
    if (outs_.size() < used_) outs_.resize(used_);
    const size_t jobs = (used_ + aJob - 1) / aJob;
    if (scratch_.size() < jobs) scratch_.resize(jobs);
    run(jobs, [&](size_t job) {
        const size_t end = std::min(used_, (job + 1) * aJob);
        for (size_t k = job * aJob; k < end; ++k) {
            outs_[k].clear();
            gamemodels::place(*items_[k].node, pic, scratch_[job], outs_[k], &wanted_[k], items_[k].toView);
        }
    });
}

}  // namespace uagfx::gamemodels
