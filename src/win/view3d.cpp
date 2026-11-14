#include "win/view3d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "core/game_camera.hpp"
#include "core/view3d.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_window.hpp"
#include "win/code_detour.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/uaui.hpp"
#include "win/walk_mode.hpp"

namespace uagfx::win::view3dwin {
namespace {

using BuildFn = void(__attribute__((thiscall)) *)(uint32_t view);
using HeightFn = float(__attribute__((thiscall)) *)(uint32_t ground, float x, float z);

// uaRO.exe (docs/superpowers/plans/2026-11-14-view3d.md)
constexpr uint32_t kBuild = 0x4FAF00;  // CView's build (fastcall, ECX the view)
constexpr uint8_t kBuildCode[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x34};  // push ebp; mov ebp, esp; sub esp, 0x34
constexpr uint32_t kRenderer = 0x6EE3D8;      // CRenderer*: +0 f0, +4 f1, +8/+0xc the slopes, +0x24/+0x28 w, h
constexpr uint32_t kGroundHeight = 0x42A8C0;  // thiscall (ground, x, z): its height there - the build's own
constexpr uint32_t kModeWorld = 0xC4, kWorldGround = 0x40;  // the game mode's world, its ground (as the build reads)
constexpr uint32_t kGameW = 0x6EBE1C, kGameH = 0x6EBE20;    // the game's size (its mouse kept within it)
constexpr uint32_t kMouseRmb = 0x6EE6F0 + 0x24;             // CMouse: the right button down
// CView: the yaw and its target (degrees), the eye, the up, the player's place; the view matrix and its inverse
constexpr uint32_t kYaw = 0x20, kYawTo = 0x38, kEye = 0x7C, kUp = 0x88, kFeet = 0x40;
constexpr uint32_t kTilt = 0x1C, kTiltTo = 0x34, kDistanceTo = 0x3C;  // beta.7: the pad's tilt, the wheel's zoom
constexpr float kHeldTilt = -45.0f;  // the game's tilt held here while in the 3D view: a change of it is the pad's
constexpr uint32_t kView = 0x94, kInverse = 0xC4;
constexpr float kDegreesPerCount = 0.07f;  // the mouse: 1000 counts (about an inch) turn 70 degrees
// 3.0.19-beta.15 (R-YCGGJR «глючит мышка»: the counts jumped - 1971 in a poll for 2 px of the cursor - as the window
// came back in front, and they go with the mouse's DPI): the Windows cursor's own move turns the view (the player's
// pointer speed), parked at the window's middle each frame; a frame's move past these taken for a jump and dropped
constexpr float kDegreesPerPixel = 0.1f;
constexpr long kMostPixels = 200, kMostCounts = 400;
// 3.0.19-beta.15 (R-W5K25A: in 15, 17, 5, 12, 5 of 78 within two seconds by Morocc's houses): the eye goes out again
// only this long after it was last brought in; nearer than kOwnNear behind, the character's own picture left out
constexpr DWORD kOutAfterMs = 500;
constexpr float kOwnNear = 12.0f;
// 3.0.19-beta.11: the crosshair pulled onto a monster this near (of the game's height), onto the Tab-locked one from
// this far; the lock let go when it is not on the screen for kLockLostMs
constexpr float kPullReach = 0.045f, kLockReach = 0.14f;
constexpr DWORD kLockLostMs = 5000;

struct State {
    view3d::Mode mode = view3d::Mode::Off, wanted = view3d::Mode::Off;
    view3d::Look look;
    float sign = 1.0f;       // which way the game's yaw grows on the screen (from its own matrix)
    float wroteYaw = -1.0f;  // the yaw put into the game last frame
    float wroteTilt = 0.0f;  // beta.7: the tilt held there (kHeldTilt once written)
    float fov = view3d::kGameFov;
    int fovFirst = int(view3d::kFirstFov), fovBehind = int(view3d::kBehindFov);  // 3.0.19-beta.10: [view3d]
    float mouseSpeed = 1.0f;
    float behindNow = 0.0f;  // 3.0.19-beta.10: the distance behind used (in at once before a wall, out slowly)
    float zoomAtEntry = 0.0f;  // 3.0.19-beta.15: the game's zoom as the 3D view began (the wheel moves from there)
    DWORD pulledAt = 0;        // 3.0.19-beta.15: brought in last - out again only kOutAfterMs later
    std::vector<view3d::Box> wayBoxes;  // 3.0.19-beta.12: the models on the way back this frame
    std::vector<uint32_t> waySeen;
    int modelPullsTold = 0;
    bool walkBefore = false;  // the walk mode as it was before the 3D view
    bool centred = false;
    bool looking = false;
    bool clipped = false;
    bool skipChar = false;
    int frames = 0;
    DWORD toldAt = 0;
    // 3.0.19-beta.11 (R-ZQWGVD «мышкой ... крутит и не слушается»): the game's cursor put at (putX, putY) each frame -
    // held there by the pad runtime (uapad::runtime::holdCursor), the mouse's own moves turning the view
    int putX = 0, putY = 0;
    POINT parked{};              // 3.0.19-beta.15: the Windows cursor put there last frame (the screen's pixels)
    bool parkedValid = false, cursorMoved = false;
    long pixels = 0, counts = 0, mostPixels = 0, jumps = 0;  // for the log
    bool skipTab = false, wantLock = false;
    uint32_t locked = 0, aimed = 0;  // Tab's monster, the one the crosshair is on
    DWORD lockSeen = 0;
    int pullsTold = 0;
    std::unordered_map<uint32_t, std::array<float, 4>> boxes;
    std::unordered_map<uint32_t, int> classes;
    std::vector<view3d::AimBox> aims;
};

State g_state;  // the game's thread: its window's keys, its frame, its camera build
uintptr_t g_build = 0;

// 3.0.19-beta.11 (R-H2UJ9X: from the eyes the character's own head and hair right in front of them): the character's
// render (its class's virtual slot 3: Render(matrix) - as uaRO.exe's actors 0x5aa130 and 0x5aac50, its shadow and body)
// left out while the view is from the eyes; the slot of its vtable ours, the others of the class drawn as before
using RenderFn = void(__attribute__((thiscall)) *)(uint32_t actor, uint32_t matrix);
constexpr uint32_t kModeWorldOf = 0xC4, kWorldPlayer = 0x3C, kRenderSlot = 3 * 4;
uintptr_t g_playerRender = 0;

uint32_t playerNow() {
    const uint32_t mode = readGameMode();
    uint32_t world = 0, player = 0;
    if (!mode || !readGame(mode + kModeWorldOf, &world, 4) || !world || !readGame(world + kWorldPlayer, &player, 4))
        return 0;
    return player;
}

bool ownHiddenNow() {  // 3.0.19-beta.15: from the eyes, or the eye pulled in right behind the head
    return g_state.mode == view3d::Mode::First ||
           (g_state.mode == view3d::Mode::Behind && g_state.behindNow > 0.0f && g_state.behindNow < kOwnNear);
}

void __attribute__((thiscall)) playerRender(uint32_t actor, uint32_t matrix) {
    if (ownHiddenNow() && actor == playerNow()) return;
    reinterpret_cast<RenderFn>(g_playerRender)(actor, matrix);
}

void hidePlayerRender() {
    static bool tried = false;
    if (tried) return;
    const uint32_t player = playerNow();
    if (!player) return;
    tried = true;
    uint32_t table = 0, fn = 0;
    uint8_t code[3] = {};
    if (!readGame(player, &table, 4) || !table || !readGame(table + kRenderSlot, &fn, 4) || !fn ||
        !readGame(fn, code, sizeof code) || code[0] != 0x55 || code[1] != 0x8B || code[2] != 0xEC) {
        LOG_INFO("3D view: the character's own render not found (its table 0x%08X, slot 3 0x%08X) - drawn from the "
                 "eyes", table, fn);
        return;
    }
    auto* slot = reinterpret_cast<uint32_t*>(uintptr_t(table + kRenderSlot));
    DWORD old = 0;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) {
        LOG_INFO("3D view: the character's render slot not writable (%lu) - drawn from the eyes", GetLastError());
        return;
    }
    g_playerRender = fn;
    *slot = uint32_t(reinterpret_cast<uintptr_t>(&playerRender));
    VirtualProtect(slot, 4, old, &old);
    LOG_INFO("3D view: the character's own picture left out from the eyes - its render (table 0x%08X slot 3, 0x%08X) "
             "ours", table, fn);
}

float* at(uint32_t view, uint32_t offset) { return reinterpret_cast<float*>(uintptr_t(view + offset)); }

// The renderer's projection for a field of view, as SetSize would make it (core/game_camera setSizeFields) - each
// frame, as the game's own SetSize (a new size) puts its 15 back
void applyFov(State& s, float fov) {
    uint32_t r = 0;
    if (!readGame(kRenderer, &r, 4) || !r) return;
    auto* f = reinterpret_cast<float*>(uintptr_t(r));
    const int w = *reinterpret_cast<const int*>(uintptr_t(r + 0x24));
    const int h = *reinterpret_cast<const int*>(uintptr_t(r + 0x28));
    if (w <= 0 || h <= 0) return;
    const camera::SetSizeFields want = camera::setSizeFields(fov, w, h);
    if (f[0] != want.f0 || f[1] != want.f1 || f[2] != want.slopeH || f[3] != want.slopeV) {
        static int told = 0;  // 3.0.19-beta.14: each time the fields are put right (a few dozen at most)
        if (fov != s.fov || told < 20) {
            ++told;
            LOG_INFO("3D view: the field of view %.0f degrees - the projection's f1 %.1f -> %.1f (%dx%d)", fov, f[1],
                     want.f1, w, h);
        }
        f[0] = want.f0;
        f[1] = want.f1;
        f[2] = want.slopeH;
        f[3] = want.slopeV;
    }
    s.fov = fov;
    camera::setFov(fov);
}

// The game's pixels as the window's client ones (the game may be drawn scaled into it)
POINT clientOf(HWND game, int gx, int gy, int w, int h) {
    RECT r{};
    if (!GetClientRect(game, &r) || r.right <= 0 || r.bottom <= 0) return POINT{gx, gy};
    return POINT{LONG(std::lround(double(gx) * double(r.right) / double(w))),
                 LONG(std::lround(double(gy) * double(r.bottom) / double(h)))};
}

// The Windows cursor kept at the game's point (x, y): out of the way while the view turns, and where the crosshair is
// when Alt frees it
bool park(HWND game, int x, int y, int w, int h, POINT* where = nullptr) {
    POINT at = clientOf(game, x, y, w, h), now{};
    if (!ClientToScreen(game, &at) || !GetCursorPos(&now)) return false;
    if (std::abs(now.x - at.x) > 1 || std::abs(now.y - at.y) > 1) SetCursorPos(at.x, at.y);
    if (where) *where = at;
    return true;
}

// The hold let go (Alt, the window behind, the view off): the Windows cursor where the crosshair was
void release(State& s) {
    if (!s.centred) return;
    s.centred = false;
    s.parkedValid = false;
    uapad::runtime::holdCursor(false, 0, 0);
    int w = 0, h = 0;
    const HWND game = uaui::gameWindow();
    if (game && readGame(kGameW, &w, 4) && readGame(kGameH, &h, 4) && w > 0 && h > 0) park(game, s.putX, s.putY, w, h);
}

// 3.0.19-beta.11: the monsters' boxes on the screen (the game's pick list of the last picture)
void readAims(State& s) {
    readActorBoxes(s.boxes, &s.classes);
    s.aims.clear();
    for (const auto& [id, b] : s.boxes) {
        const auto c = s.classes.find(id);
        if (c != s.classes.end() && view3d::monsterClass(c->second)) s.aims.push_back({id, b[0], b[1], b[2], b[3]});
    }
}

// Tab pressed: the next monster locked; the locked one not on the screen for a while: let go
void lockStep(State& s, float cx, float cy) {
    const DWORD now = GetTickCount();
    if (s.wantLock) {
        s.wantLock = false;
        s.locked = view3d::nextLock(s.aims, cx, cy, s.locked);
        s.lockSeen = now;
        if (s.locked)
            LOG_INFO("3D view: Tab - locked on %u (%zu monster(s) on the screen)", s.locked, s.aims.size());
        else
            LOG_INFO("3D view: Tab - no monster on the screen, nothing locked");
        return;
    }
    if (!s.locked) return;
    for (const view3d::AimBox& b : s.aims)
        if (b.id == s.locked) s.lockSeen = now;
    if (now - s.lockSeen > kLockLostMs) {
        LOG_INFO("3D view: the locked %u not on the screen for %lu s - let go", s.locked, kLockLostMs / 1000);
        s.locked = 0;
    }
}

// The mouse's own moves turn the view (the pad runtime holds the game's cursor and keeps them; without it the cursor's
// own move since the last frame); the game's cursor at the crosshair - pulled onto a monster near it (its clicks and
// its hover there). Alt held, or the window not in front: the cursor free.
void mouse(State& s) {
    const HWND game = uaui::gameWindow();
    s.looking = game && GetForegroundWindow() == game && GetKeyState(VK_MENU) >= 0;
    if (!s.looking) {
        release(s);
        return;
    }
    int w = 0, h = 0;
    if (!readGame(kGameW, &w, 4) || !readGame(kGameH, &h, 4) || w <= 0 || h <= 0) return;
    int* x = reinterpret_cast<int*>(uintptr_t(camera::kMouse + camera::kMouseX));
    int* y = reinterpret_cast<int*>(uintptr_t(camera::kMouse + camera::kMouseY));
    const int cx = w / 2, cy = h / 2;
    long cdx = 0, cdy = 0;
    const bool held = uapad::runtime::takeLook(cdx, cdy);
    // 3.0.19-beta.15: the Windows cursor's own move since it was parked; the counts only where it never moves (the
    // mouse taken whole by the game, no mouse freedom); without the hold the game's own cursor's move, as before
    POINT now{};
    long dx = 0, dy = 0;
    float perUnit = kDegreesPerPixel;
    if (s.centred && s.parkedValid && GetCursorPos(&now)) {
        dx = now.x - s.parked.x;
        dy = now.y - s.parked.y;
        if (dx || dy) s.cursorMoved = true;
    }
    if (held && !s.cursorMoved && (cdx || cdy)) {
        dx = cdx, dy = cdy;
        perUnit = kDegreesPerCount;
    } else if (!held && !s.cursorMoved && s.centred) {
        dx = *x - s.putX, dy = *y - s.putY;
        perUnit = kDegreesPerCount;
    }
    const long most = perUnit == kDegreesPerPixel ? kMostPixels : kMostCounts;
    if (std::labs(dx) > most || std::labs(dy) > most) {  // a jump (the window back in front): not the player's
        ++s.jumps;
        dx = dy = 0;
    }
    s.pixels += std::labs(dx) + std::labs(dy);
    s.counts += std::labs(cdx) + std::labs(cdy);
    s.mostPixels = std::max(s.mostPixels, std::max(std::labs(dx), std::labs(dy)));
    if (s.centred && (dx || dy)) view3d::turn(s.look, float(dx), float(dy), perUnit * s.mouseSpeed, s.sign);
    readAims(s);
    lockStep(s, float(cx), float(cy));
    const view3d::Aim a = view3d::aimAt(s.aims, float(cx), float(cy), kPullReach * float(h), s.locked,
                                        kLockReach * float(h));
    const int ax = int(std::lround(a.x)), ay = int(std::lround(a.y));
    if (a.id != s.aimed && a.id && (ax != cx || ay != cy) && s.pullsTold < 10) {
        ++s.pullsTold;
        LOG_INFO("3D view: the crosshair pulled onto %u%s (%d, %d px off the middle)", a.id,
                 a.id == s.locked ? " (the locked one)" : "", ax - cx, ay - cy);
    }
    s.aimed = a.id;
    *x = ax;
    *y = ay;
    s.putX = ax;
    s.putY = ay;
    uapad::runtime::holdCursor(true, ax, ay);
    s.parkedValid = park(game, cx, cy, w, h, &s.parked);
    s.centred = true;
}

// The pad's stick or the keys (Q/E) turned the game's camera since the last frame: the view turns with it (the right
// button's own turning is the mouse's, already ours)
void adoptYaw(uint32_t view, State& s) {
    const float to = *at(view, kYawTo);
    int rmb = 0;
    readGame(kMouseRmb, &rmb, 4);
    if (s.wroteYaw >= 0.0f && std::isfinite(to) && std::fabs(to - s.wroteYaw) > 0.01f && !rmb) {
        float yaw = std::fmod(s.look.yaw + (to - s.wroteYaw), 360.0f);
        s.look.yaw = yaw < 0.0f ? yaw + 360.0f : yaw;
    }
}

// 3.0.19-beta.7: the pad's right stick tilted the game's camera (its tilt's target): the view looks up or down with
// it - the tilt toward the horizon (less negative) looking up; the right button's Shift tilt is the mouse's, ours
void adoptTilt(uint32_t view, State& s) {
    const float to = *at(view, kTiltTo);
    int rmb = 0;
    readGame(kMouseRmb, &rmb, 4);
    if (s.wroteTilt < 0.0f && std::isfinite(to) && std::fabs(to - s.wroteTilt) > 0.01f && !rmb)
        s.look.pitch = std::clamp(s.look.pitch + (to - s.wroteTilt), -view3d::kPitchMost, view3d::kPitchMost);
}

// 3.0.19-beta.12 (R-ZQWGVD: behind, the eye in Morocc's buildings): the map's models on the way back from the head -
// the world's tree of the map (CView +0x1b4 the world, its tree's root at +0x68, FUN_004f9e60 / FUN_004fa1c0): a node's
// four children at +4..+0x10, its box (+0x2c the centre, +0x38 the half sizes), the leaves at the depth 5 with their
// models (+0x4c..+0x50); a model's own box at +0x130 (FUN_004fa0f0)
constexpr uint32_t kViewWorld = 0x1B4, kWorldTree = 0x68, kNodeCentre = 0x2C, kNodeHalf = 0x38, kNodeModels = 0x4C;
constexpr uint32_t kModelBox = 0x130;
constexpr int kTreeDepth = 5;

bool finiteAll(const float* f, int n, float most) {
    for (int i = 0; i < n; ++i)
        if (!std::isfinite(f[i]) || std::fabs(f[i]) > most) return false;
    return true;
}

bool modelBox(uint32_t model, view3d::Box& b) {
    float f[15];
    if (!readGame(model + kModelBox, f, sizeof f) || !finiteAll(f, 15, 1e5f)) return false;
    for (int i = 0; i < 3; ++i) {
        b.half[i] = f[i];
        b.centre[i] = f[3 + i];
        for (int k = 0; k < 3; ++k) b.axis[i][k] = f[6 + 3 * i + k];
    }
    float most = 0.0f;  // its largest half size in the world's units: the small things (a barrel, a sign) left out
    for (int i = 0; i < 3; ++i) {
        const float* a = b.axis[i];
        most = std::max(most, std::fabs(b.half[i]) * std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]));
    }
    return most >= 3.0f && most < 5000.0f;
}

void boxesOnTheWay(uint32_t node, int depth, const float head[3], const float eye[3], std::vector<view3d::Box>& out,
                   std::vector<uint32_t>& seen, int& budget) {
    if (!node || --budget < 0) return;
    view3d::Box nb;
    float c[3], h[3];
    if (!readGame(node + kNodeCentre, c, sizeof c) || !readGame(node + kNodeHalf, h, sizeof h) ||
        !finiteAll(c, 3, 1e6f) || !finiteAll(h, 3, 1e6f))
        return;
    for (int i = 0; i < 3; ++i) {
        nb.centre[i] = c[i];
        nb.half[i] = std::fabs(h[i]) + (i == 1 ? 200.0f : 20.0f);  // the tall models out of the ground's own box
    }
    if (!view3d::crosses(nb, head, eye)) return;
    if (depth >= kTreeDepth) {
        uint32_t first = 0, last = 0;
        if (!readGame(node + kNodeModels, &first, 4) || !readGame(node + kNodeModels + 4, &last, 4) || last < first ||
            last - first > 4 * 4096)
            return;
        for (uint32_t p = first; p < last; p += 4) {
            uint32_t model = 0;
            if (!readGame(p, &model, 4) || !model || std::find(seen.begin(), seen.end(), model) != seen.end()) continue;
            seen.push_back(model);
            view3d::Box mb;
            if (modelBox(model, mb)) out.push_back(mb);
        }
        return;
    }
    for (uint32_t k = 0; k < 4; ++k) {
        uint32_t kid = 0;
        if (readGame(node + 4 + 4 * k, &kid, 4)) boxesOnTheWay(kid, depth + 1, head, eye, out, seen, budget);
    }
}

// The game's ground now (the build's own: the mode's world's), 0: none
uint32_t groundNow() {
    const uint32_t mode = readGameMode();
    uint32_t world = 0, ground = 0;
    if (!mode || !readGame(mode + kModeWorld, &world, 4) || !world || !readGame(world + kWorldGround, &ground, 4))
        return 0;
    return ground;
}

// Its y at (x, z), y down (out of the map: 0, the game's own answer)
float groundY(uint32_t ground, float x, float z) {
    return reinterpret_cast<HeightFn>(uintptr_t(kGroundHeight))(ground, x, z);
}

// The eye 2 over the ground under it at least (the game's build keeps its own eye over it the same way)
void overGround(uint32_t ground, float eye[3]) {
    if (!ground) return;
    const float h = groundY(ground, eye[0], eye[2]);
    if (std::isfinite(h) && eye[1] > h - 2.0f) eye[1] = h - 2.0f;  // y down
}

void switchTo(State& s, uint32_t view) {
    const view3d::Mode from = s.mode, to = s.wanted;
    s.mode = to;
    s.frames = 0;
    s.centred = false;
    if (from == view3d::Mode::Off && to != view3d::Mode::Off) {
        s.look.yaw = *at(view, kYawTo);  // from the game's own turn
        s.look.pitch = to == view3d::Mode::First ? 0.0f : -12.0f;
        s.wroteYaw = -1.0f;
        s.wroteTilt = 0.0f;
        s.behindNow = 0.0f;
        s.zoomAtEntry = *at(view, kDistanceTo);  // 3.0.19-beta.15
        s.walkBefore = walkmodewin::on();
        walkmodewin::setOn(true);
    } else if (to == view3d::Mode::Off) {
        walkmodewin::setOn(s.walkBefore);
        release(s);  // 3.0.19-beta.11: the cursor the Windows one again, at the crosshair
        s.locked = s.aimed = 0;
    } else if (to == view3d::Mode::First) {  // 3.0.19-beta.15: from behind looking down - level, not at the feet
        s.look.pitch = std::clamp(s.look.pitch + 12.0f, -25.0f, 25.0f);
    }
    LOG_INFO("3D view: %s - the field of view %.0f, the mouse %.0f %%, the walk mode %s%s", view3d::nameOf(to),
             view3d::fovFor(to, s.fovFirst, s.fovBehind), s.mouseSpeed * 100.0f,
             to != view3d::Mode::Off || s.walkBefore ? "on" : "off",
             to != view3d::Mode::Off ? "; the grass not drawn in it" : "");
}

void tell(State& s, uint32_t view, const float eye[3], const float feet[3], const float up[3]) {
    const DWORD now = GetTickCount();
    if (s.frames != 1 && now - s.toldAt < 10000) return;
    s.toldAt = now;
    LOG_INFO("3D view: %s - yaw %.1f, pitch %.1f, the turn %+.0f; the eye %.1f %.1f %.1f over the feet %.1f %.1f %.1f, "
             "up %.2f %.2f %.2f; the game's tilt %.1f, distance %.1f (behind %.1f); %d frames",
             view3d::nameOf(s.mode), s.look.yaw, s.look.pitch, s.sign, eye[0], eye[1], eye[2], feet[0], feet[1],
             feet[2], up[0], up[1], up[2], *at(view, 0x1C), *at(view, 0x24), s.behindNow, s.frames);
    // 3.0.19-beta.15: the mouse in these seconds - the cursor's pixels (or counts) that turned it, its most a frame,
    // the jumps dropped, DirectInput's counts beside
    LOG_INFO("3D view: the mouse - %ld moved (%s), %ld at most a frame, %ld jump(s) dropped; DirectInput's counts %ld",
             s.pixels, s.cursorMoved ? "the cursor's pixels" : "counts", s.mostPixels, s.jumps, s.counts);
    s.pixels = s.counts = s.mostPixels = s.jumps = 0;
}

void __attribute__((thiscall)) build(uint32_t view) {
    State& s = g_state;
    const auto original = reinterpret_cast<BuildFn>(g_build);
    if (s.wanted != s.mode) switchTo(s, view);
    if (s.mode == view3d::Mode::Off) {
        original(view);
        // 3.0.19-beta.14 (R-W5K25A: the world left in the 3D view - a disconnect - and the next map drawn with its 60
        // degrees, our own models off their places): the game's own projection kept each frame (only written when off)
        applyFov(s, view3d::kGameFov);
        return;
    }
    if (s.mode != view3d::Mode::Off) hidePlayerRender();  // 3.0.19-beta.11; .15 behind too (pulled in near)
    mouse(s);
    adoptYaw(view, s);
    adoptTilt(view, s);
    *at(view, kYaw) = s.look.yaw;
    *at(view, kYawTo) = s.look.yaw;
    s.wroteYaw = s.look.yaw;
    *at(view, kTilt) = kHeldTilt;
    *at(view, kTiltTo) = kHeldTilt;
    s.wroteTilt = kHeldTilt;
    original(view);
    float m[12];
    std::memcpy(m, at(view, kView), sizeof m);
    float flat[3];
    if (!view3d::ahead(m, flat)) return;  // the game's own this frame
    s.sign = view3d::turnSign(m, s.look.yaw);
    float feet[3], up[3], eye[3], target[3];
    std::memcpy(feet, at(view, kFeet), sizeof feet);
    std::memcpy(up, at(view, kUp), sizeof up);
    const float want = view3d::behindDistanceOf(*at(view, kDistanceTo), s.zoomAtEntry);  // beta.7/.15: the wheel
    view3d::place(s.mode, feet, flat, up, s.look.pitch, eye, target, want);
    const uint32_t ground = groundNow();
    if (s.mode == view3d::Mode::Behind && ground) {  // 3.0.19-beta.10: a wall, a cliff behind - the eye before it
        const float onGround = view3d::freeDistance(eye, target, want,
                                                    [ground](float x, float z) { return groundY(ground, x, z); });
        // 3.0.19-beta.12: the map's models too (a house, a roof, a wall of a castle)
        float free = onGround;
        uint32_t world = 0;
        float way[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
        const float len = std::sqrt(way[0] * way[0] + way[1] * way[1] + way[2] * way[2]);
        if (readGame(view + kViewWorld, &world, 4) && world && len > 0.0f) {
            const float head[3] = {eye[0] + way[0] / len * want, eye[1] + way[1] / len * want,
                                   eye[2] + way[2] / len * want};
            s.wayBoxes.clear();
            s.waySeen.clear();
            int budget = 2000;
            boxesOnTheWay(world + kWorldTree, 0, head, eye, s.wayBoxes, s.waySeen, budget);
            const float byModels = view3d::freeOfBoxes(eye, target, want, s.wayBoxes);
            if (byModels < onGround - 0.5f && s.modelPullsTold < 10 && byModels < s.behindNow - 0.5f) {
                ++s.modelPullsTold;
                LOG_INFO("3D view: behind - brought in to %.0f of %.0f before a model (%zu model(s) on the way, %zu "
                         "looked at)", byModels, want, s.wayBoxes.size(), s.waySeen.size());
            }
            free = std::min(free, byModels);
        }
        const DWORD tick = GetTickCount();
        if (s.behindNow <= 0.0f || free < s.behindNow) {
            if (s.behindNow > 0.0f && free < s.behindNow - 0.5f) s.pulledAt = tick;
            s.behindNow = free;
        } else if (tick - s.pulledAt >= kOutAfterMs) {  // 3.0.19-beta.15: out again once it stayed clear a while
            s.behindNow += (free - s.behindNow) * 0.12f;
        }
        if (s.behindNow < want - 0.01f)
            view3d::place(s.mode, feet, flat, up, s.look.pitch, eye, target, s.behindNow);
    }
    overGround(ground, eye);
    float nm[12], inv[12];
    view3d::lookAt(eye, target, up, nm);
    if (!camera::plausibleView(nm)) return;
    view3d::inverse(nm, inv);
    std::memcpy(at(view, kEye), eye, sizeof eye);
    std::memcpy(at(view, kView), nm, sizeof nm);
    std::memcpy(at(view, kInverse), inv, sizeof inv);
    applyFov(s, view3d::fovFor(s.mode, s.fovFirst, s.fovBehind));
    ++s.frames;
    tell(s, view, eye, feet, up);
}

}  // namespace

void install() {
    static bool tried = false;
    if (tried) return;
    tried = true;
    g_build = codedetour::detour(kBuild, kBuildCode, sizeof kBuildCode, reinterpret_cast<const void*>(&build));
    if (g_build)
        LOG_INFO("3D view: Ctrl+Shift+V switches the game's camera -> behind -> first person (uaRO.exe 0x4faf00)");
    else
        LOG_INFO("3D view: not here - the camera's build is not where this build of uaRO.exe has it");
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = g_state;
    if (m == WM_CHAR && s.skipChar) {  // Ctrl+Shift+V's letter (0x16)
        s.skipChar = false;
        return wp == 0x16;
    }
    if (m == WM_CHAR && s.skipTab) {  // 3.0.19-beta.11: Tab's own
        s.skipTab = false;
        return wp == '\t';
    }
    // 3.0.19-beta.11 (the concept's decision: «Tab lock YES»): Tab in the 3D view locks the monster nearest the
    // crosshair, again - the next; only with the chat's line empty (typing, its Tab is the chat's channel)
    if (m == WM_KEYDOWN && wp == VK_TAB && s.mode != view3d::Mode::Off && s.looking && GetKeyState(VK_CONTROL) >= 0 &&
        GetKeyState(VK_MENU) >= 0 && GetKeyState(VK_SHIFT) >= 0 && chatwin::focusInChat() && chatwin::inputEmpty() &&
        !walkmodewin::typing()) {
        s.skipTab = true;
        if (((lp >> 30) & 1) == 0) s.wantLock = true;  // held: once a press
        return true;
    }
    if (m != WM_KEYDOWN || wp != 'V' || GetKeyState(VK_CONTROL) >= 0 || GetKeyState(VK_SHIFT) >= 0) return false;
    if (!g_build || !inGameWorld()) return false;
    s.skipChar = true;
    if (((lp >> 30) & 1) != 0) return true;  // held: once a press
    s.wanted = view3d::next(s.wanted);
    LOG_INFO("keys: Ctrl+Shift+V - the view %s", view3d::nameOf(s.wanted));
    return true;
}

void frame(const Config& config) {
    State& s = g_state;
    s.fovFirst = config.view3dFovFirst;
    s.fovBehind = config.view3dFovBehind;
    s.mouseSpeed = float(config.view3dMouse) / 100.0f;
    if (s.mode != view3d::Mode::Off && !inGameWorld()) {  // the login, the character list: the game's own
        s.mode = s.wanted = view3d::Mode::Off;
        walkmodewin::setOn(s.walkBefore);
        release(s);  // 3.0.19-beta.11
        s.locked = s.aimed = 0;
        applyFov(s, view3d::kGameFov);  // 3.0.19-beta.14: the game's renderer's fields too, not only ours
        s.fov = view3d::kGameFov;
        camera::setFov(view3d::kGameFov);
        s.looking = false;
        LOG_INFO("3D view: the game's own camera - the world left");
    }
    const bool clip = s.mode != view3d::Mode::Off && s.looking;
    const HWND game = uaui::gameWindow();
    RECT r;
    if (clip && game && GetClientRect(game, &r)) {
        POINT a{r.left, r.top}, b{r.right, r.bottom};
        ClientToScreen(game, &a);
        ClientToScreen(game, &b);
        const RECT c{a.x, a.y, b.x, b.y};
        ClipCursor(&c);
        s.clipped = true;
    } else if (s.clipped) {
        ClipCursor(nullptr);
        s.clipped = false;
    }
}

bool active() { return g_state.mode != view3d::Mode::Off; }

bool looking() { return g_state.mode != view3d::Mode::Off && g_state.looking; }

bool fromTheEyes() { return g_state.mode == view3d::Mode::First; }

bool ownHidden() { return ownHiddenNow(); }

}  // namespace uagfx::win::view3dwin
