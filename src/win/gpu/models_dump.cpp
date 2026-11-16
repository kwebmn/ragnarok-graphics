#include "win/gpu/models_dump.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/game_models_card.hpp"
#include "core/game_models_place.hpp"
#include "core/own_ground.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {

namespace {

constexpr float kDumpReach = 150.0f;  // the world's units: 30 cells (the driver's own)
constexpr size_t kDumpMost = 60;
constexpr float kDumpAhead = 800.0f;  // 3.0.20-beta.55: the map's models in front of the eye so far
constexpr size_t kDumpAheadMost = 40;

// a node's place on the ground: its corners' least and most x and z (a still one's are the world's)
struct Stand {
    float x0 = 1e30f, z0 = 1e30f, x1 = -1e30f, z1 = -1e30f;
    uint32_t id = 0, faces = 0;
    bool holds(float x, float z) const { return x >= x0 - 2.0f && x <= x1 + 2.0f && z >= z0 - 2.0f && z <= z1 + 2.0f; }
};

// a map's file name (CP949 bytes) in UTF-8 for the log
std::string utf8Of949(const std::string& bytes) {
    const int n = MultiByteToWideChar(949, 0, bytes.data(), int(bytes.size()), nullptr, 0);
    if (n <= 0) return bytes;
    std::wstring wide(size_t(n), L'\0');
    MultiByteToWideChar(949, 0, bytes.data(), int(bytes.size()), &wide[0], n);
    const int m = WideCharToMultiByte(CP_UTF8, 0, wide.data(), n, nullptr, 0, nullptr, nullptr);
    if (m <= 0) return bytes;
    std::string out(size_t(m), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), n, &out[0], m, nullptr, nullptr);
    return out;
}

bool nearPlayer(const float lo[3], const float hi[3], const float at[2]) {
    const float cx = (lo[0] + hi[0]) / 2, cz = (lo[2] + hi[2]) / 2;
    return std::fabs(cx - at[0]) <= kDumpReach && std::fabs(cz - at[1]) <= kDumpReach;
}

}  // namespace

void dumpModelsAround(const gamemodels::Frame& frame, const NodeOf& nodeOf, const CardFacesOf& cardFaces,
                      const float viewToWorld[12], const camera::Projection& p, const rsw::World* map,
                      const map::Axes& axes, const AllNodes& allNodes) {
    gamemodels::Picture pic;
    const bool viewOk = own_ground::viewOf(viewToWorld, pic.view);
    pic.p = p;
    pic.how = gamemodels::colouringOf(frame);
    pic.plain = 0xFF000000u;
    LOG_INFO("models dump (the renderer): %zu still and %zu moving nodes of ours this frame; those within %.0f of the "
             "world's x %.0f, z %.0f:",
             frame.ids.size(), frame.moving.size(), kDumpReach, frame.dumpAt[0], frame.dumpAt[1]);
    gamemodels::Placed out;
    std::vector<gamemodels::Corner> scratch;
    size_t told = 0, unknown = 0;
    std::vector<Stand> stands;  // 3.0.20-beta.55: where this frame's nodes stand
    // 3.0.20-beta.65 (R-64G2U9: sixty lines of cherry petals and paving, the house not among them): the near ones
    // gathered first, then told the largest first (by their faces)
    struct Near {
        uint32_t id;
        bool still;
        const gamemodels::Node* node;
        const float* toView;
        float lo[3], hi[3];
    };
    std::vector<Near> nears;
    for (size_t i = 0; i < frame.ids.size() + frame.moving.size(); ++i) {
        const bool still = i < frame.ids.size();
        const size_t k = still ? 0 : i - frame.ids.size();
        const uint32_t id = still ? frame.ids[i] : frame.moving[k];
        const gamemodels::Node* n = nodeOf(id);
        if (!n || n->vertices.empty()) {
            ++unknown;
            continue;
        }
        const float* toView = !still && frame.matrices.size() >= 12 * (k + 1) ? &frame.matrices[12 * k] : nullptr;
        float toWorld[12];
        if (toView) gamemodels::compose(toView, viewToWorld, toWorld);
        Near at{id, still, n, toView, {1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}};
        for (size_t v = 0; v + 2 < n->vertices.size(); v += 3) {
            float w[3] = {n->vertices[v], n->vertices[v + 1], n->vertices[v + 2]};
            if (toView) gamemodels::transform(toWorld, &n->vertices[v], w);
            for (int a = 0; a < 3; ++a) at.lo[a] = std::min(at.lo[a], w[a]), at.hi[a] = std::max(at.hi[a], w[a]);
        }
        Stand here;
        here.x0 = at.lo[0], here.x1 = at.hi[0], here.z0 = at.lo[2], here.z1 = at.hi[2];
        here.id = id;
        stands.push_back(here);
        if (nearPlayer(at.lo, at.hi, frame.dumpAt)) nears.push_back(at);
    }
    std::stable_sort(nears.begin(), nears.end(),
                     [](const Near& a, const Near& b) { return a.node->faces.size() > b.node->faces.size(); });
    for (const Near& at : nears) {
        if (told >= kDumpMost) break;
        ++told;
        const gamemodels::Node* n = at.node;
        uint32_t shown = 0;
        size_t front = 0;
        if (viewOk) {
            out.clear();
            gamemodels::place(*n, pic, scratch, out, nullptr, at.toView);
            shown = uint32_t(out.triangles);
            for (size_t v = 0; v < n->vertices.size() / 3 && v < scratch.size(); ++v) front += scratch[v].ok;
        }
        size_t known = 0;
        for (const uint32_t t : n->textures) known += t != 0;
        LOG_INFO("models dump: ours %08X %s - %zu faces (%u kept on the card, %zu of %zu textures known), %u shown now "
                 "by the card's rules, %zu of %zu corners in front%s%s; x %.0f..%.0f, z %.0f..%.0f, y %.0f..%.0f",
                 at.id, at.still ? "still" : "moving", n->faces.size(), cardFaces(at.id), known, n->textures.size(),
                 shown, front, n->vertices.size() / 3, n->flipped ? ", mirrored" : "",
                 n->addedFaces ? (", " + std::to_string(n->addedFaces) + " of its faces ours, closing its gaps").c_str()
                               : "",
                 at.lo[0], at.hi[0], at.lo[2], at.hi[2], at.lo[1], at.hi[1]);
    }
    if (nears.size() > told)
        LOG_INFO("models dump: %zu more nodes of ours near the player, smaller ones, not written", nears.size() - told);
    if (unknown) LOG_INFO("models dump: %zu of the frame's nodes unknown to the renderer", unknown);
    if (!map) return;
    size_t models = 0;
    for (const rsw::Model& m : map->models) {
        const float x = m.pos[0] * float(axes.x), z = m.pos[2] * float(axes.z), y = m.pos[1] * float(axes.y);
        const float at[3] = {x, y, z};
        if (!nearPlayer(at, at, frame.dumpAt)) continue;
        if (++models > kDumpMost) break;
        LOG_INFO("models dump: the map's %s at x %.0f, z %.0f, y %.0f - turn %.0f %.0f %.0f, scale %.2f %.2f %.2f",
                 utf8Of949(m.file).c_str(), x, z, y, m.rot[0], m.rot[1], m.rot[2], m.scale[0], m.scale[1],
                 m.scale[2]);
    }
    // 3.0.20-beta.55: the map's models in front of the eye that no node of this frame stands at
    std::vector<Stand> kept;  // every still node the renderer keeps
    if (allNodes)
        allNodes([&kept](const gamemodels::Node& n) {
            if (n.moving || n.vertices.size() < 3) return;
            Stand s;
            for (size_t v = 0; v + 2 < n.vertices.size(); v += 3) {
                s.x0 = std::min(s.x0, n.vertices[v]), s.x1 = std::max(s.x1, n.vertices[v]);
                s.z0 = std::min(s.z0, n.vertices[v + 2]), s.z1 = std::max(s.z1, n.vertices[v + 2]);
            }
            s.id = n.id;
            s.faces = uint32_t(n.faces.size());
            kept.push_back(s);
        });
    const float* v = viewToWorld;  // rows: the view's right, up and forward in the world, then the eye
    size_t ahead = 0, drawn = 0, missing = 0, written = 0;
    // 3.0.20-beta.65 (R-64G2U9, R-7VX6Q9): those nearest the player first - from above the cone holds a thousand
    // models, the forty written were the map's first ones far off the picture
    struct Missing {
        float away, x, z, forward, aside;
        const rsw::Model* model;
    };
    std::vector<Missing> missed;
    for (const rsw::Model& m : map->models) {
        const float x = m.pos[0] * float(axes.x), z = m.pos[2] * float(axes.z), y = m.pos[1] * float(axes.y);
        const float d[3] = {x - v[9], y - v[10], z - v[11]};
        const float forward = d[0] * v[6] + d[1] * v[7] + d[2] * v[8];
        const float aside = d[0] * v[0] + d[1] * v[1] + d[2] * v[2];
        if (!(forward > 10.0f && forward < kDumpAhead) || std::fabs(aside) > forward * 1.2f + 30.0f) continue;
        ++ahead;
        bool here = false;
        for (const Stand& s : stands) here = here || s.holds(x, z);
        if (here) {
            ++drawn;
            continue;
        }
        ++missing;
        missed.push_back(Missing{std::hypot(x - frame.dumpAt[0], z - frame.dumpAt[1]), x, z, forward, aside, &m});
    }
    std::sort(missed.begin(), missed.end(), [](const Missing& a, const Missing& b) { return a.away < b.away; });
    for (const Missing& lost : missed) {
        if (written >= kDumpAheadMost) break;
        ++written;
        std::string before;
        size_t others = 0;
        for (const Stand& s : kept) {
            if (!s.holds(lost.x, lost.z)) continue;
            char one[48];
            std::snprintf(one, sizeof one, "%s%08X (%u faces)", before.empty() ? "" : ", ", s.id, s.faces);
            if (others++ < 4) before += one;
        }
        LOG_INFO("models dump: NO NODE of this frame at the map's %s - x %.0f, z %.0f, %.0f from the player (%.0f "
                 "ahead, %.0f aside); the renderer keeps %s",
                 utf8Of949(lost.model->file).c_str(), lost.x, lost.z, lost.away, lost.forward, lost.aside,
                 before.empty() ? "no node there at all: never captured"
                                : ("these there from before, not in this frame's list: " + before).c_str());
    }
    LOG_INFO("models dump: of the map's %zu models %zu stand in front of the eye within %.0f - a node of this frame "
             "at %zu of them, none at %zu%s", map->models.size(), ahead, kDumpAhead, drawn, missing,
             missing > written ? " (those nearest the player written above)" : missing ? " (written above)" : "");
}

}  // namespace uagfx::gpu
