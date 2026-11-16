#include "core/own_ground_card.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::own_ground::card {

namespace {

uint32_t bitsOf(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

constexpr float kBelowOne = 0.99999994f;  // the largest float under 1 (own_ground.cpp's)

uint32_t fogOf(const Putting& pu, float zd) {  // own_ground::Shading::specularAt
    const int a = std::clamp(int(std::lround(pu.fogA + pu.fogB * zd)), 0, 255);
    return uint32_t(a) << 24 | pu.fogRgb;
}

uint32_t specularOf(const Putting& pu, const terrain::Vertex& v, uint8_t layer, float zd) {
    const uint32_t fog = fogOf(pu, zd);
    return layer == terrain::kGround ? fog : (fog & 0xFF000000u) | (v.blend & 0xFFFFFFu);
}

float liftOf(uint8_t layer) { return layer < 3 ? kLift[layer] : 0.0f; }

}  // namespace

bool putVertex(const Putting& pu, const terrain::Vertex& tv, bool smooth, uint8_t layer, Tl2& t) {
    if (!place(pu.view, pu.p, pu.axes, tv.pos, t)) return false;
    float zd = 1.0f / t.rhw;
    // 3.0.4-beta.29: the smooth ground eased onto the plain one as it nears the change (fineDepth) - a chunk changes
    // to the plain ground lying on it already: no jump, no crack along their seam
    if (smooth && zd > pu.morphFrom && tv.plain != tv.pos[1]) {
        const float f = std::min((zd - pu.morphFrom) / (pu.fineDepth - pu.morphFrom), 1.0f);
        const float mm = f * f * (3.0f - 2.0f * f);
        const float eased[3] = {tv.pos[0], tv.pos[1] + (tv.plain - tv.pos[1]) * mm, tv.pos[2]};
        place(pu.view, pu.p, pu.axes, eased, t);
        zd = 1.0f / t.rhw;
    }
    const float lift = liftOf(layer);
    if (lift > 0.0f) t.z = depthOf(pu.p, std::max(zd - lift, pu.p.nearZ + 0.005f));
    t.specular = specularOf(pu, tv, layer, zd);
    t.u0 = tv.uv[0];
    t.v0 = tv.uv[1];
    t.u1 = tv.lm[0];
    t.v1 = tv.lm[1];
    return true;
}

Clip clipOf(const Putting& pu, const terrain::Vertex& tv, bool smooth, uint8_t layer, bool past) {
    const float sign[3] = {float(pu.axes.x), float(pu.axes.y), float(pu.axes.z)};
    const auto viewOf = [&](const float f[3], float out[3]) {
        const float w[3] = {f[0] * sign[0], f[1] * sign[1], f[2] * sign[2]};
        toView(pu.view, w, out);
    };
    float v[3];
    viewOf(tv.pos, v);
    if (smooth && v[2] > pu.morphFrom) {
        const float f = std::min((v[2] - pu.morphFrom) / (pu.fineDepth - pu.morphFrom), 1.0f);
        const float mm = f * f * (3.0f - 2.0f * f);
        const float eased[3] = {tv.pos[0], tv.pos[1] + (tv.plain - tv.pos[1]) * mm, tv.pos[2]};
        viewOf(eased, v);
    }
    const camera::Projection& p = pu.p;
    const float zd = v[2];
    Clip c;
    c.w = zd;
    c.x = p.f0 * v[0] + p.cx * zd;
    c.y = p.f1 * v[1] + p.cy * zd;
    const float lift = liftOf(layer);
    const float zl = lift > 0.0f ? std::max(zd - lift, p.nearZ + 0.005f) : zd;
    const float a = p.farZ / (p.farZ - p.nearZ), b = p.farZ * p.nearZ / (p.farZ - p.nearZ);
    const float split = ownFrom(p);
    if (!past) {
        c.z = lift > 0.0f ? (a - b / zl) * zd : a * zd - b;
    } else {
        const float at = a - b / split;
        c.z = std::min(at + (1.0f - at) * (1.0f - split / zl), kBelowOne) * zd;
    }
    c.cut[0] = past ? zd - split : split - zd;
    c.cut[1] = zd - p.nearZ;
    c.specular = specularOf(pu, tv, layer, zd);
    return c;
}

namespace {

size_t countOf(const terrain::Mesh* m) { return m ? m->vertices.size() : 0; }

// the vertex the card's number n is of
const terrain::Vertex& vertexAt(const Meshes& m, size_t n) {
    const size_t fine = countOf(m.fine), plain = countOf(m.plain);
    if (n < fine) return m.fine->vertices[n];
    if (n < fine + plain) return m.plain->vertices[n - fine];
    return m.outskirts->vertices[n - fine - plain];
}

}  // namespace

Layout layoutOf(const Meshes& m) {
    Layout l;
    l.fine = uint32_t(countOf(m.fine));
    l.plain = uint32_t(countOf(m.plain));
    l.all = l.fine + l.plain + uint32_t(countOf(m.outskirts));
    return l;
}

void vertexWords(const Meshes& m, uint32_t from, uint32_t count, uint32_t* out) {
    for (uint32_t i = 0; i < count; ++i) {
        const terrain::Vertex& v = vertexAt(m, size_t(from) + i);
        uint32_t* w = out + size_t(i) * kVertexWords;
        w[0] = bitsOf(v.pos[0]), w[1] = bitsOf(v.pos[1]), w[2] = bitsOf(v.pos[2]), w[3] = bitsOf(v.plain);
        w[4] = bitsOf(v.uv[0]), w[5] = bitsOf(v.uv[1]), w[6] = bitsOf(v.lm[0]), w[7] = bitsOf(v.lm[1]);
    }
}

void blendWords(const Meshes& m, uint32_t from, uint32_t count, uint32_t* out) {
    for (uint32_t i = 0; i < count; ++i) out[i] = vertexAt(m, size_t(from) + i).blend;
}

void Lists::begin(size_t textures, bool blends, const std::vector<uint8_t>* leftOut) {
    textures_ = textures;
    blends_ = blends;
    leftOut_ = leftOut;
    added_.clear();
    draws_.clear();
    parts_.clear();
    partsFrom_.clear();
}

void Lists::add(const terrain::Mesh& mesh, int which, size_t chunk, bool nearSide, bool pastSide) {
    if (chunk >= mesh.chunks.size() || (!nearSide && !pastSide)) return;
    added_.push_back(Added{&mesh, which, chunk, nearSide, pastSide});
}

const std::vector<Draw>& Lists::plan() {
    // a group a layer's texture's side: (layer * (textures + 1) + texture) * 2 + side - the draws in its order
    const size_t groups = size_t(3) * (textures_ + 1) * 2;
    counts_.assign(groups, 0u);
    drawOf_.assign(groups, 0u);
    parts_.clear();
    partsFrom_.assign(added_.size() + 1, 0u);
    for (size_t a = 0; a < added_.size(); ++a) {
        partsFrom_[a] = uint32_t(parts_.size());
        const Added& ad = added_[a];
        const terrain::Chunk& ch = ad.mesh->chunks[ad.chunk];
        for (uint32_t b = ch.firstBatch; b < ch.firstBatch + ch.batchCount && b < ad.mesh->batches.size(); ++b) {
            const terrain::Batch& bt = ad.mesh->batches[b];
            if (bt.layer > terrain::kNeighbour || bt.indexCount == 0 || (bt.layer != terrain::kGround && !blends_))
                continue;
            const size_t tex = bt.texture >= 0 && size_t(bt.texture) < textures_ ? size_t(bt.texture) : textures_;
            if (leftOut_ && ad.which != 2 && tex < leftOut_->size() && (*leftOut_)[tex]) continue;  // the filler's
            for (int side = 0; side < 2; ++side) {
                if (!(side ? ad.pastSide : ad.nearSide)) continue;
                const size_t g = (size_t(bt.layer) * (textures_ + 1) + tex) * 2 + size_t(side);
                parts_.push_back(Part{b, uint32_t(g), counts_[g]});
                counts_[g] += bt.indexCount;
            }
        }
    }
    partsFrom_[added_.size()] = uint32_t(parts_.size());
    draws_.clear();
    for (size_t g = 0; g < groups; ++g) {
        if (!counts_[g]) continue;
        drawOf_[g] = uint32_t(draws_.size());
        Draw d;
        d.layer = uint8_t(g / 2 / (textures_ + 1));
        d.texture = uint32_t(g / 2 % (textures_ + 1));
        d.past = g % 2 != 0;
        d.count = counts_[g];
        draws_.push_back(d);
    }
    for (Part& p : parts_) p.draw = drawOf_[p.draw];
    return draws_;
}

void Lists::fill(const Layout& layout, const std::function<uint32_t*(size_t draw)>& where,
                 const ParallelFor& run) const {
    run(added_.size(), [&](size_t a) {
        const Added& ad = added_[a];
        const terrain::Mesh& m = *ad.mesh;
        for (uint32_t k = partsFrom_[a]; k < partsFrom_[a + 1]; ++k) {
            const Part& p = parts_[k];
            const terrain::Batch& bt = m.batches[p.batch];
            uint32_t* to = where(p.draw) + p.at;
            const uint32_t base = layout.baseOf(ad.which) + bt.firstVertex;
            const uint32_t* from = m.indices.data() + bt.firstIndex;
            for (uint32_t i = 0; i < bt.indexCount; ++i) to[i] = base + from[i];
        }
    });
}

}  // namespace uagfx::own_ground::card
