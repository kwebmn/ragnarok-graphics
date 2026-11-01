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
#include "core/skill_area.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/mob_hp.hpp"
#include "win/pick_hook.hpp"
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
// 3.0.11-beta.103: the look's arrow - faded in and out (ms), the chevrons' run
constexpr float kFadeInMs = 120, kFadeOutMs = 200;
constexpr double kFlashMs = 450, kFlashHoldMs = 150;  // 3.0.11-beta.142: a cast's cells - whole, then fading
constexpr double kRunMs = 600;

struct Walk {
    WalkLook look = WalkLook::Chevrons;
    bool mark = false;           // 3.0.11-beta.105: the mark at its end
    bool has = false;            // an arrow drawn or fading out
    groundway::WalkGlide glide;  // 3.0.11-beta.104: along the way, gliding with the feet (core/ground_way glide)
    bool cut = false;
    float alpha = 0, phase = 0;
    double lastMs = 0;   // 3.0.11-beta.104: the performance counter (GetTickCount64 steps 15.6 ms - the run jerked)
    bool told = false;   // its look told the log
    // 3.0.11-beta.104: each walk's smoothness for the log (cells a frame): the arrow's end against the feet, the feet
    int frames = 0, walls = 0, walksTold = 0, walks = 0;
    float endMax = 0, endSum = 0, feetMax = 0, lenMin = 0, lenMax = 0;
    float endX = 0, endZ = 0, feetX = 0, feetZ = 0;
};

double clockMs() {
    static LARGE_INTEGER frequency{};
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER count{};
    QueryPerformanceCounter(&count);
    return double(count.QuadPart) * 1000.0 / double(frequency.QuadPart);
}

// One frame of a walk measured: how far the arrow's end moved against the feet, how far the feet moved (cells)
void measure(Walk& w, const float me[3], float cs) {
    const float ex = std::cos(w.glide.angle) * w.glide.len / cs, ez = std::sin(w.glide.angle) * w.glide.len / cs;
    const float fx = me[0] / cs, fz = me[2] / cs, len = w.glide.len / cs;
    if (w.frames > 0) {
        const float end = std::hypot(ex - w.endX, ez - w.endZ);
        w.endMax = std::max(w.endMax, end);
        w.endSum += end;
        w.feetMax = std::max(w.feetMax, std::hypot(fx - w.feetX, fz - w.feetZ));
        w.lenMin = std::min(w.lenMin, len);
        w.lenMax = std::max(w.lenMax, len);
    } else {
        w.lenMin = w.lenMax = len;
    }
    w.walls += w.cut ? 1 : 0;
    ++w.frames;
    w.endX = ex, w.endZ = ez, w.feetX = fx, w.feetZ = fz;
}

// A walk over: its smoothness told (the first 6, then every 25th)
void tellWalk(Walk& w) {
    ++w.walks;
    if (w.frames > 1 && (w.walksTold < 6 || w.walks % 25 == 0)) {
        ++w.walksTold;
        LOG_INFO("walk arrow: a walk of %d frames - its end moved against the feet at most %.3f cells a frame (%.4f on "
                 "average), the feet at most %.2f; its length %.1f-%.1f cells, a wall in %d frames",
                 w.frames, double(w.endMax), double(w.endSum / float(w.frames - 1)), double(w.feetMax),
                 double(w.lenMin), double(w.lenMax), w.walls);
    }
    w.frames = w.walls = 0;
    w.endMax = w.endSum = w.feetMax = 0;
}

// 3.0.11-beta.142: a ground skill's cells flashed after its cast
struct Flash {
    std::vector<std::pair<int, int>> cells;
    uint32_t fill = 0, line = 0;
    double atMs = 0;
};

// A skill's colour on the ground: fire; ice, water and mud; holy and the shields; wind, lightning and earth; traps
void colours(uint16_t skill, uint32_t& fill, uint32_t& line) {
    uint32_t rgb = 0xFFFFFF;
    switch (skill) {
        case 18: case 80: case 83: case 285: rgb = 0xFF8A3D; break;           // Fire Wall, Pillar, Meteor, Volcano
        case 87: case 89: case 92: case 286: rgb = 0x6CC7E8; break;           // Ice Wall, Storm Gust, Quagmire, Deluge
        case 12: case 25: case 70: case 79: case 288: rgb = 0xE6CB8A; break;  // Safety Wall, Pneuma, Sanctuary, Magnus
        case 21: case 85: case 91: case 287: rgb = 0xB7A6FF; break;           // Thunder Storm, LoV, Heaven's Drive
        default:
            if (skillarea::kindOf(skill) == skillarea::Kind::Trap) rgb = 0x7FD3C3;
            break;
    }
    fill = 0x50000000u | rgb;
    line = 0xE6000000u | rgb;
}

// 3.0.11-beta.144: the game waiting for a ground skill's place - the skill and its level; false none
bool gameAims(uint16_t& skill, int& lv) {
    constexpr uint32_t kModeClass = 0x69F388, kWaitState = 0x3C4, kWaitSkill = 0x3C8, kWaitLevel = 0x3D0;
    const uint32_t mode = readGameMode();
    uint32_t cls = 0, state = 0, id = 0, level = 0;
    if (!mode || !readGame(mode, &cls, 4) || cls != kModeClass) return false;
    if (!readGame(mode + kWaitState, &state, 4) || (state != 1 && state != 5)) return false;
    if (!readGame(mode + kWaitSkill, &id, 4) || !readGame(mode + kWaitLevel, &level, 4) || id == 0 || id > 0xFFFF)
        return false;
    skill = uint16_t(id);
    lv = int(level);
    return true;
}

// 3.0.11-beta.146: the game waiting for any skill's target (+0x3C4 1 to 5), its mode when it is
uint32_t gameWaiting(uint32_t& skill) {
    constexpr uint32_t kModeClass = 0x69F388, kWaitState = 0x3C4, kWaitSkill = 0x3C8;
    const uint32_t mode = readGameMode();
    uint32_t cls = 0, state = 0;
    if (!mode || !readGame(mode, &cls, 4) || cls != kModeClass) return 0;
    if (!readGame(mode + kWaitState, &state, 4) || state < 1 || state > 5) return 0;
    if (!readGame(mode + kWaitSkill, &skill, 4)) skill = 0;
    return mode;
}

struct State {
    TargetRing ring = TargetRing::Pad;
    bool cellsOn = true;  // 3.0.11-beta.142: [interface] skill_cells
    int cancelsTold = 0;  // 3.0.11-beta.146
    std::vector<Flash> flashes;
    int castsTold = 0;
    uint16_t aimTold = 0;  // 3.0.11-beta.144: the game's aim last told the log
    int aimsTold = 0;
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

void walkLook(WalkLook look, bool mark) {
    Walk& w = state().walk;
    if (look == w.look && mark == w.mark) return;
    w.look = look;
    w.mark = mark;
    w.told = false;
    LOG_INFO("walk arrow: the look %s, %s", walkLookToken(look),
             mark ? "a mark at its end where it goes" : "no mark at its end");
}

void skillCells(bool on) {
    State& s = state();
    if (on == s.cellsOn) return;
    s.cellsOn = on;
    if (!on) s.flashes.clear();
    LOG_INFO("skill cells: [interface] skill_cells = %d", on ? 1 : 0);
}

void skillCast(uint16_t skill, int lv, int x, int y) {
    State& s = state();
    int px = 0, py = 0, w = 0, h = 0;
    if (!s.cellsOn || !readPlayerCell(px, py, w, h)) return;
    const std::vector<skillarea::Cell> cells =
        skillarea::cells(skill, lv, skillarea::Cell{px, py}, skillarea::Cell{x, y});
    if (cells.empty()) return;
    Flash f;
    for (const skillarea::Cell& c : cells) f.cells.emplace_back(c.x, c.y);
    colours(skill, f.fill, f.line);
    f.atMs = clockMs();
    if (s.castsTold++ < 20)
        LOG_INFO("skill cells: skill %u lv %d at %d, %d cast from %d, %d - %zu cells flashed", unsigned(skill), lv, x,
                 y, px, py, f.cells.size());
    if (s.flashes.size() >= 8) s.flashes.erase(s.flashes.begin());
    s.flashes.push_back(std::move(f));
}

bool cancelGameAim(const char* why) {
    constexpr uint32_t kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kEndWait = 0x4E;
    using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
    uint32_t skill = 0, vtable = 0, fn = 0;
    const uint32_t mode = gameWaiting(skill);
    if (!mode || !readGame(mode, &vtable, 4) || !readGame(vtable + kModeMsgSlot, &fn, 4) || fn != kModeMsg)
        return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(fn))(mode, kEndWait, 0, 0, 0);
    State& s = state();
    if (s.cancelsTold++ < 20)
        LOG_INFO("skill cells: the game's own wait for skill %u's target ended (its message 0x4e) - %s", skill, why);
    return true;
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
    int lead = 0;
    if (walking) uapad::runtime::walkWay(wayEast, wayNorth, cut, lead);
    const double now = clockMs();
    const float dt = w.lastMs > 0 ? float(std::min(now - w.lastMs, 100.0)) : 0.0f;
    w.lastMs = now;
    w.alpha = walking ? std::min(1.0f, w.alpha + dt / kFadeInMs) : std::max(0.0f, w.alpha - dt / kFadeOutMs);
    if (!walking && w.alpha <= 0 && w.has) {
        w.has = false;
        w.glide = groundway::WalkGlide{};
        tellWalk(w);
    }
    const bool arrowShown = w.look != WalkLook::Off && (walking || w.has);
    if (padNow && s.ring != TargetRing::Off) {
        target = uapad::runtime::target();
        next = uapad::runtime::preview();
        mate = uapad::runtime::choosing();
    }
    const bool mouse = !target && s.ring == TargetRing::Always;
    if (mouse) target = targetpanel::fighting();  // 3.0.11-beta.99: the card on or not
    const double nowFlash = clockMs();  // 3.0.11-beta.142: a ground skill's cells after its cast
    s.flashes.erase(std::remove_if(s.flashes.begin(), s.flashes.end(),
                                   [&](const Flash& f) { return nowFlash - f.atMs > kFlashMs; }),
                    s.flashes.end());
    // 3.0.11-beta.144: the game waiting for a ground skill's place - its cells on the cell under the mouse
    uint16_t aimSkill = 0;
    int aimLv = 0, aimX = 0, aimY = 0;
    // 3.0.11-beta.145: the pad's aim first (a ground skill's button held), the game's with the mouse after
    const bool padAim = s.cellsOn && uapad::runtime::aimCells(aimSkill, aimLv, aimX, aimY);
    // 3.0.11-beta.146: the mouse's only while the pad is not the one played with (its cursor hidden then)
    const bool aiming = padAim || (s.cellsOn && !padNow && gameAims(aimSkill, aimLv) && lastPick(aimX, aimY));
    if (aiming && aimSkill != s.aimTold && s.aimsTold < 20) {
        ++s.aimsTold;
        LOG_INFO("skill cells: %s skill %u lv %d - its cells at %d, %d", padAim ? "the pad aims" : "the game waits for",
                 unsigned(aimSkill), aimLv, aimX, aimY);
    }
    s.aimTold = aiming ? aimSkill : 0;
    if (!target && !next && !mate && !arrowShown && s.flashes.empty() && !aiming) return;
    float view[12];
    if (!readGameCamera(view)) return;
    const camera::Projection p = camera::projectionFor(width, height);
    s.verts.clear();
    s.flat.clear();
    float me[3];
    int mapW = 0, mapH = 0, cellSize = 0;
    if (arrowShown && readPlayerWorld(me, mapW, mapH, cellSize)) {  // 3.0.11-beta.88: the way the stick walks
        const float cs = float(cellSize), y = me[1] - kLift;
        if (walking) {  // 3.0.11-beta.104: along the march's way, gliding with the feet (not to the asked cell)
            float cx = 0, cz = 0;
            groundway::cellToWorld(toX, toY, mapW, mapH, cellSize, cx, cz);
            groundway::WalkAim aim;
            aim.x0 = me[0];
            aim.z0 = me[2];
            aim.wayX = wayEast;
            aim.wayZ = wayNorth;
            aim.cellX = cx;
            aim.cellZ = cz;
            aim.cut = cut;
            aim.lead = float(lead) * cs;
            groundway::glide(w.glide, aim, dt);
            w.cut = cut;
            w.has = true;
            w.phase = float(std::fmod(now / kRunMs, 1.0));
            measure(w, me, cs);
        }
        float sign = 0;  // 3.0.11-beta.102: on the ground's heights, the flat arrow without them
        const groundway::Heights* heights = heightsHere(s, me, mapW, mapH, cellSize, sign);
        groundway::WalkArrow arrow;
        arrow.x0 = me[0];
        arrow.z0 = me[2];
        arrow.wayX = std::cos(w.glide.angle);
        arrow.wayZ = std::sin(w.glide.angle);
        arrow.len = w.glide.len;
        arrow.blocked = w.cut;
        arrow.phase = w.phase;
        arrow.alpha = w.alpha;
        arrow.mark = w.mark;
        std::vector<groundway::Tri> tris;
        if (heights) tris = groundway::walkShape(w.look, arrow, *heights, cellSize, sign, kLift);
        const float len = w.glide.len, tx = me[0] + arrow.wayX * len, tz = me[2] + arrow.wayZ * len;
        if (!tris.empty()) {
            groundway::placeTris(tris, view, p, kBias, s.verts);
        } else if (!heights && walking && len > 3.0f) {
            const float x0 = me[0] + arrow.wayX * 2.2f, z0 = me[2] + arrow.wayZ * 2.2f;
            groundway::arrow(x0, z0, tx, tz, y, kArrowWidth + 0.8f, kArrowHead + 0.6f, kArrowHeadWidth + 0.8f, view, p,
                             kArrowShade, kBias, s.verts);
            groundway::arrow(x0, z0, tx, tz, y, kArrowWidth, kArrowHead, kArrowHeadWidth, view, p, kArrowWhite,
                             kBias + 0.2f, s.verts);
            if (w.mark) groundway::ring(tx, y, tz, 1.4f, 0.45f, 20, false, view, p, kArrowGold, kBias + 0.2f, s.verts);
        }
        if (!s.arrowTold && walking) {
            s.arrowTold = true;
            LOG_INFO("walk arrow: from the world's %.1f, %.1f along %.2f, %.2f, %d cells ahead (the cell %d, %d asked)",
                     me[0], me[2], wayEast, wayNorth, lead, toX, toY);
        }
        if (!w.told && !tris.empty() && w.alpha >= 1.0f) {
            w.told = true;
            LOG_INFO("walk arrow: %s - %zu triangles on the ground, the way %.0f deg from east%s%s",
                     walkLookToken(w.look), tris.size(), double(w.glide.angle) * 57.29578,
                     w.mark ? "" : ", no mark at its end", w.cut ? ", a wall past its end" : "");
        }
    }
    float feet[3];
    int fw = 0, fh = 0, fcs = 0;
    if ((!s.flashes.empty() || aiming) && readPlayerWorld(feet, fw, fh, fcs)) {  // 3.0.11-beta.142: on the heights
        float sign = 0;
        const groundway::Heights* heights = heightsHere(s, feet, fw, fh, fcs, sign);
        int px = 0, py = 0, pw = 0, ph = 0;
        if (heights && aiming && readPlayerCell(px, py, pw, ph)) {  // 3.0.11-beta.144: the mouse's aim, whole
            std::vector<std::pair<int, int>> cells;
            for (const skillarea::Cell& c :
                 skillarea::cells(aimSkill, aimLv, skillarea::Cell{px, py}, skillarea::Cell{aimX, aimY}))
                cells.emplace_back(c.x, c.y);
            uint32_t fill = 0, line = 0;
            colours(aimSkill, fill, line);
            groundway::placeTris(groundway::cellsShape(cells, fill, line, 0.85f, *heights, fcs, sign, kLift), view, p,
                                 kBias, s.verts);
        }
        if (heights)
            for (const Flash& f : s.flashes) {
                const double age = nowFlash - f.atMs;
                const float a =
                    age < kFlashHoldMs ? 1.0f : float(1.0 - (age - kFlashHoldMs) / (kFlashMs - kFlashHoldMs));
                groundway::placeTris(groundway::cellsShape(f.cells, f.fill, f.line, a, *heights, fcs, sign, kLift),
                                     view, p, kBias, s.verts);
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
