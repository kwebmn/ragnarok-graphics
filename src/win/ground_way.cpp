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
#include "core/walk_path.hpp"
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
// 3.0.11-beta.33: the footprints - a stride apart, to each side of the way, the first clear of the player's feet, none
// farther than the camera shows (60 cells), their dark edge all round.
// 3.0.11-beta.34 (the owner: «можно чтобы на локации шажки были синенькие», «каждый раз перестраивается линия, можно
// так чтобы она не очень дергалась»): blue (the minimap's way's blue before the concept's gold), a dark blue edge; the
// ones the player comes to fade out under their feet, the far ones fade in; a way made again fades out as the new
// one comes in
constexpr float kStride = 3.2f, kApart = 0.75f, kSkip = 2.5f, kMost = 300.0f, kPrintEdge = 0.22f, kPrintLift = 0.3f;
constexpr float kFade = 3.5f;  // world units
constexpr ULONGLONG kChangeMs = 350;
constexpr uint32_t kPrintBlue = 0xE65AC8FA, kPrintShade = 0x5A061830;

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
    WayOnGround way = WayOnGround::Steps;  // 3.0.11-beta.33
    std::vector<uagfx::groundway::Print> prints;  // made with the points
    // 3.0.11-beta.34: the way kept (win/minimap_proto) cut at the player's place on it - how far along it they are,
    // looked for near where they were; what this frame shows; what was shown before the way was made again, fading
    std::vector<std::pair<float, float>> flat;  // the points' x, z
    float at = 0;
    std::vector<uagfx::groundway::Print> shown, was;
    std::vector<uint8_t> fresh;  // the way's prints not where the one before had its own: these fade in
    std::vector<uagfx::groundway::Point> line, wasLine;
    float lineAlpha = 1, wasLineAlpha = 1;
    ULONGLONG changedAt = 0;
    int changesLogged = 0;
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
    if (map != s.map) {  // 3.0.11-beta.34: another map - nothing of the way before fades on it
        s.shown.clear();
        s.line.clear();
        s.was.clear();
        s.wasLine.clear();
    }
    s.map = map;
    s.cells = cells;
    s.pointsMade = false;
}

void look(WayOnGround way) {
    State& s = state();
    if (way == s.way) return;
    s.way = way;
    s.shown.clear();  // 3.0.11-beta.34: nothing of the look before fades in the new one
    s.line.clear();
    s.was.clear();
    s.wasLine.clear();
    s.drawsLogged = 0;  // the next draws logged again in the new look
    LOG_INFO("ground way: drawn as %s", wayOnGroundToken(way));
}

void newFrame() { state().drawn = false; }

void draw(FrameRecord& record, int width, int height) {
    State& s = state();
    if (s.drawn || s.way == WayOnGround::Off || width <= 0 || height <= 0) return;
    if (s.cells.size() < 2 && s.shown.empty() && s.line.empty() && s.was.empty() && s.wasLine.empty()) return;
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
    const ULONGLONG now = GetTickCount64();
    if (!s.pointsMade || cellSize != s.cellSize) {
        // 3.0.11-beta.34: the way made again (another one, or none): what was shown fades out as the new comes in
        // (made again while the one before it still fades: that one goes on from where it is)
        const float before = std::min(1.0f, float(now - s.changedAt) / float(kChangeMs));
        std::vector<uagfx::groundway::Print> was;
        for (uagfx::groundway::Print q : before < 1 ? s.was : std::vector<uagfx::groundway::Print>{}) {
            q.alpha *= 1 - before;
            was.push_back(q);
        }
        was.insert(was.end(), s.shown.begin(), s.shown.end());
        s.was.swap(was);
        s.wasLine = s.line;
        s.wasLineAlpha = s.lineAlpha;
        s.changedAt = now;
        s.points = s.cells.size() >= 2 ? uagfx::groundway::along(s.cells, s.heights, cellSize, kLift, s.sign)
                                       : std::vector<uagfx::groundway::Point>{};
        s.prints = uagfx::groundway::prints(s.points, kStride, kApart);
        // the ones on the same places as the ones shown (a way made again to the same goal shares its end): kept as
        // they are, neither out nor in
        s.fresh.assign(s.prints.size(), 1);
        std::vector<uagfx::groundway::Print> out;
        for (const uagfx::groundway::Print& o : s.was) {
            bool same = false;
            for (size_t i = 0; i < s.prints.size() && !same; ++i) {
                const uagfx::groundway::Print& q = s.prints[i];
                same = q.left == o.left && std::fabs(q.x - o.x) < 0.05f && std::fabs(q.z - o.z) < 0.05f;
                if (same) s.fresh[i] = 0;
            }
            if (!same) out.push_back(o);
        }
        s.was.swap(out);
        s.flat.clear();
        for (const uagfx::groundway::Point& q : s.points) s.flat.emplace_back(q.x, q.z);
        s.at = 0;
        s.pointsMade = true;
        s.cellSize = cellSize;
        if (s.changesLogged < 12 && !s.points.empty()) {
            ++s.changesLogged;
            LOG_INFO("ground way: the way on %s is %zu points, %zu footprints counted back from its end%s",
                     s.map.c_str(), s.points.size(), s.prints.size(),
                     s.was.empty() && s.wasLine.empty() ? "" : " - the one before fades out");
        }
    }
    const float k = std::min(1.0f, float(now - s.changedAt) / float(kChangeMs));  // the new one in, the old out
    if (k >= 1) {
        s.was.clear();
        s.wasLine.clear();
    }
    // where the player is along it: their place (not their cell), looked for near where they were
    if (s.flat.size() >= 2)
        s.at = walkpath::onWay(s.flat, at[0], at[2], s.at, walkpath::kBack * float(cellSize),
                               walkpath::kAhead * float(cellSize)).at;
    s.shown.clear();
    s.line.clear();
    if (s.points.size() >= 2) {
        if (s.way == WayOnGround::Steps) {
            std::vector<uagfx::groundway::Print> coming;  // while the way before fades: the new ones in
            if (k < 1) {
                coming = s.prints;
                for (size_t i = 0; i < coming.size(); ++i)
                    if (s.fresh[i]) coming[i].alpha *= k;
            }
            s.shown = uagfx::groundway::ahead(k < 1 ? coming : s.prints, s.at, kSkip, kMost, kFade);
        } else {
            s.line = uagfx::groundway::cut(s.points, s.at);
            s.lineAlpha = k;
        }
    }
    float view[12];
    if (!readGameCamera(view)) return;
    const camera::Projection p = camera::projectionFor(width, height);
    s.verts.clear();
    size_t shade = 0;
    std::vector<uagfx::groundway::Print> going = s.was;  // as they were shown, out by k
    for (uagfx::groundway::Print& q : going) q.alpha *= 1 - k;
    const auto prints = [&](const std::vector<uagfx::groundway::Print>& list) {  // the dark under, the blue over
        shade += uagfx::groundway::footprints(list, s.heights, cellSize, s.sign, kPrintLift, kPrintEdge, view, p,
                                              kPrintShade, kBias, s.verts);
        uagfx::groundway::footprints(list, s.heights, cellSize, s.sign, kPrintLift, 0.0f, view, p, kPrintBlue,
                                     kBias + 0.2f, s.verts);
    };
    const auto strip = [&](const std::vector<uagfx::groundway::Point>& line, float alpha) {
        shade += uagfx::groundway::strip(line, view, p, kEdge, uagfx::groundway::faded(kShade, alpha), kBias,
                                         s.verts);
        uagfx::groundway::strip(line, view, p, kWidth, uagfx::groundway::faded(kGold, alpha), kBias + 0.2f, s.verts);
    };
    if (!going.empty()) prints(going);
    if (!s.wasLine.empty()) strip(s.wasLine, s.wasLineAlpha * (1 - k));
    if (!s.shown.empty()) prints(s.shown);
    if (!s.line.empty()) strip(s.line, s.lineAlpha);
    if (s.verts.empty()) return;
    DrawCmd cmd;
    cmd.state = record.addState(stripSetup());
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.effect = uint8_t(effect_exe::Kind::None);
    record.draw(cmd, s.verts.data(), uint32_t(s.verts.size()), nullptr, 0);
    if (s.drawsLogged < 2) {
        ++s.drawsLogged;
        LOG_INFO("ground way: drawn on %s as %s - %zu points, %zu footprints shown of %zu, the player %.1f along, "
                 "%zu triangles (the edge's %zu)",
                 s.map.c_str(), wayOnGroundToken(s.way), s.points.size(), s.shown.size(), s.prints.size(), s.at,
                 s.verts.size() / 3, shade / 3);
    }
}

}  // namespace uagfx::win::groundway
