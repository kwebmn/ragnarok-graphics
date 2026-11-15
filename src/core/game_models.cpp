#include "core/game_models.hpp"

#include <cmath>
#include <cstring>

namespace uagfx::gamemodels {

namespace {
constexpr uint32_t kMostTextures = 256, kMostVertices = 65536, kMostFaces = 65536;
}  // namespace

void write(bridge::WireWriter& w, const Node& n) {
    w.u32(n.id);
    w.u32(n.map);
    w.u32(uint32_t(n.textures.size()));
    for (const uint32_t t : n.textures) w.u32(t);
    w.u32(uint32_t(n.vertices.size() / 3));
    w.bytes(n.vertices.data(), n.vertices.size() * sizeof(float));
    w.u32(uint32_t(n.faces.size()));
    w.bytes(n.faces.data(), n.faces.size() * sizeof(Face));
}

bool read(bridge::WireReader& r, Node& n) {
    n.id = r.u32();
    n.map = r.u32();
    const uint32_t textures = r.u32();
    if (!r.ok() || textures > kMostTextures) return false;
    n.textures.resize(textures);
    for (uint32_t& t : n.textures) t = r.u32();
    const uint32_t vertices = r.u32();
    if (!r.ok() || vertices > kMostVertices) return false;
    const uint8_t* v = r.take(size_t(vertices) * 3 * sizeof(float));
    if (!v) return false;
    n.vertices.resize(size_t(vertices) * 3);
    std::memcpy(n.vertices.data(), v, n.vertices.size() * sizeof(float));
    const uint32_t faces = r.u32();
    if (!r.ok() || faces > kMostFaces) return false;
    const uint8_t* f = r.take(size_t(faces) * sizeof(Face));
    if (!f) return false;
    n.faces.resize(faces);
    std::memcpy(n.faces.data(), f, n.faces.size() * sizeof(Face));
    for (const Face& face : n.faces)
        if (face.v[0] >= vertices || face.v[1] >= vertices || face.v[2] >= vertices ||
            (face.texture >= n.textures.size() && !n.textures.empty()))
            return false;
    return r.ok();
}

void compose(const float a[12], const float b[12], float out[12]) {
    float m[12];
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col)
            m[row * 3 + col] = a[row * 3] * b[col] + a[row * 3 + 1] * b[3 + col] + a[row * 3 + 2] * b[6 + col] +
                               (row == 3 ? b[9 + col] : 0.0f);
    std::memcpy(out, m, sizeof m);
}

void worldOf(const float combined[12], const float viewInverse[12], float world[12]) {
    compose(combined, viewInverse, world);
}

void transform(const float m[12], const float p[3], float out[3]) {
    for (int col = 0; col < 3; ++col) out[col] = p[0] * m[col] + p[1] * m[3 + col] + p[2] * m[6 + col] + m[9 + col];
}

uint64_t Corners::key(int x, int y, int z) const {
    return (uint64_t(uint32_t(x) & 0x1FFFFF) << 42) | (uint64_t(uint32_t(y) & 0x1FFFFF) << 21) |
           uint64_t(uint32_t(z) & 0x1FFFFF);
}

void Corners::add(const float p[3]) {
    const int x = int(std::floor(p[0] / cell_)), y = int(std::floor(p[1] / cell_)), z = int(std::floor(p[2] / cell_));
    cells_[key(x, y, z)].push_back({p[0], p[1], p[2]});
    ++size_;
}

bool Corners::within(const float p[3], float reach) const {
    const int x = int(std::floor(p[0] / cell_)), y = int(std::floor(p[1] / cell_)), z = int(std::floor(p[2] / cell_));
    const float most = reach * reach;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dz = -1; dz <= 1; ++dz) {
                const auto it = cells_.find(key(x + dx, y + dy, z + dz));
                if (it == cells_.end()) continue;
                for (const auto& c : it->second) {
                    const float ax = c[0] - p[0], ay = c[1] - p[1], az = c[2] - p[2];
                    if (ax * ax + ay * ay + az * az <= most) return true;
                }
            }
    return false;
}

}  // namespace uagfx::gamemodels
