#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

// The game's two-texture vertex (FVF_TL2): its screen place, colours and both texture coordinates.
struct Tl2 {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u0, v0, u1, v1;
};
static_assert(sizeof(Tl2) == 40, "the game's FVF_TL2 vertex");

constexpr size_t kMostGround = 60000;  // triangles a frame

}  // namespace

// 1.9: a draw of the ground (two texture coordinates, writing depth): its triangles back into the world, for the
// seams (applySeams), with its state and textures as a group.
void Renderer::Impl::noteGround(const FrameRecord& rec, const DrawCmd& d) {
    if (groundTris.size() >= kMostGround) return;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    if (d.vertexOffset + size_t(d.vertexCount) * sizeof(Tl2) > vb.size()) return;
    const Tl2* v = reinterpret_cast<const Tl2*>(vb.data() + d.vertexOffset);
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const auto world = [&](const Tl2& t, seams::Vertex& out) {
        if (t.rhw <= 0.0f) return false;
        const float vz = 1.0f / t.rhw, vx = (t.x - p.cx) * vz / p.f0, vy = (t.y - p.cy) * vz / p.f1;
        out.pos[0] = vx * m[0] + vy * m[3] + vz * m[6] + m[9];
        out.pos[1] = vx * m[1] + vy * m[4] + vz * m[7] + m[10];
        out.pos[2] = vx * m[2] + vy * m[5] + vz * m[8] + m[11];
        out.diffuse = t.diffuse;
        out.specular = t.specular;
        out.uv0[0] = t.u0;
        out.uv0[1] = t.v0;
        out.uv1[0] = t.u1;
        out.uv1[1] = t.v1;
        return true;
    };
    const uint32_t group = uint32_t(groundGroups.size());
    GroundGroup g;
    g.state = rec.states()[d.state];
    g.texture[0] = d.texture[0];
    g.texture[1] = d.texture[1];
    g.keyed[0] = d.keyed[0];
    g.keyed[1] = d.keyed[1];
    groundGroups.push_back(g);
    groundTextures.push_back(d.texture[0]);
    const std::vector<uint16_t>& ix = rec.indices();
    const uint32_t n = d.indexCount ? d.indexCount : d.vertexCount;
    const auto vertexAt = [&](uint32_t k, uint32_t& out) {
        if (!d.indexCount) {
            out = k;
            return k < d.vertexCount;
        }
        if (d.firstIndex + k >= ix.size()) return false;
        out = ix[d.firstIndex + k];
        return out < d.vertexCount;
    };
    const bool strip = d.primitive == d3d7::PT_TRIANGLESTRIP;
    const uint32_t triangles = strip ? (n >= 3 ? n - 2 : 0) : n / 3;
    for (uint32_t t = 0; t < triangles && groundTris.size() < kMostGround; ++t) {
        uint32_t a = 0, b = 0, c = 0;
        const uint32_t base = strip ? t : t * 3;
        if (!vertexAt(base, a) || !vertexAt(base + 1, b) || !vertexAt(base + 2, c)) break;
        seams::Triangle tri;
        tri.group = group;
        if (!world(v[a], tri.v[0]) || !world(v[b], tri.v[1]) || !world(v[c], tri.v[2])) continue;
        groundTris.push_back(tri);
    }
}

// 1.9: the strips across the ground's seams (core/seams.hpp), drawn as the game's ground is - its pipeline, its
// textures, the AI's pictures, the materials - blended over it by the noise (shade(), bit 31), before the soft
// shadows and the grass. A record of their own: the game's geometry goes up again with its next draw.
bool Renderer::Impl::applySeams() {
    seamsDone = true;
    crumb("ground seams");
    const std::vector<seams::Strip> strips = seams::build(groundTris, groundTextures, 0.45f);
    if (strips.empty()) return true;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return true;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    seamRec.reset();
    std::vector<Tl2> verts;
    size_t drawn = 0;
    for (const seams::Strip& s : strips) {
        if (s.inside >= groundGroups.size() || s.shows >= groundGroups.size()) continue;
        const GroundGroup& in = groundGroups[s.inside];
        const GroundGroup& shows = groundGroups[s.shows];
        DrawSetup st = in.state;
        st.blend = true;
        st.srcBlend = d3d7::BLEND_SRCALPHA;
        st.dstBlend = d3d7::BLEND_INVSRCALPHA;
        st.zWrite = false;
        st.zFunc = d3d7::CMP_LESSEQUAL;
        st.cull = d3d7::CULL_NONE;
        st.alphaTest = false;
        DrawCmd cmd;
        cmd.state = seamRec.addState(st);
        cmd.texture[0] = shows.texture[0];
        cmd.keyed[0] = shows.keyed[0];
        cmd.texture[1] = in.texture[1];
        cmd.keyed[1] = in.keyed[1];
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL2;
        verts.clear();
        bool behind = false;
        for (const seams::Vertex& v : s.vertices) {
            const float x = v.pos[0] - m[9], y = v.pos[1] - m[10], z = v.pos[2] - m[11];
            const float vx = x * inv[0] + y * inv[3] + z * inv[6], vy = x * inv[1] + y * inv[4] + z * inv[7];
            const float vz = x * inv[2] + y * inv[5] + z * inv[8];
            if (vz <= p.nearZ) behind = true;
            Tl2 t{};
            const float zv = std::max(vz, p.nearZ + 0.01f);
            t.x = vx / zv * p.f0 + p.cx;
            t.y = vy / zv * p.f1 + p.cy;
            // the game's depth, a little towards the camera: the strip lies on its cell's surface, not under it
            const float zd = std::max(zv - 0.08f, p.nearZ + 0.005f);
            t.z = p.farZ * (1.0f - p.nearZ / zd) / (p.farZ - p.nearZ);
            t.rhw = 1.0f / zv;
            t.diffuse = v.diffuse;
            t.specular = v.specular;
            t.u0 = v.uv0[0];
            t.v0 = v.uv0[1];
            t.u1 = v.uv1[0];
            t.v1 = v.uv1[1];
            verts.push_back(t);
        }
        if (behind || verts.empty()) continue;
        seamRec.draw(cmd, verts.data(), uint32_t(verts.size()), s.indices.data(), uint32_t(s.indices.size()));
        ++drawn;
    }
    if (!drawn) return true;
    prepareGeometry(seamRec);
    if (vertexBase) {
        seamPass = true;
        for (const DrawCmd& cmd : seamRec.draws()) draw(seamRec, cmd);
        seamPass = false;
    }
    geometryList = 0;  // the game's record's vertices go up again with its next draw
    if (!seamsLogged)
        LOG_INFO("ground seams: %zu kinds of strips over %zu triangles of %s's ground", drawn, groundTris.size(),
                 mapName.c_str());
    seamsLogged = true;
    return !lost;
}

}  // namespace uagfx::gpu
