#include "core/game_models_card.hpp"

#include <algorithm>
#include <cstring>

namespace uagfx::gamemodels {

namespace {

uint32_t bitsOf(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

}  // namespace

modelcensus::Colouring colouringOf(const Frame& f) {
    modelcensus::Colouring how;
    how.way = f.lit ? modelcensus::Way::Lit : modelcensus::Way::Plain;
    std::copy(f.factors, f.factors + 3, how.factors);
    how.alpha = 0xFF;
    return how;
}

uint64_t colouringKey(const Frame& f) {
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](uint32_t v) {
        for (int b = 0; b < 4; ++b) h = (h ^ ((v >> (8 * b)) & 0xFF)) * 1099511628211ull;
    };
    for (const float k : f.factors) mix(bitsOf(k));
    mix(f.lit ? 1u : 0u);
    return h | 1;
}

bool CardStore::add(const Node& n) {
    if (nodes_.count(n.id)) return false;
    Kept& k = nodes_[n.id];
    k.node = n;
    k.first = faces();
    for (size_t f = 0; f < n.faces.size(); ++f)
        if (n.faces[f].texture < n.textures.size() && n.textures[n.faces[f].texture]) k.order.push_back(uint16_t(f));
    std::stable_sort(k.order.begin(), k.order.end(), [&n](uint16_t a, uint16_t b) {
        return n.textures[n.faces[a].texture] < n.textures[n.faces[b].texture];
    });
    for (size_t i = 0; i < k.order.size(); ++i) {
        const Face& f = n.faces[k.order[i]];
        const uint32_t texture = n.textures[f.texture];
        if (k.runs.empty() || k.runs.back().texture != texture) {
            CardRun r;
            r.texture = texture;
            r.first = k.first + uint32_t(i);
            for (int a = 0; a < 3; ++a) r.lo[a] = 1e30f, r.hi[a] = -1e30f;
            k.runs.push_back(r);
        }
        CardRun& run = k.runs.back();
        ++run.count;
        for (int c = 0; c < 3; ++c)  // 3.0.20-beta.12: its box
            for (int a = 0; a < 3; ++a) {
                const float v = n.vertices[size_t(f.v[c]) * 3 + size_t(a)];
                run.lo[a] = std::min(run.lo[a], v);
                run.hi[a] = std::max(run.hi[a], v);
            }
        uint32_t w[kCardFaceWords] = {};
        for (int c = 0; c < 3; ++c)
            for (int a = 0; a < 3; ++a) w[4 * c + a] = bitsOf(n.vertices[size_t(f.v[c]) * 3 + size_t(a)]);
        for (int u = 0; u < 6; ++u) w[12 + u] = bitsOf(f.uv[u]);
        w[18] = ((f.flags & kTwoSided) ? 1u : 0u) | (n.flipped ? 2u : 0u);
        words_.insert(words_.end(), w, w + kCardFaceWords);
    }
    if (key_) colourOf(k);
    return true;
}

void CardStore::clear() {
    nodes_.clear();
    words_.clear();
    key_ = 0;
}

void CardStore::colour(const modelcensus::Colouring& how, uint64_t key) {
    how_ = how;
    key_ = key;
    for (auto& [id, k] : nodes_) colourOf(k);
}

void CardStore::colourOf(Kept& k) {
    for (size_t i = 0; i < k.order.size(); ++i) {
        const Face& f = k.node.faces[k.order[i]];
        uint32_t* w = words_.data() + size_t(k.first + i) * kCardFaceWords;
        for (int c = 0; c < 3; ++c) w[4 * c + 3] = modelcensus::expected(how_, f.colour[c], &f.lit[3 * c]);
    }
}

void CardStore::frameOf(const std::vector<uint32_t>& ids, CardFrame& out) const {
    out.textures.clear();
    for (std::vector<uint32_t>& list : out.faces) list.clear();
    out.nodes = out.unknown = 0;
    at_.clear();
    for (const uint32_t id : ids) {
        const auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            ++out.unknown;
            continue;
        }
        ++out.nodes;
        for (const CardRun& r : it->second.runs) {
            const auto [at, fresh] = at_.try_emplace(r.texture, uint32_t(out.textures.size()));
            if (fresh) {
                out.textures.push_back(r.texture);
                if (out.faces.size() < out.textures.size()) out.faces.emplace_back();
            }
            std::vector<uint32_t>& list = out.faces[at->second];
            for (uint32_t i = 0; i < r.count; ++i) list.push_back(r.first + i);
        }
    }
    out.faces.resize(std::max(out.faces.size(), out.textures.size()));
}

void CardStore::runsOf(const std::vector<uint32_t>& ids, std::vector<CardRun>& out) const {
    out.clear();
    for (const uint32_t id : ids) {
        const auto it = nodes_.find(id);
        if (it != nodes_.end()) out.insert(out.end(), it->second.runs.begin(), it->second.runs.end());
    }
}

void CardStore::cornersOf(const CardRun& r, std::vector<float>& out) const {
    if (size_t(r.first + r.count) * kCardFaceWords > words_.size()) return;
    for (uint32_t f = r.first; f < r.first + r.count; ++f) {
        const uint32_t* w = words_.data() + size_t(f) * kCardFaceWords;
        for (int c = 0; c < 3; ++c) {
            float v[6];
            std::memcpy(v, w + 4 * c, 3 * sizeof(float));      // x y z
            std::memcpy(v + 3, w + 12 + 2 * c, 2 * sizeof(float));  // u v
            std::memcpy(v + 5, w + 4 * c + 3, sizeof(float));  // the colour's bits
            out.insert(out.end(), v, v + 6);
        }
    }
}

}  // namespace uagfx::gamemodels
