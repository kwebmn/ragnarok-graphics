#include "core/grass_beds.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::grass_beds {
namespace {

std::string lower(std::string s) {  // the ASCII letters (the Korean names' cp949 bytes stay)
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

// 나무잡초꽃\둥근꽃밭.rsm and 프론테라\weed.bmp in cp949
const char kRoundBed[] = "\xb3\xaa\xb9\xab\xc0\xe2\xc3\xca\xb2\xc9\\\xb5\xd5\xb1\xd9\xb2\xc9\xb9\xe7.rsm";
const char kWeed[] = "\xc7\xc1\xb7\xd0\xc5\xd7\xb6\xf3\\weed.bmp";

// A turn of p by `degrees` about one axis (0 x, 1 y, 2 z), as the game's matrices turn a model
void turn(float p[3], int axis, float degrees) {
    if (degrees == 0.0f) return;
    const float r = degrees * 3.14159265f / 180.0f, c = std::cos(r), s = std::sin(r);
    const int a = (axis + 1) % 3, b = (axis + 2) % 3;  // y: z, x - so x' = c x + s z, z' = -s x + c z
    const float u = p[a], v = p[b];
    p[a] = c * u - s * v;
    p[b] = s * u + c * v;
}

}  // namespace

bool bedModel(const std::string& file) { return lower(file) == kRoundBed; }

bool bedTexture(const std::string& texture) { return lower(texture) == kWeed; }

std::vector<Box> place(const rsm::Model& model, const rsw::Model& at) {
    std::vector<Box> out;
    for (const rsm::Part& part : model.parts) {
        if (!bedTexture(part.texture)) continue;
        Box b;
        for (int a = 0; a < 3; ++a) b.lo[a] = 1e30f, b.hi[a] = -1e30f;
        for (int k = 0; k < 8; ++k) {  // its corners: scaled, turned about y, x and z, put at the place
            float p[3] = {(k & 1) ? part.hi[0] : part.lo[0], (k & 2) ? part.hi[1] : part.lo[1],
                          (k & 4) ? part.hi[2] : part.lo[2]};
            for (int a = 0; a < 3; ++a) p[a] *= at.scale[a];
            turn(p, 1, at.rot[1]);
            turn(p, 0, at.rot[0]);
            turn(p, 2, at.rot[2]);
            for (int a = 0; a < 3; ++a) {
                p[a] += at.pos[a];
                b.lo[a] = std::min(b.lo[a], p[a]);
                b.hi[a] = std::max(b.hi[a], p[a]);
            }
        }
        out.push_back(b);
    }
    return out;
}

std::vector<Box> inWorld(const std::vector<Box>& file, const map::Axes& axes) {
    std::vector<Box> out;
    if (!axes.known()) return out;
    const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
    for (const Box& f : file) {
        Box w;
        for (int a = 0; a < 3; ++a) {
            w.lo[a] = std::min(f.lo[a] * sign[a], f.hi[a] * sign[a]);
            w.hi[a] = std::max(f.lo[a] * sign[a], f.hi[a] * sign[a]);
        }
        out.push_back(w);
    }
    return out;
}

void addTo(ground_cells::Grid& g, const std::vector<Box>& beds) {
    if (g.side <= 0.0f || g.span.size() < size_t(g.width) * size_t(g.height) * 2) return;
    for (const Box& b : beds) {
        const int x0 = std::max(0, int(std::floor((b.lo[0] - g.originX) / g.side)));
        const int x1 = std::min(g.width - 1, int(std::floor((b.hi[0] - g.originX) / g.side)));
        const int z0 = std::max(0, int(std::floor((b.lo[2] - g.originZ) / g.side)));
        const int z1 = std::min(g.height - 1, int(std::floor((b.hi[2] - g.originZ) / g.side)));
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x) {
                float* s = &g.span[(size_t(z) * size_t(g.width) + size_t(x)) * 2];
                s[0] = std::min(s[0], b.lo[1]);  // a cell with no ground (lowest > highest) takes the bed's alone
                s[1] = std::max(s[1], b.hi[1]);
            }
    }
}

}  // namespace uagfx::grass_beds
