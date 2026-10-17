#include "core/rsm.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx::rsm {
namespace {

class Reader {
public:
    Reader(const uint8_t* d, size_t n) : d_(d), n_(n) {}
    bool skip(size_t k) {
        if (n_ - p_ < k) return ok_ = false;
        p_ += k;
        return true;
    }
    template <typename T>
    T get() {
        T v{};
        if (n_ - p_ < sizeof v) {
            ok_ = false;
            return v;
        }
        std::memcpy(&v, d_ + p_, sizeof v);
        p_ += sizeof v;
        return v;
    }
    std::string name(size_t width) {
        if (n_ - p_ < width) {
            ok_ = false;
            return {};
        }
        const char* s = reinterpret_cast<const char*>(d_ + p_);
        p_ += width;
        return std::string(s, strnlen(s, width));
    }
    bool ok() const { return ok_; }

private:
    const uint8_t* d_;
    size_t n_;
    size_t p_ = 0;
    bool ok_ = true;
};

struct Node {
    std::string name, parent;
    std::vector<int32_t> textures;  // the model's texture of each of the node's
    float mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float offset[3] = {0, 0, 0}, pos[3] = {0, 0, 0};
    float angle = 0, axis[3] = {0, 1, 0}, scale[3] = {1, 1, 1};
    std::vector<float> verts;  // x y z
    struct Face {
        uint16_t v[3];
        uint16_t texture;
    };
    std::vector<Face> faces;
};

void turn(const float axis[3], float angle, float p[3]) {
    float x = axis[0], y = axis[1], z = axis[2];
    const float l = std::sqrt(x * x + y * y + z * z);
    if (l < 1e-9f || angle == 0.0f) return;
    x /= l, y /= l, z /= l;
    const float c = std::cos(angle), s = std::sin(angle), d = x * p[0] + y * p[1] + z * p[2];
    const float cx[3] = {y * p[2] - z * p[1], z * p[0] - x * p[2], x * p[1] - y * p[0]};
    const float a[3] = {x, y, z};
    for (int k = 0; k < 3; ++k) p[k] = p[k] * c + a[k] * d * (1.0f - c) + cx[k] * s;
}

}  // namespace

bool parse(const uint8_t* data, size_t size, Model& out, std::string& error) {
    out = Model{};
    if (!data || size < 6 || std::memcmp(data, "GRSM", 4) != 0) {
        error = "not a model file (GRSM)";
        return false;
    }
    Reader r(data, size);
    r.skip(4);
    const int major = r.get<uint8_t>(), minor = r.get<uint8_t>();
    const int v = major * 256 + minor;
    out.version = v;
    if (v >= 0x200) {
        error = "a model file of version 2 (not read)";
        return false;
    }
    r.skip(8);  // the animation's length, the shading
    if (v >= 0x104) r.skip(1);  // alpha
    r.skip(16);
    const int32_t textureCount = r.get<int32_t>();
    if (!r.ok() || textureCount < 0 || textureCount > 4096) {
        error = "a damaged model file";
        return false;
    }
    std::vector<std::string> textures;
    for (int32_t i = 0; i < textureCount; ++i) textures.push_back(r.name(40));
    const std::string mainName = r.name(40);
    const int32_t nodeCount = r.get<int32_t>();
    if (!r.ok() || nodeCount < 1 || nodeCount > 4096) {
        error = "a damaged model file";
        return false;
    }
    std::vector<Node> nodes(static_cast<size_t>(nodeCount));
    for (Node& n : nodes) {
        n.name = r.name(40);
        n.parent = r.name(40);
        const int32_t tc = r.get<int32_t>();
        if (!r.ok() || tc < 0 || tc > 4096) {
            error = "a damaged model file";
            return false;
        }
        for (int32_t i = 0; i < tc; ++i) n.textures.push_back(r.get<int32_t>());
        for (float& f : n.mat) f = r.get<float>();
        for (float& f : n.offset) f = r.get<float>();
        for (float& f : n.pos) f = r.get<float>();
        n.angle = r.get<float>();
        for (float& f : n.axis) f = r.get<float>();
        for (float& f : n.scale) f = r.get<float>();
        const int32_t vc = r.get<int32_t>();
        if (!r.ok() || vc < 0 || vc > 1000000) {
            error = "a damaged model file";
            return false;
        }
        n.verts.resize(size_t(vc) * 3);
        for (float& f : n.verts) f = r.get<float>();
        const int32_t tvc = r.get<int32_t>();
        if (!r.ok() || tvc < 0 || tvc > 1000000) {
            error = "a damaged model file";
            return false;
        }
        r.skip(size_t(tvc) * (v >= 0x102 ? 12 : 8));
        const int32_t fc = r.get<int32_t>();
        if (!r.ok() || fc < 0 || fc > 1000000) {
            error = "a damaged model file";
            return false;
        }
        for (int32_t i = 0; i < fc; ++i) {
            Node::Face f{};
            for (uint16_t& x : f.v) x = r.get<uint16_t>();
            r.skip(6);  // its texture coordinates
            f.texture = r.get<uint16_t>();
            r.skip(2 + 4 + (v >= 0x102 ? 4 : 0));  // padding, two-sided, the smoothing group
            n.faces.push_back(f);
        }
        if (v >= 0x105) {
            const int32_t keys = r.get<int32_t>();
            if (keys < 0 || keys > 100000) break;
            r.skip(size_t(keys) * 16);
        }
        const int32_t keys = r.get<int32_t>();
        if (!r.ok() || keys < 0 || keys > 100000) break;
        r.skip(size_t(keys) * 20);
    }
    if (!r.ok() && nodes.front().faces.empty()) {
        error = "a model file cut short";
        return false;
    }
    // each node's vertices into the model: its matrix and offset (not the main node's offset), then up the chain its
    // scale, turn and place (not the main node's place)
    const auto find = [&](const std::string& name) -> const Node* {
        for (const Node& n : nodes)
            if (n.name == name) return &n;
        return nullptr;
    };
    std::vector<std::vector<float>> placed(nodes.size());
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i < nodes.size(); ++i) {
        const Node& n = nodes[i];
        const bool main = n.name == mainName && n.parent.empty();
        placed[i].resize(n.verts.size());
        for (size_t k = 0; k + 2 < n.verts.size(); k += 3) {
            const float* m = n.mat;
            const float x = n.verts[k], y = n.verts[k + 1], z = n.verts[k + 2];
            float p[3] = {m[0] * x + m[3] * y + m[6] * z, m[1] * x + m[4] * y + m[7] * z,
                          m[2] * x + m[5] * y + m[8] * z};
            if (!main)
                for (int a = 0; a < 3; ++a) p[a] += n.offset[a];
            const Node* cur = &n;
            for (int depth = 0; cur && depth < 64; ++depth) {
                for (int a = 0; a < 3; ++a) p[a] *= cur->scale[a];
                turn(cur->axis, cur->angle, p);
                const bool curMain = cur->name == mainName && cur->parent.empty();
                if (!curMain)
                    for (int a = 0; a < 3; ++a) p[a] += cur->pos[a];
                const Node* up = cur->parent.empty() ? nullptr : find(cur->parent);
                cur = up == cur ? nullptr : up;
            }
            for (int a = 0; a < 3; ++a) {
                placed[i][k + size_t(a)] = p[a];
                lo[a] = std::min(lo[a], p[a]);
                hi[a] = std::max(hi[a], p[a]);
            }
        }
    }
    if (lo[0] > hi[0]) {
        error = "a model with no vertices";
        return false;
    }
    // as the game stands it: the middle across over the place, the bottom (the largest y) on the ground
    const float shift[3] = {-(lo[0] + hi[0]) * 0.5f, -hi[1], -(lo[2] + hi[2]) * 0.5f};
    for (int a = 0; a < 3; ++a) {
        out.lo[a] = lo[a] + shift[a];
        out.hi[a] = hi[a] + shift[a];
    }
    for (size_t i = 0; i < nodes.size(); ++i) {
        const Node& n = nodes[i];
        for (const Node::Face& f : n.faces) {
            if (f.texture >= n.textures.size()) continue;
            const int32_t t = n.textures[f.texture];
            if (t < 0 || size_t(t) >= textures.size()) continue;
            Part* part = nullptr;
            for (Part& p : out.parts)
                if (p.texture == textures[size_t(t)]) part = &p;
            if (!part) {
                out.parts.push_back(Part{});
                part = &out.parts.back();
                part->texture = textures[size_t(t)];
                for (int a = 0; a < 3; ++a) {
                    part->lo[a] = 1e30f;
                    part->hi[a] = -1e30f;
                }
            }
            ++part->faces;
            for (uint16_t vi : f.v) {
                if (size_t(vi) * 3 + 2 >= placed[i].size()) continue;
                for (int a = 0; a < 3; ++a) {
                    const float c = placed[i][size_t(vi) * 3 + size_t(a)] + shift[a];
                    part->lo[a] = std::min(part->lo[a], c);
                    part->hi[a] = std::max(part->hi[a], c);
                }
            }
        }
    }
    return true;
}

}  // namespace uagfx::rsm
