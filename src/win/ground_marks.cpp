#include "win/ground_marks.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "core/d3d7.hpp"
#include "core/effect_exe.hpp"
#include "core/game_camera.hpp"
#include "core/ground_way.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/mob_hp.hpp"
#include "win/target_card.hpp"

namespace uagfx::win::groundmarks {
namespace {

// a cell is 5 world units: the ring 4.2 out (a medium monster's feet), its colour 0.9 wide over a dark 1.7
constexpr float kRadius = 4.2f, kWidth = 0.9f, kEdge = 1.7f, kLift = 0.4f, kBias = 1.5f;
constexpr int kSides = 40;
constexpr uint32_t kGold = 0xF5E6CB8A, kShade = 0x70000000, kWhite = 0xC8FFFFFF, kGreen = 0xF05BD36C;
// 3.0.11-beta.88: the walk without the mouse's arrow - faint white over a dark edge, a small gold ring at its end
constexpr float kArrowWidth = 1.0f, kArrowHead = 3.2f, kArrowHeadWidth = 3.0f;
constexpr uint32_t kArrowWhite = 0x96FFFFFF, kArrowShade = 0x50000000, kArrowGold = 0xC8E6CB8A;
// 3.0.11-beta.103: the look's arrow - faded in and out (ms), its way turned to (ms, a time constant), the chevrons' run
constexpr float kFadeInMs = 120, kFadeOutMs = 200, kTurnMs = 80;
constexpr double kRunMs = 600;

struct Walk {
    WalkLook look = WalkLook::Chevrons;
    bool has = false;  // an arrow drawn or fading out
    float angle = 0;   // its way (radians from east toward north)
    int toX = 0, toY = 0;
    bool cut = false;
    float alpha = 0, phase = 0;
    ULONGLONG lastMs = 0;
    bool told = false;  // its look told the log
};

struct State {
    TargetRing ring = TargetRing::Pad;
    bool drawn = false;
    std::vector<groundway::Vertex> verts, flat;
    uint32_t told = 0;  // the target last told the log
    int tells = 0;
    bool arrowTold = false;
    // 3.0.11-beta.102: the map's .gat heights for the stick's arrow on the ground, and which way round they run
    std::string heightsMap;
    groundway::Heights heights;
    bool heightsRead = false;
    float sign = 0;
    std::unique_ptr<gamefiles::Reader> files;
    Walk walk;  // 3.0.11-beta.103
};

State& state() {
    static State s;
    return s;
}

DrawSetup setup(bool depth) {  // blended, untextured, its colour its own; the rings tested against the world's depth
    DrawSetup s;
    s.zEnable = depth;
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

void put(FrameRecord& record, const std::vector<groundway::Vertex>& verts, bool depth) {
    if (verts.empty()) return;
    DrawCmd cmd;
    cmd.state = record.addState(setup(depth));
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.effect = uint8_t(effect_exe::Kind::None);
    record.draw(cmd, verts.data(), uint32_t(verts.size()), nullptr, 0);
}

// The map's heights where the player stands, told which way round against its own height (none: not read, or they do
// not fit it - the flat arrow then)
const groundway::Heights* heightsHere(State& s, const float me[3], int mapW, int mapH, int cellSize, float& sign) {
    const std::string map = readGameMap();
    if (map.empty()) return nullptr;
    if (map != s.heightsMap) {
        s.heightsMap = map;
        s.heightsRead = false;
        s.sign = 0;
        s.heights = groundway::Heights{};
        if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
        std::vector<uint8_t> bytes;
        if (s.files->read("data\\" + map + ".gat", bytes) &&
            groundway::parseGat(std::string(bytes.begin(), bytes.end()), s.heights))
            s.heightsRead = true;
        LOG_INFO("walk arrow: %s's heights %s", map.c_str(), s.heightsRead ? "read - the arrow on the ground" : "none");
    }
    if (!s.heightsRead || s.heights.w != mapW || s.heights.h != mapH || cellSize <= 0) return nullptr;
    if (s.sign == 0) {
        float h = 0;
        if (!s.heights.at(me[0] / float(cellSize) + float(mapW / 2), me[2] / float(cellSize) + float(mapH / 2), h))
            return nullptr;
        s.sign = std::fabs(me[1] - h) <= 3.0f ? 1.0f : std::fabs(me[1] + h) <= 3.0f ? -1.0f : 0.0f;
        if (s.sign == 0) {
            s.heightsRead = false;  // they do not fit the player's own height: the flat arrow on this map
            LOG_INFO("walk arrow: %s's heights do not fit the player's own - the flat arrow", map.c_str());
            return nullptr;
        }
    }
    sign = s.sign;
    return &s.heights;
}

}  // namespace

void look(TargetRing ring) {
    State& s = state();
    if (ring == s.ring) return;
    s.ring = ring;
    LOG_INFO("target ring: %s", targetRingToken(ring));
}

void walkLook(WalkLook look) {
    Walk& w = state().walk;
    if (look == w.look) return;
    w.look = look;
    w.told = false;
    LOG_INFO("walk arrow: the look %s", walkLookToken(look));
}

void newFrame() { state().drawn = false; }

void draw(FrameRecord& record, int width, int height) {
    State& s = state();
    if (s.drawn || width <= 0 || height <= 0) return;
    s.drawn = true;
    uint32_t target = 0, next = 0, mate = 0;
    int toX = 0, toY = 0;
    const bool padNow = uapad::runtime::look().shown;
    const bool walking = padNow && uapad::runtime::walkTarget(toX, toY);  // 3.0.11-beta.88: the stick without the mouse
    Walk& w = s.walk;  // 3.0.11-beta.103: faded in while it walks, out after
    float wayEast = 0, wayNorth = 0;
    bool cut = false;
    if (walking) uapad::runtime::walkWay(wayEast, wayNorth, cut);
    const ULONGLONG now = GetTickCount64();
    const float dt = w.lastMs ? float(std::min<ULONGLONG>(now - w.lastMs, 100)) : 0.0f;
    w.lastMs = now;
    w.alpha = walking ? std::min(1.0f, w.alpha + dt / kFadeInMs) : std::max(0.0f, w.alpha - dt / kFadeOutMs);
    if (!walking && w.alpha <= 0) w.has = false;
    const bool arrowShown = w.look != WalkLook::Off && (walking || w.has);
    if (padNow && s.ring != TargetRing::Off) {
        target = uapad::runtime::target();
        next = uapad::runtime::preview();
        mate = uapad::runtime::choosing();
    }
    const bool mouse = !target && s.ring == TargetRing::Always;
    if (mouse) target = targetpanel::fighting();  // 3.0.11-beta.99: the card on or not
    if (!target && !next && !mate && !arrowShown) return;
    float view[12];
    if (!readGameCamera(view)) return;
    const camera::Projection p = camera::projectionFor(width, height);
    s.verts.clear();
    s.flat.clear();
    float me[3];
    int mapW = 0, mapH = 0, cellSize = 0;
    if (arrowShown && readPlayerWorld(me, mapW, mapH, cellSize)) {  // 3.0.11-beta.88: the way the stick walks
        float tx = 0, tz = 0;
        groundway::cellToWorld(walking ? toX : w.toX, walking ? toY : w.toY, mapW, mapH, cellSize, tx, tz);
        const float dx = tx - me[0], dz = tz - me[2], len = std::hypot(dx, dz), y = me[1] - kLift;
        if (walking) {  // 3.0.11-beta.103: the way to the cell (the stick's own standing on it), turned to softly
            const bool there = len < 0.6f * float(cellSize);
            const float wx = there ? wayEast : dx, wz = there ? wayNorth : dz;
            if (std::hypot(wx, wz) > 1e-4f) {
                const float want = std::atan2(wz, wx);
                float turn = std::remainder(want - w.angle, 6.2831853f);
                if (!w.has) turn = want - w.angle;  // a new arrow: its way at once
                w.angle += w.has ? turn * (1.0f - std::exp(-dt / kTurnMs)) : turn;
            }
            w.toX = toX;
            w.toY = toY;
            w.cut = cut;
            w.has = true;
            w.phase = float(std::fmod(double(now) / kRunMs, 1.0));
        }
        float sign = 0;  // 3.0.11-beta.102: on the ground's heights, the flat arrow without them
        const groundway::Heights* heights = heightsHere(s, me, mapW, mapH, cellSize, sign);
        groundway::WalkArrow arrow;
        arrow.x0 = me[0];
        arrow.z0 = me[2];
        arrow.wayX = std::cos(w.angle);
        arrow.wayZ = std::sin(w.angle);
        arrow.toX = w.toX;
        arrow.toY = w.toY;
        arrow.blocked = w.cut;
        arrow.phase = w.phase;
        arrow.alpha = w.alpha;
        std::vector<groundway::Tri> tris;
        if (heights) tris = groundway::walkShape(w.look, arrow, *heights, cellSize, sign, kLift);
        if (!tris.empty()) {
            groundway::placeTris(tris, view, p, kBias, s.verts);
        } else if (!heights && walking && len > 3.0f) {
            const float x0 = me[0] + dx / len * 2.2f, z0 = me[2] + dz / len * 2.2f;
            groundway::arrow(x0, z0, tx, tz, y, kArrowWidth + 0.8f, kArrowHead + 0.6f, kArrowHeadWidth + 0.8f, view, p,
                             kArrowShade, kBias, s.verts);
            groundway::arrow(x0, z0, tx, tz, y, kArrowWidth, kArrowHead, kArrowHeadWidth, view, p, kArrowWhite,
                             kBias + 0.2f, s.verts);
            groundway::ring(tx, y, tz, 1.4f, 0.45f, 20, false, view, p, kArrowGold, kBias + 0.2f, s.verts);
        }
        if (!s.arrowTold && walking) {
            s.arrowTold = true;
            LOG_INFO("walk arrow: from the world's %.1f, %.1f to the cell %d, %d (%.1f units)", me[0], me[2], toX, toY,
                     len);
        }
        if (!w.told && !tris.empty() && w.alpha >= 1.0f) {
            w.told = true;
            LOG_INFO("walk arrow: %s - %zu triangles on the ground, the way %.0f deg from east%s",
                     walkLookToken(w.look), tris.size(), double(w.angle) * 57.29578,
                     w.cut ? ", a wall past the cell" : "");
        }
    }
    float at[3] = {0, 0, 0};
    const auto ringAt = [&](uint32_t id, uint32_t argb, bool dashed, bool notched) {
        if (!id || !readActorWorld(id, at)) return false;
        const float y = at[1] - kLift;  // up along -y
        groundway::ring(at[0], y, at[2], kRadius, kEdge, kSides, dashed, view, p, kShade, kBias, s.verts);
        groundway::ring(at[0], y, at[2], kRadius, kWidth, kSides, dashed, view, p, argb, kBias + 0.2f, s.verts);
        if (notched) groundway::notches(at[0], y, at[2], kRadius, kWidth, view, p, argb, kBias + 0.2f, s.verts);
        return true;
    };
    const bool ringed = ringAt(target, kGold, false, false);
    const float where[3] = {at[0], at[1], at[2]};
    ringAt(next, kWhite, true, false);
    ringAt(mate, kGreen, false, true);
    float box[4];
    const bool boxed = ringed && readActorBox(target, box);
    if (boxed) {  // the chevron over its head: the dark under, the gold over
        const float w = std::max(12.0f, float(height) * 0.016f), h = w * 0.7f;
        float tip = box[1] - w * 0.3f, bar = 0;
        if (mobhpwin::targetBarTop(target, box, height, bar)) tip = std::min(tip, bar - 3);  // 3.0.11-beta.95: over it
        const float cx = (box[0] + box[2]) / 2;
        groundway::chevron(cx, tip + 2, w + 6, h + 4, 0x90000000u, s.flat);
        groundway::chevron(cx, tip, w, h, kGold, s.flat);
    }
    put(record, s.verts, true);
    put(record, s.flat, false);
    if (target && target != s.told && s.tells < 30) {
        s.told = target;
        ++s.tells;
        if (ringed)
            LOG_INFO("target ring: under %u (%s) at the world's %.1f, %.1f, %.1f%s", target,
                     mouse ? "the mouse's fight" : "the pad's target", where[0], where[1], where[2],
                     boxed ? ", the chevron over its sprite" : ", its sprite's box not found");
        else
            LOG_INFO("target ring: %u not found among the game's actors", target);
    }
}

}  // namespace uagfx::win::groundmarks
