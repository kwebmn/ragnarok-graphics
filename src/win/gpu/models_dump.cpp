#include "win/gpu/models_dump.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
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
                      const map::Axes& axes) {
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
    for (size_t i = 0; i < frame.ids.size() + frame.moving.size() && told < kDumpMost; ++i) {
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
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (size_t v = 0; v + 2 < n->vertices.size(); v += 3) {
            float w[3] = {n->vertices[v], n->vertices[v + 1], n->vertices[v + 2]};
            if (toView) gamemodels::transform(toWorld, &n->vertices[v], w);
            for (int a = 0; a < 3; ++a) lo[a] = std::min(lo[a], w[a]), hi[a] = std::max(hi[a], w[a]);
        }
        if (!nearPlayer(lo, hi, frame.dumpAt)) continue;
        ++told;
        uint32_t shown = 0;
        size_t front = 0;
        if (viewOk) {
            out.clear();
            gamemodels::place(*n, pic, scratch, out, nullptr, toView);
            shown = uint32_t(out.triangles);
            for (size_t v = 0; v < n->vertices.size() / 3 && v < scratch.size(); ++v) front += scratch[v].ok;
        }
        size_t known = 0;
        for (const uint32_t t : n->textures) known += t != 0;
        LOG_INFO("models dump: ours %08X %s - %zu faces (%u kept on the card, %zu of %zu textures known), %u shown now "
                 "by the card's rules, %zu of %zu corners in front%s; x %.0f..%.0f, z %.0f..%.0f, y %.0f..%.0f",
                 id, still ? "still" : "moving", n->faces.size(), cardFaces(id), known, n->textures.size(), shown,
                 front, n->vertices.size() / 3, n->flipped ? ", mirrored" : "", lo[0], hi[0], lo[2], hi[2], lo[1],
                 hi[1]);
    }
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
}

}  // namespace uagfx::gpu
