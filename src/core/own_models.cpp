#include "core/own_models.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>

namespace uagfx::own_models {
namespace {

std::string lower(std::string s) {  // the ASCII letters (the Korean names' cp949 bytes stay)
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

struct Part {  // a chunk's texture: its vertices, its triangles' indices into them
    std::vector<terrain::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::map<std::array<uint32_t, 5>, uint32_t> known;  // a corner (its place and texture coordinates' bits) -> vertex
};

std::array<uint32_t, 5> bitsOf(const float p[3], const float uv[2]) {
    std::array<uint32_t, 5> k{};
    std::memcpy(k.data(), p, 12);
    std::memcpy(k.data() + 3, uv, 8);
    return k;
}

}  // namespace

terrain::Mesh place(const std::vector<rsw::Model>& instances, const std::vector<const rsm::Model*>& files, int width,
                    int height, float zoom, std::vector<std::string>& textures) {
    terrain::Mesh m;
    std::map<std::string, int> known;  // a texture's name (lower case) -> its index
    std::map<int64_t, std::map<int, Part>> chunks;  // the chunk's square (y, x) -> its textures
    for (size_t i = 0; i < instances.size() && i < files.size(); ++i) {
        const rsm::Model* file = files[i];
        if (!file) continue;
        const rsw::Model& at = instances[i];
        const int cx = std::clamp(int(std::floor(at.pos[0] / zoom + float(width) / 2.0f)), 0, std::max(width - 1, 0));
        const int cy = std::clamp(int(std::floor(at.pos[2] / zoom + float(height) / 2.0f)), 0, std::max(height - 1, 0));
        std::map<int, Part>& chunk = chunks[int64_t(cy / terrain::kChunk) << 32 | int64_t(cx / terrain::kChunk)];
        for (const rsm::Face& f : file->faces) {
            if (f.texture < 0 || size_t(f.texture) >= file->textures.size()) continue;
            const std::string name = lower(file->textures[size_t(f.texture)]);
            auto t = known.find(name);
            if (t == known.end()) {
                t = known.emplace(name, int(textures.size())).first;
                textures.push_back(file->textures[size_t(f.texture)]);
            }
            float p[3][3];
            for (int k = 0; k < 3; ++k) {
                std::copy(f.pos[k], f.pos[k] + 3, p[k]);
                rsw::placeInMap(at, p[k]);
            }
            const float ax = p[1][0] - p[0][0], ay = p[1][1] - p[0][1], az = p[1][2] - p[0][2];
            const float bx = p[2][0] - p[0][0], by = p[2][1] - p[0][1], bz = p[2][2] - p[0][2];
            float n[3] = {ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx};
            const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (l < 1e-9f) continue;  // no area
            for (float& c : n) c /= l;
            Part& part = chunk[t->second];
            for (int k = 0; k < 3; ++k) {
                const auto key = bitsOf(p[k], f.uv[k]);
                auto at = part.known.find(key);
                if (at == part.known.end()) {
                    terrain::Vertex v{};
                    std::copy(p[k], p[k] + 3, v.pos);
                    v.uv[0] = f.uv[k][0];
                    v.uv[1] = f.uv[k][1];
                    v.color = 0xFFFFFFFFu;
                    at = part.known.emplace(key, uint32_t(part.vertices.size())).first;
                    part.vertices.push_back(v);
                }
                terrain::Vertex& v = part.vertices[at->second];
                for (int a = 0; a < 3; ++a) v.normal[a] += n[a];  // made unit once all its faces are in
                part.indices.push_back(at->second);
            }
        }
    }
    for (const auto& entry : chunks) {
        terrain::Chunk ch;
        ch.cellX = int(entry.first & 0xFFFFFFFF) * terrain::kChunk;
        ch.cellY = int(entry.first >> 32) * terrain::kChunk;
        ch.firstVertex = uint32_t(m.vertices.size());
        ch.firstBatch = uint32_t(m.batches.size());
        for (int k = 0; k < 3; ++k) ch.lo[k] = 1e30f, ch.hi[k] = -1e30f;
        for (const auto& t : entry.second) {
            // a batch of 16-bit indices: past 65535 vertices the texture goes on in another (its triangles' corners
            // taken again there)
            const Part& part = t.second;
            size_t i = 0;
            while (i < part.indices.size()) {
                terrain::Batch batch;
                batch.texture = t.first;
                batch.firstVertex = uint32_t(m.vertices.size());
                batch.firstIndex = uint32_t(m.indices.size());
                std::map<uint32_t, uint32_t> local;  // the part's vertex -> the batch's
                for (; i + 3 <= part.indices.size(); i += 3) {
                    if (local.size() + 3 > 65535) break;
                    for (int k = 0; k < 3; ++k) {
                        const uint32_t pv = part.indices[i + size_t(k)];
                        auto at = local.find(pv);
                        if (at == local.end()) {
                            at = local.emplace(pv, uint32_t(local.size())).first;
                            terrain::Vertex v = part.vertices[pv];
                            const float l = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] +
                                                      v.normal[2] * v.normal[2]);
                            for (float& c : v.normal) c = l > 1e-9f ? c / l : 0.0f;
                            for (int a = 0; a < 3; ++a)
                                ch.lo[a] = std::min(ch.lo[a], v.pos[a]), ch.hi[a] = std::max(ch.hi[a], v.pos[a]);
                            m.vertices.push_back(v);
                        }
                        m.indices.push_back(at->second);
                    }
                }
                batch.vertexCount = uint32_t(local.size());
                batch.indexCount = uint32_t(m.indices.size()) - batch.firstIndex;
                if (batch.indexCount) m.batches.push_back(batch);
            }
        }
        ch.vertexCount = uint32_t(m.vertices.size()) - ch.firstVertex;
        ch.batchCount = uint32_t(m.batches.size()) - ch.firstBatch;
        if (ch.vertexCount) m.chunks.push_back(ch);
    }
    return m;
}

uint64_t Learning::keyOf(int x, int y, int z) {
    return (uint64_t(uint32_t(x) & 0x1FFFFF) << 42) | (uint64_t(uint32_t(y) & 0x1FFFFF) << 21) |
           uint64_t(uint32_t(z) & 0x1FFFFF);
}

void Learning::reset(const terrain::Mesh& models, const float toLight[3]) {
    mesh_ = &models;
    std::copy(toLight, toLight + 3, toLight_);
    middles_.clear();
    learnt_.assign(models.vertices.size(), 0u);
    std::fill(&sums_[0][0][0], &sums_[0][0][0] + sizeof sums_ / sizeof sums_[0][0][0], 0.0);
    for (int c = 0; c < 4; ++c) a_[c] = 255.0f, b_[c] = 0.0f;
    side_ = 1.0f;
    seen_ = matched_ = changes_ = 0;
    triangles_.clear();
    for (const terrain::Batch& b : models.batches)
        for (uint32_t i = b.firstIndex; i + 3 <= b.firstIndex + b.indexCount; i += 3) {
            Triangle t;
            bool whole = true;
            for (int k = 0; k < 3; ++k) {
                t.v[k] = b.firstVertex + models.indices[i + uint32_t(k)];
                whole = whole && t.v[k] < models.vertices.size();
            }
            if (!whole) continue;
            float mid[3] = {0, 0, 0};
            for (int k = 0; k < 3; ++k)
                for (int a = 0; a < 3; ++a) mid[a] += models.vertices[t.v[k]].pos[a] / 3.0f;
            middles_.emplace(keyOf(int(std::lround(mid[0] * 4.0f)), int(std::lround(mid[1] * 4.0f)),
                                   int(std::lround(mid[2] * 4.0f))),
                             uint32_t(triangles_.size()));
            triangles_.push_back(t);
        }
}

bool Learning::note(const float corners[3][3], const uint32_t colours[3]) {
    if (!mesh_) return false;
    ++seen_;
    // how much it faces the light (both ways), its colours' mean: into the line's sums
    const float ax = corners[1][0] - corners[0][0], ay = corners[1][1] - corners[0][1];
    const float az = corners[1][2] - corners[0][2], bx = corners[2][0] - corners[0][0];
    const float by = corners[2][1] - corners[0][1], bz = corners[2][2] - corners[0][2];
    const float n[3] = {ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx};
    const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (l > 1e-6f) {
        const float d = (n[0] * toLight_[0] + n[1] * toLight_[1] + n[2] * toLight_[2]) / l;
        for (int s = 0; s < 2; ++s) {
            const double x = std::max(0.0f, s == 0 ? d : -d);
            for (int c = 0; c < 4; ++c) {
                double mean = 0.0;
                for (int k = 0; k < 3; ++k) mean += double((colours[k] >> (c * 8)) & 0xFF) / 3.0;
                double* q = sums_[s][c];
                q[0] += 1.0, q[1] += x, q[2] += x * x, q[3] += mean, q[4] += x * mean;
            }
        }
    }
    // one of ours: its middle within a quarter unit (a corner of the quarter grid next to it too), each corner near one
    float mid[3] = {0, 0, 0};
    for (int k = 0; k < 3; ++k)
        for (int a = 0; a < 3; ++a) mid[a] += corners[k][a] / 3.0f;
    const int q[3] = {int(std::floor(mid[0] * 4.0f)), int(std::floor(mid[1] * 4.0f)), int(std::floor(mid[2] * 4.0f))};
    for (int k = 0; k < 8; ++k) {
        const auto it = middles_.find(keyOf(q[0] + (k & 1), q[1] + ((k >> 1) & 1), q[2] + ((k >> 2) & 1)));
        if (it == middles_.end()) continue;
        const Triangle& tri = triangles_[it->second];
        int mine[3] = {-1, -1, -1};
        bool all = true;
        for (int c = 0; c < 3 && all; ++c) {
            float best = 0.09f;  // 0.3 units, squared
            for (int o = 0; o < 3; ++o) {
                const float* p = mesh_->vertices[tri.v[o]].pos;
                const float dx = p[0] - corners[c][0], dy = p[1] - corners[c][1], dz = p[2] - corners[c][2];
                const float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < best) best = d2, mine[c] = o;
            }
            all = mine[c] >= 0;
        }
        if (!all) continue;
        for (int c = 0; c < 3; ++c) {
            const uint32_t now = colours[c] ? colours[c] : 1u;
            uint32_t& kept = learnt_[tri.v[mine[c]]];
            changes_ += kept != now ? 1 : 0;
            kept = now;
        }
        ++matched_;
        return true;
    }
    return false;
}

void Learning::fit() {
    double best = 0.0;
    bool have = false;
    for (int s = 0; s < 2; ++s) {
        double rss = 0.0, a[4], b[4];
        bool ok = true;
        for (int c = 0; c < 4; ++c) {
            const double* q = sums_[s][c];
            if (q[0] < 1.0) {
                ok = false;
                break;
            }
            const double d = q[0] * q[2] - q[1] * q[1];
            b[c] = std::fabs(d) < 1e-9 ? 0.0 : (q[0] * q[4] - q[1] * q[3]) / d;
            a[c] = (q[3] - b[c] * q[1]) / q[0];
            // the sum of squares left: S(c - a - b x)^2 needs S c^2, not kept - the line's own share of the variance
            // compares the two sides alike: the larger b^2 Sxx' (x's spread), the more the side explains
            rss -= b[c] * b[c] * (q[2] - q[1] * q[1] / q[0]);
        }
        if (!ok) continue;
        if (!have || rss < best) {
            have = true;
            best = rss;
            side_ = s == 0 ? 1.0f : -1.0f;
            for (int c = 0; c < 4; ++c) a_[c] = float(a[c]), b_[c] = float(b[c]);
        }
    }
}

uint32_t Learning::colourOf(uint32_t vertex) const {
    if (vertex < learnt_.size() && learnt_[vertex]) return learnt_[vertex] == 1u ? 0u : learnt_[vertex];
    if (!mesh_ || vertex >= mesh_->vertices.size()) return 0xFFFFFFFFu;
    const float* n = mesh_->vertices[vertex].normal;
    const float x = std::max(0.0f, side_ * (n[0] * toLight_[0] + n[1] * toLight_[1] + n[2] * toLight_[2]));
    uint32_t out = 0;
    for (int c = 0; c < 4; ++c)  // rounded by hand: lround was most of the far models' pass
        out |= uint32_t(std::clamp(a_[c] + b_[c] * x, 0.0f, 255.0f) + 0.5f) << (c * 8);
    return out;
}

}  // namespace uagfx::own_models
