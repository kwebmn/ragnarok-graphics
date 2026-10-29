#include "win/ground_way.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/game_camera.hpp"
#include "core/ground_way.hpp"
#include "core/minimap_proto.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::groundway {
namespace {

constexpr float kLift = 0.5f;  // world units over the ground
constexpr float kBias = 1.5f;  // its depth that much nearer the camera: over the ground's own bumps, under the rest
constexpr float kWidth = 1.6f, kEdge = 2.8f;  // the gold, its dark edge (a cell is 5)
constexpr uint32_t kGold = 0xEBF2C14E, kShade = 0x66000000;

struct State {
    std::string map;  // the cells' map
    std::vector<std::pair<int, int>> cells;
    bool drawn = false;  // this frame
    std::string heightsMap;  // the .gat read for
    uagfx::groundway::Heights heights;
    bool heightsRead = false;
    float sign = 0;           // the .gat's heights to the game's world y (0: not told yet, NaN-free)
    bool off = false;         // the heights do not fit the player's own: not drawn on this map
    std::vector<uagfx::groundway::Point> points;  // made when the cells change
    bool pointsMade = false;
    int cellSize = 0;
    std::unique_ptr<gamefiles::Reader> files;
    std::vector<uagfx::groundway::Vertex> verts;
    int drawsLogged = 0;
};

State& state() {
    static State s;
    return s;
}

DrawSetup stripSetup() {  // tested against the world's depth, not written; blended; untextured, its colour its own
    DrawSetup s;
    s.zEnable = true;
    s.zWrite = false;
    s.zFunc = d3d7::CMP_LESSEQUAL;
    s.cull = d3d7::CULL_NONE;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_SRCALPHA;
    s.dstBlend = d3d7::BLEND_INVSRCALPHA;
    s.stage[0].colorOp = s.stage[0].alphaOp = d3d7::TOP_SELECTARG1;
    s.stage[0].colorArg1 = s.stage[0].alphaArg1 = d3d7::TA_DIFFUSE;
    s.stage[1].colorOp = s.stage[1].alphaOp = d3d7::TOP_DISABLE;
    return s;
}

void readHeights(State& s) {
    s.heightsMap = s.map;
    s.heightsRead = false;
    s.sign = 0;
    s.off = false;
    s.heights = uagfx::groundway::Heights{};
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    std::string from;
    const ULONGLONG t0 = GetTickCount64();
    if (s.files->read("data\\" + s.map + ".gat", bytes, &from) &&
        uagfx::groundway::parseGat(std::string(bytes.begin(), bytes.end()), s.heights)) {
        s.heightsRead = true;
        LOG_INFO("ground way: %s's heights (%dx%d cells) from %s in %llu ms", s.map.c_str(), s.heights.w,
                 s.heights.h, from.c_str(), static_cast<unsigned long long>(GetTickCount64() - t0));
    } else {
        LOG_INFO("ground way: %s has no .gat to read - the way on the minimap alone", s.map.c_str());
    }
}

// The .gat's heights against the player's own height: the same way round (or the other) within 3 units - else the
// way is not drawn on this map (the log says so)
void tell(State& s, const float at[3], int width, int height, int cellSize) {
    minimap::Cell c;
    float h = 0;  // at the player's very place in the cell (on a slope its middle is not)
    if (!minimap::cellOf(at[0], at[2], width, height, cellSize, c) ||
        !s.heights.at(at[0] / float(cellSize) + float(width / 2), at[2] / float(cellSize) + float(height / 2), h))
        return;
    if (std::fabs(at[1] - h) <= 3.0f) s.sign = 1;
    else if (std::fabs(at[1] + h) <= 3.0f) s.sign = -1;
    else s.off = true;
    LOG_INFO("ground way: the player on %s at %d, %d stands at y %.2f, its cell's .gat says %.2f - %s", s.map.c_str(),
             c.x, c.y, at[1], h,
             s.off ? "they differ: the way not drawn on the ground here"
                   : s.sign > 0 ? "the same: drawn on the ground" : "the other way round: drawn on the ground");
}

}  // namespace

void set(const std::string& map, const std::vector<std::pair<int, int>>& cells) {
    State& s = state();
    if (map == s.map && cells == s.cells) return;
    s.map = map;
    s.cells = cells;
    s.pointsMade = false;
}

void newFrame() { state().drawn = false; }

void draw(FrameRecord& record, int width, int height) {
    State& s = state();
    if (s.drawn || s.cells.size() < 2 || width <= 0 || height <= 0) return;
    s.drawn = true;
    if (s.heightsMap != s.map) readHeights(s);
    if (!s.heightsRead || s.off) return;
    float at[3];
    int w = 0, h = 0, cellSize = 0;
    if (!readPlayerWorld(at, w, h, cellSize) || w != s.heights.w || h != s.heights.h) return;
    if (s.sign == 0) {
        tell(s, at, w, h, cellSize);
        if (s.sign == 0) return;
    }
    if (!s.pointsMade || cellSize != s.cellSize) {
        s.points = uagfx::groundway::along(s.cells, s.heights, cellSize, kLift, s.sign);
        s.pointsMade = true;
        s.cellSize = cellSize;
    }
    float view[12];
    if (s.points.size() < 2 || !readGameCamera(view)) return;
    const camera::Projection p = camera::projectionFor(width, height);
    s.verts.clear();
    const size_t shade = uagfx::groundway::strip(s.points, view, p, kEdge, kShade, kBias, s.verts);
    uagfx::groundway::strip(s.points, view, p, kWidth, kGold, kBias + 0.2f, s.verts);
    if (s.verts.empty()) return;
    DrawCmd cmd;
    cmd.state = record.addState(stripSetup());
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.effect = uint8_t(effect_exe::Kind::None);
    record.draw(cmd, s.verts.data(), uint32_t(s.verts.size()), nullptr, 0);
    if (s.drawsLogged < 2) {
        ++s.drawsLogged;
        LOG_INFO("ground way: drawn on %s - %zu points, %zu triangles (the edge's %zu)", s.map.c_str(),
                 s.points.size(), s.verts.size() / 3, shade / 3);
    }
}

}  // namespace uagfx::win::groundway
