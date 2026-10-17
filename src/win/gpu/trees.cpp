#include <windows.h>

#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"
#include "core/daylight.hpp"
#include "core/game_camera.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

// The game's one-texture vertex (FVF_TL1).
struct Tl1 {
    float x, y, z, rhw;
    uint32_t diffuse, specular;
    float u, v;
};
static_assert(sizeof(Tl1) == 32, "the game's FVF_TL1 vertex");

}  // namespace

// 1.10: a draw of leaves (a crown) - its box in the world, its state and texture, kept for applyTrees; false when it is
// none of the kind (no leaves picture, the camera unknown, a card too small to be a tree).
bool Renderer::Impl::takeTree(const FrameRecord& rec, const DrawCmd& d) {
    if (!d.texture[0] || frameTrees.size() >= 96) return false;
    const auto it = textures.find(d.texture[0]);
    if (it == textures.end() || !it->second.leafy || !it->second.leaves.any) return false;
    const std::vector<uint8_t>& vb = rec.vertexBytes();
    const size_t stride = FrameRecord::stride(d.fvf);
    if (d.vertexOffset + size_t(d.vertexCount) * stride > vb.size() || d.vertexCount < 3) return false;
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    TreeCrown c;
    for (int k = 0; k < 3; ++k) {
        c.lo[k] = 1e30f;
        c.hi[k] = -1e30f;
    }
    for (uint32_t i = 0; i < d.vertexCount; ++i) {
        float xyzw[4];
        std::memcpy(xyzw, vb.data() + d.vertexOffset + size_t(i) * stride, sizeof xyzw);
        if (xyzw[3] <= 0.0f) return false;
        const float vz = 1.0f / xyzw[3], vx = (xyzw[0] - p.cx) * vz / p.f0, vy = (xyzw[1] - p.cy) * vz / p.f1;
        const float w[3] = {vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                            vx * m[2] + vy * m[5] + vz * m[8] + m[11]};
        for (int k = 0; k < 3; ++k) {
            c.lo[k] = std::min(c.lo[k], w[k]);
            c.hi[k] = std::max(c.hi[k], w[k]);
        }
    }
    const float across = std::max(c.hi[0] - c.lo[0], c.hi[2] - c.lo[2]);
    if (across < 2.0f || across > 80.0f) return false;  // a sprig, or a whole hedge row: as the game draws it
    c.state = rec.states()[d.state];
    c.texture = d.texture[0];
    c.keyed = d.keyed[0];
    frameTrees.push_back(c);
    return true;
}

// 1.10: the new trees (core/new_trees.hpp) - each crown kept this frame as bunches of its leaves, cards facing the
// camera, drawn as the game drew the crown (its state and texture; the AI's picture of it; the diffuse its light).
bool Renderer::Impl::applyTrees() {
    treesDone = true;
    crumb("new trees");
    const camera::Projection p = camera::projectionFor(gameWidth, gameHeight);
    const float* m = viewToWorld;
    const float a = m[0], b = m[1], c3 = m[2], d = m[3], e = m[4], g = m[5], h = m[6], i = m[7], k = m[8];
    const float det = a * (e * k - g * i) - b * (d * k - g * h) + c3 * (d * i - e * h);
    if (std::fabs(det) < 1e-8f) return true;
    const float inv[9] = {(e * k - g * i) / det, (c3 * i - b * k) / det, (b * g - c3 * e) / det,
                          (g * h - d * k) / det, (a * k - c3 * h) / det, (c3 * d - a * g) / det,
                          (d * i - e * h) / det, (b * h - a * i) / det, (a * e - b * d) / det};
    // the way to the sun in the world, as the materials find it
    float sun[2];
    sunOnScreen(sun);
    const daylight::Light day = daylight::at(effects.dayCycle ? effects.dayNow : -1);
    const float height = std::clamp(50.0f / day.length * 0.55f, 15.0f, 70.0f) * 0.0174533f;
    float gdir[3];
    for (int q = 0; q < 3; ++q) gdir[q] = sun[0] * m[q] + sun[1] * m[3 + q];
    const float gl = std::max(std::sqrt(gdir[0] * gdir[0] + gdir[2] * gdir[2]), 1e-4f);
    const float toSun[3] = {-gdir[0] / gl * std::cos(height), -std::sin(height), -gdir[2] / gl * std::cos(height)};
    const float sunLight = mapOutdoors ? 1.0f - float(std::clamp(effects.nightNow, 0, 100)) / 100.0f : 0.3f;
    const double now = double(waterTime());
    treeRec.reset();
    struct Batch {
        const TreeCrown* crown;
        std::vector<Tl1> verts;
        std::vector<uint16_t> indices;
    };
    std::vector<Batch> batches;
    std::vector<new_trees::Bunch> bunches;
    size_t total = 0;
    for (const TreeCrown& crown : frameTrees) {
        const auto tex = textures.find(crown.texture);
        if (tex == textures.end()) continue;
        bunches.clear();
        new_trees::bunchesOf(crown.lo, crown.hi, tex->second.leaves, now, toSun, sunLight, bunches);
        Batch* into = nullptr;
        for (Batch& bt : batches)
            if (bt.crown->texture == crown.texture && bt.crown->state == crown.state &&
                bt.verts.size() + bunches.size() * 4 <= 60000) {
                into = &bt;
                break;
            }
        if (!into) {
            batches.push_back(Batch{&crown, {}, {}});
            into = &batches.back();
        }
        for (const new_trees::Bunch& bn : bunches) {
            const float x = bn.pos[0] - m[9], y = bn.pos[1] - m[10], z = bn.pos[2] - m[11];
            const float vx = x * inv[0] + y * inv[3] + z * inv[6], vy = x * inv[1] + y * inv[4] + z * inv[7];
            const float vz = x * inv[2] + y * inv[5] + z * inv[8];
            if (vz <= p.nearZ + 1.0f) continue;
            const float sx = vx / vz * p.f0 + p.cx, sy = vy / vz * p.f1 + p.cy;
            const float half = bn.size * 0.5f * p.f0 / vz;
            const float cs = std::cos(bn.turn) * half, sn = std::sin(bn.turn) * half;
            const float zd = p.farZ * (1.0f - p.nearZ / vz) / (p.farZ - p.nearZ);
            const uint32_t l = uint32_t(std::clamp(bn.light * 200.0f, 0.0f, 255.0f));
            const uint32_t colour = 0xff000000u | l << 16 | l << 8 | l;
            const uint16_t base = uint16_t(into->verts.size());
            const float corner[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
            for (int q = 0; q < 4; ++q) {
                Tl1 t{};
                t.x = sx + corner[q][0] * cs - corner[q][1] * sn;
                t.y = sy + corner[q][0] * sn + corner[q][1] * cs;
                t.z = zd;
                t.rhw = 1.0f / vz;
                t.diffuse = colour;
                t.specular = 0xff000000u;  // no fog taken off
                t.u = q & 1 ? bn.uv[2] : bn.uv[0];
                t.v = q & 2 ? bn.uv[3] : bn.uv[1];
                into->verts.push_back(t);
            }
            for (uint16_t q : {0, 1, 2, 2, 1, 3}) into->indices.push_back(uint16_t(base + q));
            ++total;
        }
    }
    size_t drawn = 0;
    for (const Batch& bt : batches) {
        if (bt.verts.empty()) continue;
        DrawSetup st = bt.crown->state;
        st.cull = d3d7::CULL_NONE;
        DrawCmd cmd;
        cmd.state = treeRec.addState(st);
        cmd.texture[0] = bt.crown->texture;
        cmd.keyed[0] = bt.crown->keyed;
        cmd.primitive = d3d7::PT_TRIANGLELIST;
        cmd.fvf = d3d7::FVF_TL1;
        treeRec.draw(cmd, bt.verts.data(), uint32_t(bt.verts.size()), bt.indices.data(), uint32_t(bt.indices.size()));
        ++drawn;
    }
    if (!drawn) return true;
    prepareGeometry(treeRec);
    if (vertexBase) {
        treePass = true;
        for (const DrawCmd& cmd : treeRec.draws()) draw(treeRec, cmd);
        treePass = false;
    }
    geometryList = 0;  // the game's record's vertices go up again with its next draw
    if (!treesLogged)
        LOG_INFO("new trees: %zu crowns of %s drawn as %zu bunches of leaves in %zu draws", frameTrees.size(),
                 mapName.c_str(), total, drawn);
    treesLogged = true;
    return !lost;
}

}  // namespace uagfx::gpu
