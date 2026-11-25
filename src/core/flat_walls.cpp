#include "core/flat_walls.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::flatwalls {
namespace {

struct Corner {
    float c[3];  // in the view
};

own_ground::Tl1 tl1(const camera::Projection& p, const float c[3], uint32_t colour) {
    own_ground::Tl2 t{};
    own_ground::placeView(p, c, t);
    own_ground::Tl1 o{};
    o.x = t.x, o.y = t.y, o.z = t.z, o.rhw = t.rhw;
    o.diffuse = colour;
    o.specular = 0xFF000000u;
    return o;
}

}  // namespace

void place(const terrain::Mesh& walls, const own_ground::View& v, const camera::Projection& p, const map::Axes& axes,
           const Look& look, std::vector<own_ground::Tl1>& out) {
    const float sign[3] = {float(axes.x), float(axes.y), float(axes.z)};
    const float nearZ = p.nearZ + 0.5f;  // a little past the plane: no rhw blown up at it
    for (size_t i = 0; i + 2 < walls.indices.size(); i += 3) {
        Corner k[3];
        const terrain::Vertex* first = nullptr;
        bool ok = true;
        for (int j = 0; j < 3; ++j) {
            const uint32_t at = walls.indices[i + size_t(j)];
            if (at >= walls.vertices.size()) {
                ok = false;
                break;
            }
            const terrain::Vertex& vx = walls.vertices[at];
            if (!first) first = &vx;
            const float w[3] = {vx.pos[0] * sign[0], vx.pos[1] * sign[1], vx.pos[2] * sign[2]};
            own_ground::toView(v, w, k[j].c);
        }
        if (!ok || !first) continue;
        // the side's light: its normal (by the axes) against a way from the upper left, both sides alike
        const float n[3] = {first->normal[0] * sign[0], first->normal[2] * sign[2]};
        const float turn = std::fabs(n[0] * 0.8f + n[1] * 0.6f);
        const float light = std::clamp(look.ambient + look.diffuse * turn, 0.0f, 1.0f);
        uint32_t colour = 0xFF000000u;
        for (int c = 0; c < 3; ++c)
            colour |= uint32_t(std::lround(std::clamp(look.rgb[c] * light, 0.0f, 1.0f) * 255.0f)) << (16 - 8 * c);
        // cut at the near plane (a polygon of up to four corners), then a fan of triangles
        Corner poly[4];
        int m = 0;
        for (int j = 0; j < 3; ++j) {
            const Corner& a = k[j];
            const Corner& b = k[(j + 1) % 3];
            const bool aIn = a.c[2] >= nearZ, bIn = b.c[2] >= nearZ;
            if (aIn) poly[m++] = a;
            if (aIn != bIn) {
                const float t = (nearZ - a.c[2]) / (b.c[2] - a.c[2]);
                Corner x;
                for (int q = 0; q < 3; ++q) x.c[q] = a.c[q] + (b.c[q] - a.c[q]) * t;
                poly[m++] = x;
            }
        }
        for (int j = 1; j + 1 < m; ++j) {
            out.push_back(tl1(p, poly[0].c, colour));
            out.push_back(tl1(p, poly[j].c, colour));
            out.push_back(tl1(p, poly[j + 1].c, colour));
        }
    }
}

}  // namespace uagfx::flatwalls
