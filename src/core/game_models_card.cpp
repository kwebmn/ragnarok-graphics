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

const std::vector<CardRun>* CardStore::runsOfNode(uint32_t id) const {
    const auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : &it->second.runs;
}

bool CardStore::moving(uint32_t id) const {
    const auto it = nodes_.find(id);
    return it != nodes_.end() && it->second.node.moving;
}

const Node* CardStore::node(uint32_t id) const {
    const auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : &it->second.node;
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
    static const std::vector<uint32_t> kNone;
    frameOf(ids, kNone, out);
}

void CardStore::frameOf(const std::vector<uint32_t>& ids, const std::vector<uint32_t>& moving, CardFrame& out) const {
    out.textures.clear();
    for (std::vector<uint32_t>& list : out.faces) list.clear();
    out.nodes = out.unknown = out.moving = 0;
    at_.clear();
    for (const uint32_t id : ids) {
        const auto it = nodes_.find(id);
        if (it == nodes_.end()) {
            ++out.unknown;
            continue;
        }
        if (it->second.node.moving) continue;  // 3.0.20-beta.14: its own space - only with its matrix
        into(it->second, 0, out, at_);
    }
    for (size_t m = 0; m < moving.size() && m < kCardMostMoving; ++m) {  // 3.0.20-beta.14
        const auto it = nodes_.find(moving[m]);
        if (it == nodes_.end() || !it->second.node.moving) {
            ++out.unknown;
            continue;
        }
        into(it->second, uint32_t(m + 1), out, at_);
        ++out.moving;
    }
    out.faces.resize(std::max(out.faces.size(), out.textures.size()));
}

void CardStore::framePart(const std::vector<uint32_t>& ids, const std::vector<uint32_t>& moving, size_t k, size_t parts,
                          FramePart& part) const {
    CardFrame& out = part.frame;
    out.textures.clear();
    for (std::vector<uint32_t>& list : out.faces) list.clear();
    out.nodes = out.unknown = out.moving = 0;
    part.at.clear();
    const size_t still = ids.size(), all = still + std::min<size_t>(moving.size(), kCardMostMoving);
    if (!parts || k >= parts) return;
    for (size_t i = all * k / parts; i < all * (k + 1) / parts; ++i) {
        if (i < still) {  // as frameOf's
            const auto it = nodes_.find(ids[i]);
            if (it == nodes_.end()) {
                ++out.unknown;
                continue;
            }
            if (it->second.node.moving) continue;
            into(it->second, 0, out, part.at);
            continue;
        }
        const size_t m = i - still;
        const auto it = nodes_.find(moving[m]);
        if (it == nodes_.end() || !it->second.node.moving) {
            ++out.unknown;
            continue;
        }
        into(it->second, uint32_t(m + 1), out, part.at);
        ++out.moving;
    }
    out.faces.resize(std::max(out.faces.size(), out.textures.size()));
}

void joinParts(const std::vector<FramePart>& parts, size_t count, CardFrame& out) {
    out.textures.clear();
    for (std::vector<uint32_t>& list : out.faces) list.clear();
    out.nodes = out.unknown = out.moving = 0;
    std::unordered_map<uint32_t, uint32_t> at;
    at.reserve(256);
    for (size_t k = 0; k < count && k < parts.size(); ++k) {
        const CardFrame& p = parts[k].frame;
        out.nodes += p.nodes;
        out.unknown += p.unknown;
        out.moving += p.moving;
        for (size_t t = 0; t < p.textures.size(); ++t) {
            const auto [place, fresh] = at.try_emplace(p.textures[t], uint32_t(out.textures.size()));
            if (fresh) {
                out.textures.push_back(p.textures[t]);
                if (out.faces.size() < out.textures.size()) out.faces.emplace_back();
            }
            std::vector<uint32_t>& list = out.faces[place->second];
            list.insert(list.end(), p.faces[t].begin(), p.faces[t].end());
        }
    }
    out.faces.resize(std::max(out.faces.size(), out.textures.size()));
}

void CardStore::into(const Kept& k, uint32_t slot, CardFrame& out,
                     std::unordered_map<uint32_t, uint32_t>& places) const {
    ++out.nodes;
    const uint32_t high = slot << kCardSlotShift;
    for (const CardRun& r : k.runs) {
        const auto [at, fresh] = places.try_emplace(r.texture, uint32_t(out.textures.size()));
        if (fresh) {
            out.textures.push_back(r.texture);
            if (out.faces.size() < out.textures.size()) out.faces.emplace_back();
        }
        std::vector<uint32_t>& list = out.faces[at->second];
        for (uint32_t i = 0; i < r.count; ++i) list.push_back((r.first + i) | high);
    }
}

void CardStore::runsOf(const std::vector<uint32_t>& ids, std::vector<CardRun>& out) const {
    out.clear();
    for (const uint32_t id : ids) {
        const auto it = nodes_.find(id);
        if (it != nodes_.end()) out.insert(out.end(), it->second.runs.begin(), it->second.runs.end());
    }
}

void CardStore::cornersOf(const CardRun& r, std::vector<float>& out, const float* toWorld) const {
    if (size_t(r.first + r.count) * kCardFaceWords > words_.size()) return;
    for (uint32_t f = r.first; f < r.first + r.count; ++f) {
        const uint32_t* w = words_.data() + size_t(f) * kCardFaceWords;
        for (int c = 0; c < 3; ++c) {
            float v[6];
            std::memcpy(v, w + 4 * c, 3 * sizeof(float));      // x y z
            if (toWorld) {  // 3.0.20-beta.14: a moving node's own space
                const float p[3] = {v[0], v[1], v[2]};
                transform(toWorld, p, v);
            }
            std::memcpy(v + 3, w + 12 + 2 * c, 2 * sizeof(float));  // u v
            std::memcpy(v + 5, w + 4 * c + 3, sizeof(float));  // the colour's bits
            out.insert(out.end(), v, v + 6);
        }
    }
}

}  // namespace uagfx::gamemodels
