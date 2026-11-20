#include "win/view3d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

#include "core/game_camera.hpp"
#include "core/settings/strings.hpp"
#include "core/view3d.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_window.hpp"
#include "win/code_detour.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/model_census.hpp"
#include "win/uaui.hpp"
#include "win/walk_mode.hpp"
#include "win/world_mouse.hpp"

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
constexpr long kMostPixels = 1000, kMostCounts = 400;  // .17: 200 dropped a quick flick (R-9KRT3M: 143 a frame)
// 3.0.19-beta.15 (R-W5K25A: in 15, 17, 5, 12, 5 of 78 within two seconds by Morocc's houses): the eye goes out again
// only this long after it was last brought in; nearer than view3d::kOwnNear behind, the character's own picture left
// out (.18: shown again past view3d::kOwnShownAgain - core view3d::ownOut)
constexpr DWORD kOutAfterMs = 500;
// 3.0.19-beta.18 (R-57KBSF «вижу свою или голову пеко ... мигает», «нпс утоплены в пол»): the sprites' depth tilt -
// uaRO.exe's camera update (0x4FACA0) makes each row of a sprite 0.16 tan(tilt) / (height / 480) nearer than its feet
// (0x6B789C; 0.062 a row at 1234 high): an upright figure as its 45-degree camera sees it. From the eyes a sprite near
// by is hundreds of rows tall - its top went tens of units nearer, past the eye (a head cut off, flickering as one
// walks; its upper half over the walls in front of it). 0 in the 3D view, written after the game's update each frame
// (its own back the first frame the view is the game's): upright cards at their feet
constexpr uint32_t kSpriteTilt = 0x6B789C;
// 3.0.19-beta.24 (R-T9FPZZ «лучше но еще не то»): the game's frustum's far (uaRO.exe 0x69C5DC, 1500, read by its
// culling FUN_004f9e60 alone) - in the 3D view [view3d] distance: the game culls its ground and models there, the
// haze whole there (core view3d::hazeReach), the own ground just past it
constexpr uint32_t kCullFar = 0x69C5DC;
// 3.0.21-beta.12 (R-EV2ABB): the step of the sprites the game keeps over everything (core view3d::overAll) - written
// by its camera's update each frame (uaRO.exe 0x4FAE13: 9e-6 x 300 / its distance x sin(tilt) / sin(45)), read at
// the camera's build, after it
constexpr uint32_t kOnTopStepAt = 0x6D5EF4;
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
    float onTopStep = view3d::kOnTopStep;  // 3.0.21-beta.12: the game's of this frame (kOnTopStepAt)
    uint32_t casts = 0;                    // 3.0.21-beta.13: skills cast by a click on the crosshair (the key hints)
    float onTopTold = 0.0f;
    bool ownOut = false;     // 3.0.19-beta.18: the character's own picture left out now (view3d::ownOut)
    int sight = view3d::kSightDefault;  // 3.0.19-beta.24: [view3d] distance
    int sightTold = 0;
    bool uprightTold = false;
    float zoomAtEntry = 0.0f;  // 3.0.19-beta.15: the game's zoom as the 3D view began (the wheel moves from there)
    DWORD pulledAt = 0;        // 3.0.19-beta.15: brought in last - out again only kOutAfterMs later
    std::vector<view3d::Box> wayBoxes;  // 3.0.19-beta.12: the models on the way back this frame
    std::vector<uint32_t> waySeen;
    int modelPullsTold = 0;
    bool walkBefore = false;  // the walk mode as it was before the 3D view
    int castsTold = 0;        // 3.0.19-beta.31: castWaiting's lines
    int wheelsTold = 0;       // 3.0.19-beta.34: the wheel's locks told
    float viewNow[12] = {};   // 3.0.19-beta.36: the view matrix written last (viewAxes)
    bool aimPull = false;     // 3.0.19-beta.36: [view3d] aim_pull
    bool cameraWalls = true;  // 3.0.20-beta.70: [view3d] camera_walls - by the models' own triangles
    // 3.0.19-beta.37 (the owner: «можнол попробовать» - the photo mode with a free camera): Ctrl+Shift+C in the 3D
    // view - the camera off the character, flown by W A S D, Space up, C down (Shift faster)
    bool free = false, wantFree = false, skipFreeChar = false;
    float freeEye[3] = {0, 0, 0};
    float eyeNow[3] = {0, 0, 0};  // the eye of the last frame (the free camera starts there)
    bool eyeKnown = false;
    double flewAt = 0.0;  // 3.0.22-beta.24: precise seconds (preciseNow), 0 none yet
    int freeStopsTold = 0;  // 3.0.19-beta.41: kept off a model
    int freeRockTold = 0;   // 3.0.19-beta.45: kept out of the ground's rock
    // 3.0.22-beta.24 (the owner 19.11 «делай 1-4», 1 - the cinema camera for the channel's videos): the flight's glide,
    // the look shown (eased toward the mouse's, or the flight's), [view3d] free_speed and free_smooth; the flight's
    // points (Ctrl+Shift+P; another map's world clears them), Ctrl+Shift+O flying them since flightAt (pose: this
    // frame's); a note for the screen (settings::Text, its {1})
    view3d::Glide glide;
    view3d::Look shown;
    bool shownKnown = false;
    double lookAt = 0.0;
    float freeScale = 1.0f;
    bool freeSmooth = true;
    std::vector<view3d::Waypoint> points;
    uint32_t pointsWorld = 0;
    bool flying = false;
    double flightAt = 0.0;
    view3d::FlightPose pose;
    bool wantPoint = false, wantFly = false, wantClear = false, skipFlightChar = false;
    int noteText = 0, noteArg = 0;
    uint64_t builds = 0;    // 3.0.19-beta.42: the camera's builds in the 3D view
    // 3.0.19-beta.39: the world (its models' tree) of the last frame, and each actor's models' answer of late
    uint32_t worldNow = 0;
    struct Sight {
        DWORD at = 0;
        bool seen = true;
    };
    std::unordered_map<uint32_t, Sight> sights;
    DWORD sightsSwept = 0;
    // 3.0.19-beta.39: standing, the character turned where the camera looks (faceTheView) - its place of the last
    // frame, when it last fought (attacked, cast, was hit) and when it was turned
    float stoodAt[3] = {0, 0, 0};
    DWORD foughtAt = 0, facedAt = 0;
    int facesTold = 0;
    // 3.0.19-beta.54: behind, the character in the middle unless [view3d] shoulder; Shift held turns the camera round
    // it (core view3d::orbitStep) - the facing waits while it is round or on its way back
    bool shoulder = false;
    view3d::Orbit orbit;
    bool orbitHolds = false;
    int orbitsTold = 0;
    bool viewKnown = false;
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
    // 3.0.22-beta.30 (the owner's video session 20.11: after Pause the view from behind instead of the eyes, turned
    // elsewhere): Pause holds the 3D view (core view3d::Held) - asked (holdAsked) until it ends, the view asked before
    // it, when it began (the flight's clock stands still meanwhile), its first lines of the game's own camera
    view3d::Held held;
    bool holdAsked = false;
    view3d::Mode wantedBeforeHold = view3d::Mode::Off;
    double heldAt = 0.0;
    int heldLines = 0;
    // 3.0.22-beta.32 (the client's MCP «view»): a look asked with the view in the same call - put once it is on
    bool lookPending = false;
    float pendingYaw = 0.0f, pendingPitch = 0.0f;
    // 3.0.22-beta.33 (the MCP's «camera»): a move of the free camera asked - cells along its look
    bool movePending = false;
    float moveForward = 0.0f, moveRight = 0.0f, moveUp = 0.0f;
};

State g_state;  // the game's thread: its window's keys, its frame, its camera build
uintptr_t g_build = 0;
uint32_t g_lastView = 0;  // 3.0.20-beta.3 (lastView)

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

bool ownHiddenNow() {  // 3.0.19-beta.15: from the eyes, or the eye pulled in right behind the head (.18: a margin)
    return g_state.mode != view3d::Mode::Off && g_state.ownOut;
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

// 3.0.19-beta.24: the 3D view's sight - the game's culling far there (its own 1500 back when the view is the game's),
// the renderer told it with the field of view (camera::setSight, the bridge's Camera message)
void applySight(State& s, float units) {
    camera::setSight(units);
    float now = 0.0f;
    if (!readGame(kCullFar, &now, 4) || !(now >= 100.0f && now <= camera::kFar) || now == units) return;
    auto* at = reinterpret_cast<float*>(uintptr_t(kCullFar));
    DWORD old = 0;
    if (!VirtualProtect(at, 4, PAGE_READWRITE, &old)) return;
    *at = units;
    VirtualProtect(at, 4, old, &old);
    if (s.sightTold < 20) {
        ++s.sightTold;
        LOG_INFO("3D view: the game culls its world at %.0f (was %.0f; uaRO.exe 0x%X)", units, now, kCullFar);
    }
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
    if (std::abs(now.x - at.x) > 1 || std::abs(now.y - at.y) > 1) {
        SetCursorPos(at.x, at.y);
        // 3.0.19-beta.36 (the owner: «в режиме от третего лица в 3д чтото мышка какбудто в бок идет»): where it
        // truly stands now - a screen scaled (175 %) puts it a pixel off the point asked, and that pixel, measured
        // from the point asked, was a move every frame: the view turned by itself
        if (!GetCursorPos(&now)) now = at;
    }
    if (where) *where = now;
    return true;
}

// The hold let go (Alt, the window behind, the view off): the Windows cursor where the crosshair was
void release(State& s) {
    if (!s.centred) return;
    s.centred = false;
    s.parkedValid = false;
    uapad::runtime::holdCursor(false, 0, 0);
    uapad::runtime::preferTarget(0);  // 3.0.19-beta.34
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

// 3.0.19-beta.54: the game mode waiting for a skill's target (its +0x3C4, castWaiting's) - Shift is the game's then;
// 3.0.21-beta.13: its kind (2 an enemy's, 4 a friend's, 0 none) for the key hints
int skillWaitKind() {
    constexpr uint32_t kModeClass = 0x69F388, kWait = 0x3C4;
    const uint32_t mode = readGameMode();
    uint32_t vtable = 0, wait = 0;
    if (!mode || !readGame(mode, &vtable, 4) || vtable != kModeClass || !readGame(mode + kWait, &wait, 4)) return 0;
    return int(wait);
}
bool skillWaits() { return skillWaitKind() != 0; }

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
    // 3.0.19-beta.54 (the owner: «и чтобы с шифтом можно было крутить?»): behind, Shift held - not with Ctrl (the
    // view's own keys), not while a skill waits for its target (the game's Shift then), not the free camera (its Shift
    // flies faster) - the camera round the character, its facing waits; let go: back where it was
    const bool shift = s.mode == view3d::Mode::Behind && !s.free && GetKeyState(VK_SHIFT) < 0 &&
                       GetKeyState(VK_CONTROL) >= 0 && !skillWaits();
    const bool wasRound = s.orbit.on;
    s.orbitHolds = view3d::orbitStep(s.orbit, s.look, shift, GetTickCount());
    if (!wasRound && s.orbit.on && s.orbitsTold < 5) {
        ++s.orbitsTold;
        LOG_INFO("3D view: Shift - the camera round the character (its facing waits); let go - back where it was");
    }
    readAims(s);
    lockStep(s, float(cx), float(cy));
    // 3.0.19-beta.34 (the concept: «Колесо: ... от первого лица: смена цели среди тех, что в прицеле»): from the eyes
    // the wheel steps the lock - down the next one, up the one before
    if (const long wheel = uapad::runtime::takeWheel(); wheel != 0 && s.mode == view3d::Mode::First) {
        const uint32_t next = view3d::nextLock(s.aims, float(cx), float(cy), s.locked, wheel < 0 ? 1 : -1);
        if (next != s.locked) {
            s.locked = next;
            s.lockSeen = GetTickCount();
            if (s.wheelsTold < 10) {
                ++s.wheelsTold;
                LOG_INFO("3D view: the wheel - locked on %u (%zu monster(s) on the screen)", next, s.aims.size());
            }
        }
    }
    // 3.0.19-beta.36 (the owner: «автонаводка на мобов раздражает»): pulled only with [view3d] aim_pull
    const view3d::Aim a = view3d::aimAt(s.aims, float(cx), float(cy), s.aimPull ? kPullReach * float(h) : 0.0f,
                                        s.locked, s.aimPull ? kLockReach * float(h) : 0.0f);
    const int ax = int(std::lround(a.x)), ay = int(std::lround(a.y));
    if (a.id != s.aimed && a.id && (ax != cx || ay != cy) && s.pullsTold < 10) {
        ++s.pullsTold;
        LOG_INFO("3D view: the crosshair pulled onto %u%s (%d, %d px off the middle)", a.id,
                 a.id == s.locked ? " (the locked one)" : "", ax - cx, ay - cy);
    }
    s.aimed = a.id;
    uapad::runtime::preferTarget(s.locked ? s.locked : s.aimed);  // 3.0.19-beta.34: the keys' attack and slots
    *x = ax;
    *y = ay;
    s.putX = ax;
    s.putY = ay;
    uapad::runtime::holdCursor(true, ax, ay, s.mode == view3d::Mode::First);
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
constexpr uint32_t kModelRoot = 0x28;  // 3.0.20-beta.70: its root node (win/model_census's kActorNode)
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
            // 3.0.20-beta.70 (the owner's pick 1): a model the game has drawn is known by its own triangles
            // (win/model_census solidsNow) - its box is no wall then
            uint32_t root = 0;
            if (g_state.cameraWalls && readGame(model + kModelRoot, &root, 4) && root &&
                modelcensus::solidsNow().has(root))
                continue;
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

// 3.0.19-beta.43 (R-RRGQ5S: the HP bars through pay_dun00's cave walls - groundY is the walk map's, a wall's cell
// there at the floor): the drawn ground (C3dGround, the world's +0x38 - its cells +0x90, 0x54 bytes each, wide +8,
// high +0xC, units a cell +0x10; a cell's top face +0x40, four corners 0x20 apart, x y z first: uaRO.exe FUN_004E6800)
// - its height at (x, z), the rows tried both ways (the corners tell which holds the place); NaN: none there
constexpr uint32_t kWorldDrawnGround = 0x38, kDrawnCells = 0x90, kDrawnCellBytes = 0x54, kDrawnTop = 0x40;
float drawnGroundY(uint32_t world, float x, float z) {
    uint32_t g = 0, cells = 0;
    int32_t size[2] = {0, 0};
    float zoom = 0.0f;
    if (!world || !readGame(world + kWorldDrawnGround, &g, 4) || !g || !readGame(g + 8, size, sizeof size) ||
        !readGame(g + 0x10, &zoom, 4) || !readGame(g + kDrawnCells, &cells, 4) || !cells || size[0] <= 0 ||
        size[0] > 4096 || size[1] <= 0 || size[1] > 4096)
        return std::numeric_limits<float>::quiet_NaN();
    for (const bool flipped : {false, true}) {
        int col = 0, row = 0;
        if (!view3d::cellOf(size[0], size[1], zoom, x, z, flipped, col, row)) continue;
        uint32_t face = 0;
        const uint32_t cell = cells + (uint32_t(size[0]) * uint32_t(row) + uint32_t(col)) * kDrawnCellBytes;
        if (!readGame(cell + kDrawnTop, &face, 4) || !face) continue;
        float corners[4][3];
        bool read = true;
        for (int k = 0; k < 4 && read; ++k) read = readGame(face + uint32_t(k) * 0x20, corners[k], sizeof corners[k]);
        float y = 0.0f;
        if (read && view3d::heightOnFace(corners, x, z, y)) return y;
    }
    return std::numeric_limits<float>::quiet_NaN();
}

// 3.0.19-beta.43/.45 (R-RRGQ5S): the 3D view's ground - the drawn ground's height (a cave's walls) and the walk
// map's (a bridge's deck), the higher of the two where both are known (y down); NaN: neither
float heightNow(uint32_t ground, uint32_t world, float x, float z) {
    const float walk = ground ? groundY(ground, x, z) : std::numeric_limits<float>::quiet_NaN();
    const float drawn = drawnGroundY(world, x, z);
    return std::isfinite(drawn) && std::isfinite(walk) ? std::min(drawn, walk) : std::isfinite(drawn) ? drawn : walk;
}

// The eye 2 over the ground under it at least (the game's build keeps its own eye over it the same way); .45 by
// heightNow - over a cave's rock too
void overGround(uint32_t ground, uint32_t world, float eye[3]) {
    if (!ground && !world) return;
    const float h = heightNow(ground, world, eye[0], eye[2]);
    if (std::isfinite(h) && eye[1] > h - 2.0f) eye[1] = h - 2.0f;  // y down
}

// 3.0.19-beta.18: the sprites stand upright in the 3D view (kSpriteTilt 0 after the game's camera update)
void uprightSprites(State& s) {
    float tilt = 0.0f;
    if (!readGame(kSpriteTilt, &tilt, 4) || !std::isfinite(tilt) || tilt < 0.0f || tilt > 1.0f) return;
    if (!s.uprightTold) {
        s.uprightTold = true;
        LOG_INFO("3D view: the sprites upright - the game's depth tilt %.4f a row (0x%X) 0 in the 3D view", tilt,
                 kSpriteTilt);
    }
    *reinterpret_cast<float*>(uintptr_t(kSpriteTilt)) = 0.0f;
}

// 3.0.22-beta.30: the game's camera fields the 3D view writes - its yaw and the yaw's target, the tilt and its target,
// the distance and its target (the wheel's)
constexpr uint32_t kHeldFields[6] = {kYaw, kYawTo, kTilt, kTiltTo, 0x24, kDistanceTo};

double preciseNow();

void switchTo(State& s, uint32_t view) {
    const view3d::Mode from = s.mode, to = s.wanted;
    // 3.0.22-beta.30: Pause - the game's own camera meanwhile, all of the 3D view kept (Held) and the fields it wrote
    if (to == view3d::Mode::Off && from != view3d::Mode::Off && s.holdAsked) {
        float fields[6];
        for (int i = 0; i < 6; ++i) fields[i] = *at(view, kHeldFields[i]);
        uint32_t world = 0;
        readGame(view + kViewWorld, &world, 4);
        s.held = view3d::hold(from, s.free, world, fields);
        s.heldAt = preciseNow();
        s.heldLines = 0;
        release(s);
        walkmodewin::setOn(s.walkBefore);
        s.locked = s.aimed = 0;
        s.mode = view3d::Mode::Off;
        s.frames = 0;
        s.centred = false;
        LOG_INFO("3D view: held for Pause - %s%s, yaw %.1f, pitch %.1f, the game's distance %.1f (behind %.1f): the "
                 "same view back after it", view3d::nameOf(from), s.free ? " with the free camera" : "", s.look.yaw,
                 s.look.pitch, fields[5], s.behindNow);
        return;
    }
    if (from == view3d::Mode::Off && s.held.on) {
        uint32_t world = 0;
        readGame(view + kViewWorld, &world, 4);
        const view3d::Held h = s.held;
        s.held = view3d::Held{};
        if (view3d::resumesHeld(h, to, world)) {  // back the same: the game's fields as the view left them
            for (int i = 0; i < 6; ++i) *at(view, kHeldFields[i]) = h.fields[i];
            s.free = h.free;
            if (s.flying) s.flightAt += preciseNow() - s.heldAt;  // the flight goes on from where it stood
            s.flewAt = 0.0;
            s.mode = to;
            s.frames = 0;
            s.centred = false;
            s.walkBefore = walkmodewin::on();
            walkmodewin::setOn(true);
            LOG_INFO("3D view: back from Pause as held - %s%s, yaw %.1f, pitch %.1f", view3d::nameOf(to),
                     s.free ? " with the free camera" : "", s.look.yaw, s.look.pitch);
            return;
        }
        LOG_INFO("3D view: back from Pause on another map (or another view asked) - the view %s anew",
                 view3d::nameOf(to));
    }
    // 3.0.19-beta.18 (R-57KBSF «после выхода с режима 3д мышка перестает работать»): the hold let go while it is still
    // known - release() asks `centred`, cleared below; Ctrl+Shift+V back to the game's own left the pad runtime holding
    // the game's cursor at the crosshair
    if (to == view3d::Mode::Off) release(s);
    s.free = false;  // 3.0.19-beta.37: another view - the camera with the character again
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
        walkmodewin::setOn(s.walkBefore);  // the cursor the Windows one again, at the crosshair: above (.11, .18)
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

// 3.0.22-beta.24 (the owner 19.11 «делай 1-4», 1 - the cinema camera): the flight's keys taken (a point at the eye and
// the look shown; the flight started or stopped; the points cleared), then the look shown this frame - the flight's
// pose, or eased toward the mouse's in the free camera ([view3d] free_smooth), else the mouse's own
// 3.0.22-beta.24: the camera's own clock - the performance counter's seconds (GetTickCount's 16 ms steps made a flight
// shiver on a recording)
double preciseNow() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return double(c.QuadPart) / double(f.QuadPart > 0 ? f.QuadPart : 1);
}

void lookShown(State& s) {
    const double now = preciseNow();
    const double gap = s.lookAt > 0.0 ? now - s.lookAt : 1.0;
    const float seconds = float(std::min(gap, 0.1));
    s.lookAt = now;
    if (gap > 0.25) s.shown = s.look;  // a pause (the 3D view off, a load): no swing back to an old look
    if (!s.points.empty() && s.worldNow != s.pointsWorld) {  // another map: its own points
        LOG_INFO("3D view: the flight's %zu point(s) cleared - another map", s.points.size());
        s.points.clear();
        s.flying = false;
    }
    const auto note = [&s](settings::Text text, int arg) {
        s.noteText = int(text);
        s.noteArg = arg;
    };
    if (s.wantPoint) {
        s.wantPoint = false;
        if (int(s.points.size()) >= view3d::kWaypointsMost) {
            note(settings::Text::FlightMostNote, 0);
        } else {
            view3d::Waypoint w;
            std::copy(s.freeEye, s.freeEye + 3, w.eye);
            w.yaw = s.shown.yaw;
            w.pitch = s.shown.pitch;
            s.points.push_back(w);
            s.pointsWorld = s.worldNow;
            note(settings::Text::FlightPointNote, int(s.points.size()));
            LOG_INFO("3D view: flight point %zu at (%.0f, %.0f, %.0f), yaw %.0f, pitch %.0f", s.points.size(),
                     w.eye[0], w.eye[1], w.eye[2], w.yaw, w.pitch);
        }
    }
    if (s.wantClear) {
        s.wantClear = false;
        s.points.clear();
        s.flying = false;
        note(settings::Text::FlightClearedNote, 0);
        LOG_INFO("3D view: the flight's points cleared (Ctrl+Shift+L)");
    }
    if (s.wantFly) {
        s.wantFly = false;
        if (s.flying) {
            s.flying = false;
            LOG_INFO("3D view: the flight stopped (Ctrl+Shift+O)");
        } else if (s.points.size() < 2) {
            note(settings::Text::FlightTwoPointsNote, 0);
        } else {
            s.flying = true;
            s.flightAt = now;  // precise seconds
            s.glide = view3d::Glide{};
            LOG_INFO("3D view: the flight through %zu points, %.1f s at %d %% of the speed (Ctrl+Shift+O)",
                     s.points.size(), view3d::flightSeconds(s.points, view3d::kFreeSpeed * s.freeScale),
                     int(s.freeScale * 100.0f + 0.5f));
        }
    }
    if (s.free && s.flying) {
        s.pose = view3d::flightAt(s.points, float(now - s.flightAt), view3d::kFreeSpeed * s.freeScale);
        s.shown.yaw = s.pose.yaw;
        s.shown.pitch = s.pose.pitch;
        s.look = s.shown;  // the mouse goes on from here after it
        if (s.pose.done) {
            s.flying = false;
            LOG_INFO("3D view: the flight done - at its last point");
        }
    } else if (s.free && s.freeSmooth && s.shownKnown) {
        view3d::easeLook(s.shown, s.look, seconds);
    } else {
        s.shown = s.look;
    }
    s.shownKnown = true;
}

void __attribute__((thiscall)) build(uint32_t view) {
    g_lastView = view;  // 3.0.20-beta.3
    State& s = g_state;
    const auto original = reinterpret_cast<BuildFn>(g_build);
    // 3.0.21-beta.12 (R-EV2ABB «не видна анимация урона»): the over-everything sprites' step of this frame - the
    // game's camera update (before this build) wrote it from its distance, which the 3D view's wheel moves
    s.onTopStep = *reinterpret_cast<const float*>(uintptr_t(kOnTopStepAt));
    if (s.mode != view3d::Mode::Off && std::fabs(s.onTopStep - s.onTopTold) > 0.25f * s.onTopTold) {
        LOG_INFO("3D view: the game's step of the sprites kept over everything %.3g (9e-06 at its distance 300) - "
                 "the damage numbers, Miss and the target's cursor known by it", s.onTopStep);
        s.onTopTold = s.onTopStep;
    }
    if (s.wanted != s.mode) switchTo(s, view);
    if (s.lookPending && s.mode != view3d::Mode::Off) {  // 3.0.22-beta.32: the MCP's look with its view
        s.lookPending = false;
        setLook(s.pendingYaw, s.pendingPitch);
    }
    // 3.0.19-beta.18: the last frame's distance; 3.0.19-beta.37: the free camera's as from behind (seen, unless near)
    s.ownOut = view3d::ownOut(s.free ? view3d::Mode::Behind : s.mode, s.behindNow, s.ownOut);
    if (s.mode == view3d::Mode::Off) {
        original(view);
        // 3.0.19-beta.14 (R-W5K25A: the world left in the 3D view - a disconnect - and the next map drawn with its 60
        // degrees, our own models off their places): the game's own projection kept each frame (only written when off)
        applyFov(s, view3d::kGameFov);
        applySight(s, camera::kFar);  // 3.0.19-beta.24: the game's own culling
        // 3.0.22-beta.30 (the owner: «при включении Pause около 1.5 с мелькают странные ракурсы сверху»): the game's
        // own camera in Pause's first seconds - a line each half a second
        if (s.held.on && s.heldLines < 4 && preciseNow() - s.heldAt >= 0.5 * s.heldLines) {
            ++s.heldLines;
            LOG_INFO("3D view: held for Pause, %.1f s - the game's own camera: yaw %.1f (to %.1f), tilt %.1f "
                     "(to %.1f), distance %.1f (to %.1f)", preciseNow() - s.heldAt, *at(view, kYaw), *at(view, kYawTo),
                     *at(view, kTilt), *at(view, kTiltTo), *at(view, 0x24), *at(view, kDistanceTo));
        }
        return;
    }
    applySight(s, float(s.sight));  // 3.0.19-beta.24
    if (s.mode != view3d::Mode::Off) hidePlayerRender();  // 3.0.19-beta.11; .15 behind too (pulled in near)
    uprightSprites(s);  // 3.0.19-beta.18
    mouse(s);
    adoptYaw(view, s);
    adoptTilt(view, s);
    lookShown(s);  // 3.0.22-beta.24: the free camera's eased look, or its flight's
    *at(view, kYaw) = s.shown.yaw;
    *at(view, kYawTo) = s.shown.yaw;
    s.wroteYaw = s.shown.yaw;
    *at(view, kTilt) = kHeldTilt;
    *at(view, kTiltTo) = kHeldTilt;
    s.wroteTilt = kHeldTilt;
    original(view);
    if (!readGame(view + kViewWorld, &s.worldNow, 4)) s.worldNow = 0;  // 3.0.19-beta.39: its models for seenActor
    float m[12];
    std::memcpy(m, at(view, kView), sizeof m);
    float flat[3];
    if (!view3d::ahead(m, flat)) return;  // the game's own this frame
    s.sign = view3d::turnSign(m, s.shown.yaw);
    float feet[3], up[3], eye[3], target[3];
    std::memcpy(feet, at(view, kFeet), sizeof feet);
    std::memcpy(up, at(view, kUp), sizeof up);
    const float want = view3d::behindDistanceOf(*at(view, kDistanceTo), s.zoomAtEntry);  // beta.7/.15: the wheel
    view3d::place(s.mode, feet, flat, up, s.shown.pitch, eye, target, want, s.shoulder);  // .54: centred or not
    const uint32_t ground = groundNow();
    if (s.mode == view3d::Mode::Behind && ground) {  // 3.0.19-beta.10: a wall, a cliff behind - the eye before it
        // 3.0.19-beta.45: by the drawn ground too - a cave's rock wall behind brings the eye in before it
        const uint32_t drawnWorld = s.worldNow;
        const float onGround = view3d::freeDistance(eye, target, want, [ground, drawnWorld](float x, float z) {
            return heightNow(ground, drawnWorld, x, z);
        });
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
            // 3.0.20-beta.70: by the drawn models' own triangles (the eye three units wide); the boxes left are those
            // of models not drawn yet
            const solids::Set& walls = modelcensus::solidsNow();
            const float byBoxes = view3d::freeOfBoxes(eye, target, want, s.wayBoxes);
            const float byWalls =
                s.cameraWalls ? solids::freeOfSolids(eye, target, want, walls, solids::kEyeRadius) : want;
            const float byModels = std::min(byBoxes, byWalls);
            if (byModels < onGround - 0.5f && s.modelPullsTold < 10 && byModels < s.behindNow - 0.5f) {
                ++s.modelPullsTold;
                LOG_INFO("3D view: behind - brought in to %.0f of %.0f before a model: by its own triangles %.0f "
                         "(%zu triangles of %zu nodes known on this map), by the boxes of models not drawn yet %.0f "
                         "(%zu of %zu looked at)",
                         byModels, want, byWalls, walls.triangles(), walls.parts(), byBoxes, s.wayBoxes.size(),
                         s.waySeen.size());
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
            view3d::place(s.mode, feet, flat, up, s.look.pitch, eye, target, s.behindNow, s.shoulder);
    }
    if (s.wantFree) {  // 3.0.19-beta.37: Ctrl+Shift+C
        s.wantFree = false;
        s.free = !s.free && s.eyeKnown;
        if (s.free) std::copy(s.eyeNow, s.eyeNow + 3, s.freeEye);
        s.flewAt = 0.0;
        s.glide = view3d::Glide{};  // 3.0.22-beta.24: from rest; the flight over with the free camera
        s.flying = false;
        LOG_INFO("3D view: the free camera %s", s.free ? "on - W A S D fly it, Space up, C down, Shift faster; the "
                                                         "character stays" : "off - back with the character");
    }
    if (s.free) {
        float f[3], right[3];
        view3d::axesOf(flat, up, s.shown.pitch, f, right);
        const double flewNow = preciseNow();  // 3.0.22-beta.24: precise, as the flight's
        const float seconds = s.flewAt > 0.0 ? float(std::min(flewNow - s.flewAt, 0.1)) : 0.0f;
        s.flewAt = flewNow;
        const auto held = [](int vk) { return GetAsyncKeyState(vk) < 0 ? 1 : 0; };
        const bool keys = !walkmodewin::typing() && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_MENU) >= 0;
        const float before[3] = {s.freeEye[0], s.freeEye[1], s.freeEye[2]};
        if (s.flying) {  // 3.0.22-beta.24: the flight through the points - its pose of this frame (lookShown)
            std::copy(s.pose.eye, s.pose.eye + 3, s.freeEye);
        } else {  // 3.0.22-beta.24: gliding - it gathers speed and comes to rest ([view3d] free_smooth, free_speed)
            const bool on = keys;
            view3d::glideStep(s.freeEye, s.glide, f, right, on ? held('W') - held('S') : 0,
                              on ? held('D') - held('A') : 0, on ? held(VK_SPACE) - held('C') : 0, seconds,
                              held(VK_SHIFT) != 0, s.freeScale, s.freeSmooth, feet);
        }
        if (s.movePending && !s.flying) {  // 3.0.22-beta.33: the MCP's move, cells along the look (y down)
            constexpr float kUnitsACell = 5.0f;
            s.movePending = false;
            for (int k = 0; k < 3; ++k) s.freeEye[k] += (f[k] * s.moveForward + right[k] * s.moveRight) * kUnitsACell;
            s.freeEye[1] -= s.moveUp * kUnitsACell;
            const float d[3] = {s.freeEye[0] - feet[0], s.freeEye[1] - feet[1], s.freeEye[2] - feet[2]};
            const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (len > view3d::kFreeReach)  // within its reach of the feet, as the keys keep it
                for (int k = 0; k < 3; ++k) s.freeEye[k] = feet[k] + d[k] * view3d::kFreeReach / len;
            LOG_INFO("3D view: the free camera moved %.1f ahead, %.1f right, %.1f up (cells; the MCP)", s.moveForward,
                     s.moveRight, s.moveUp);
            s.moveForward = s.moveRight = s.moveUp = 0.0f;
        }
        // 3.0.19-beta.41 (item 7): kept off the map's models (a house, a castle's wall, a tree) - along them it slides
        if (s.worldNow && (before[0] != s.freeEye[0] || before[1] != s.freeEye[1] || before[2] != s.freeEye[2])) {
            std::vector<view3d::Box> boxes;
            std::vector<uint32_t> looked;
            int budget = 1200;
            boxesOnTheWay(s.worldNow + kWorldTree, 0, before, s.freeEye, boxes, looked, budget);
            const float wanted[3] = {s.freeEye[0], s.freeEye[1], s.freeEye[2]};
            view3d::keepOffBoxes(before, s.freeEye, boxes);
            // 3.0.20-beta.70: and off the drawn models' own triangles - through a gate's opening it flies
            if (s.cameraWalls) solids::keepOffSolids(before, s.freeEye, modelcensus::solidsNow());
            if (s.freeStopsTold < 5 && (wanted[0] != s.freeEye[0] || wanted[1] != s.freeEye[1] ||
                                        wanted[2] != s.freeEye[2])) {
                ++s.freeStopsTold;
                LOG_INFO("3D view: the free camera kept off a model (%zu box(es) of models not drawn yet near its "
                         "way, %zu looked at; %zu triangles known)",
                         boxes.size(), looked.size(), modelcensus::solidsNow().triangles());
            }
        }
        // 3.0.19-beta.45: and out of the ground's rock rising over it (a cave's walls are the ground's, not models)
        if (before[0] != s.freeEye[0] || before[2] != s.freeEye[2]) {
            const uint32_t walk = groundNow(), drawnWorld = s.worldNow;
            const float wanted[3] = {s.freeEye[0], s.freeEye[1], s.freeEye[2]};
            view3d::keepOverGround(before, s.freeEye, [walk, drawnWorld](float x, float z) {
                return heightNow(walk, drawnWorld, x, z);
            });
            if (s.freeRockTold < 5 && (wanted[0] != s.freeEye[0] || wanted[2] != s.freeEye[2])) {
                ++s.freeRockTold;
                LOG_INFO("3D view: the free camera kept out of the ground's rock (a wall over the eye)");
            }
        }
        std::copy(s.freeEye, s.freeEye + 3, eye);
        for (int i = 0; i < 3; ++i) target[i] = eye[i] + f[i] * 10.0f;
        const float head[3] = {feet[0], feet[1] - view3d::kHeadHeight, feet[2]};  // y down
        s.behindNow = std::sqrt((eye[0] - head[0]) * (eye[0] - head[0]) + (eye[1] - head[1]) * (eye[1] - head[1]) +
                                (eye[2] - head[2]) * (eye[2] - head[2]));  // the own picture hidden only near it
    }
    overGround(ground, s.worldNow, eye);
    if (s.free) std::copy(eye, eye + 3, s.freeEye);  // kept over the ground
    std::copy(eye, eye + 3, s.eyeNow);
    s.eyeKnown = true;
    float nm[12], inv[12];
    view3d::lookAt(eye, target, up, nm);
    if (!camera::plausibleView(nm)) return;
    view3d::inverse(nm, inv);
    std::memcpy(at(view, kEye), eye, sizeof eye);
    std::memcpy(at(view, kView), nm, sizeof nm);
    std::memcpy(at(view, kInverse), inv, sizeof inv);
    std::memcpy(s.viewNow, nm, sizeof nm);  // 3.0.19-beta.36: its axes for the threat's arc
    s.viewKnown = true;
    ++s.builds;  // 3.0.19-beta.42: the ground's turns (win/ground_thin)
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
    if (m == WM_CHAR && s.skipFreeChar) {  // 3.0.19-beta.37: Ctrl+Shift+C's letter (0x03)
        s.skipFreeChar = false;
        return wp == 0x03;
    }
    if (m == WM_CHAR && s.skipFlightChar) {  // 3.0.22-beta.24: Ctrl+Shift+P, O, L's letters
        s.skipFlightChar = false;
        return wp == 0x10 || wp == 0x0F || wp == 0x0C;
    }
    // 3.0.22-beta.24 (the cinema camera): in the free camera Ctrl+Shift+P a flight point, O the flight, L clears them
    if (m == WM_KEYDOWN && (wp == 'P' || wp == 'O' || wp == 'L') && s.free && s.mode != view3d::Mode::Off &&
        GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0 && GetKeyState(VK_MENU) >= 0) {
        s.skipFlightChar = true;
        if (((lp >> 30) & 1) == 0) (wp == 'P' ? s.wantPoint : wp == 'O' ? s.wantFly : s.wantClear) = true;
        return true;
    }
    // 3.0.19-beta.37: Ctrl+Shift+C - the free camera on and off in the 3D view
    if (m == WM_KEYDOWN && wp == 'C' && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0 &&
        GetKeyState(VK_MENU) >= 0 && s.mode != view3d::Mode::Off) {
        s.skipFreeChar = true;
        if (((lp >> 30) & 1) == 0) s.wantFree = true;  // held: once a press
        return true;
    }
    // while free its flight's keys are not the walk mode's (the character stays)
    if (s.free && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_MENU) >= 0 && !walkmodewin::typing()) {
        const bool flight = wp == 'W' || wp == 'A' || wp == 'S' || wp == 'D' || wp == 'C' || wp == VK_SPACE;
        if ((m == WM_KEYDOWN || m == WM_KEYUP) && flight) return true;
        if (m == WM_CHAR && (wp == 'w' || wp == 'a' || wp == 's' || wp == 'd' || wp == 'c' || wp == ' ' ||
                             wp == 'W' || wp == 'A' || wp == 'S' || wp == 'D' || wp == 'C'))
            return true;
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
    if (s.holdAsked) {  // 3.0.22-beta.30: under Pause the old client's own camera - the view held stays as it is
        LOG_INFO("keys: Ctrl+Shift+V under Pause - the 3D view held as it was");
        return true;
    }
    s.wanted = view3d::next(s.wanted);
    LOG_INFO("keys: Ctrl+Shift+V - the view %s", view3d::nameOf(s.wanted));
    return true;
}

// 3.0.19-beta.39 (the owner's «делаем все по порядку»): standing, the character turned where the camera looks (core
// view3d::faceToward) - its facing (actor + 0x38) put as the game's /bangbang puts it, the server told by the game
// mode's message 0x98 (the direction, the head straight: FUN_00567900). Only while it stands where it stood a frame
// before in its standing or ready pose (its action - actor + 0x24 - of kind 0 or 4, the act's 8 sides a kind) and
// kFaceAfterMs past its last attack, cast or hit (a fight turns it to its target), at most each kFaceEveryMs; never
// with the free camera, nor off the game's thread
void faceTheView(State& s, bool on) {
    constexpr uint32_t kModeClass = 0x69F388, kAction = 0x24, kFacing = 0x38, kPlace = 4;
    constexpr DWORD kFaceEveryMs = 250, kFaceAfterMs = 2000;
    constexpr int kTurnMsg = 0x98;
    using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, int msg, int a, int b, int c);
    if (!on || s.mode == view3d::Mode::Off || s.free || !s.viewKnown) return;
    const HWND game = uaui::gameWindow();
    if (!game || GetWindowThreadProcessId(game, nullptr) != GetCurrentThreadId()) return;
    const uint32_t player = playerNow();
    uint32_t action = 0;
    float facing = 0, place[3] = {0, 0, 0};
    if (!player || !readGame(player + kAction, &action, 4) || !readGame(player + kFacing, &facing, 4) ||
        !readGame(player + kPlace, place, sizeof place))
        return;
    const DWORD now = GetTickCount();
    const uint32_t kind = action / 8;
    if (kind == 5 || kind == 6 || (kind >= 10 && kind <= 12)) s.foughtAt = now;  // attacks, a hit, a cast
    const bool still = place[0] == s.stoodAt[0] && place[1] == s.stoodAt[1] && place[2] == s.stoodAt[2];
    std::memcpy(s.stoodAt, place, sizeof place);
    if (!still || (kind != 0 && kind != 4) || now - s.foughtAt < kFaceAfterMs || now - s.facedAt < kFaceEveryMs)
        return;
    float right[2], forward[2];
    view3d::Facing f;
    if (!viewAxes(right, forward) || !view3d::faceToward(forward[0], forward[1], facing, f)) return;
    const uint32_t mode = readGameMode();
    uint32_t vtable = 0, fn = 0;
    if (!mode || !readGame(mode, &vtable, 4) || vtable != kModeClass || !readGame(vtable + 0x14, &fn, 4) ||
        fn < 0x401000 || fn >= 0x6A0000 || !writeGame(player + kFacing, &f.angle, 4))
        return;
    s.facedAt = now;
    reinterpret_cast<ModeMsgFn>(uintptr_t(fn))(mode, kTurnMsg, f.dir, 0, 0);
    if (s.facesTold < 6) {
        ++s.facesTold;
        LOG_INFO("3D view: the character turned where the camera looks - %.0f degrees (it faced %.0f), the server's "
                 "direction %d", double(f.angle), double(facing), f.dir);
    }
}

void frame(const Config& config) {
    State& s = g_state;
    s.fovFirst = config.view3dFovFirst;
    s.fovBehind = config.view3dFovBehind;
    s.mouseSpeed = float(config.view3dMouse) / 100.0f;
    s.sight = config.view3dDistance;  // 3.0.19-beta.24
    s.aimPull = config.view3dAimPull;  // 3.0.19-beta.36
    if (config.effects.cameraWalls3d != s.cameraWalls) {  // 3.0.20-beta.70
        s.cameraWalls = config.effects.cameraWalls3d;
        LOG_INFO("3D view: [view3d] camera_walls = %d - the camera kept out of the models by %s",
                 s.cameraWalls ? 1 : 0, s.cameraWalls ? "their own triangles as the game drew them" : "their boxes");
    }
    s.shoulder = config.view3dShoulder;  // 3.0.19-beta.54
    s.freeScale = float(config.view3dFreeSpeed) / 100.0f;  // 3.0.22-beta.24
    s.freeSmooth = config.view3dFreeSmooth;
    if (!inGameWorld() && (s.held.on || s.holdAsked)) {  // 3.0.22-beta.30: the world left under Pause - not held
        s.held = view3d::Held{};
        s.wantedBeforeHold = view3d::Mode::Off;
    }
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
    faceTheView(s, config.view3dFace && !s.orbitHolds);  // 3.0.19-beta.39; .54: not while Shift turns it round
}

bool available() { return g_build != 0; }

view3d::Mode wanted() { return g_state.wanted; }

bool hold(bool on, const char* why) {
    State& s = g_state;
    if (on) {
        if (s.holdAsked || !g_build || !inGameWorld()) return false;
        s.holdAsked = true;
        s.wantedBeforeHold = s.wanted;
        if (s.wanted == view3d::Mode::Off && s.mode == view3d::Mode::Off) return true;  // the game's own: nothing
        s.wanted = view3d::Mode::Off;  // held at the camera's next build (switchTo)
        LOG_INFO("3D view: %s - the view %s held", why, view3d::nameOf(s.wantedBeforeHold));
        return true;
    }
    if (!s.holdAsked) return false;
    s.holdAsked = false;
    // the view held back (or the one asked before a build held it); none once the world was left meanwhile
    const view3d::Mode back = s.held.on ? s.held.mode : s.wantedBeforeHold;
    s.wantedBeforeHold = view3d::Mode::Off;
    if (back == view3d::Mode::Off || !g_build || !inGameWorld()) return true;
    s.wanted = back;
    LOG_INFO("3D view: %s - the view %s back", why, view3d::nameOf(back));
    return true;
}

bool ask(view3d::Mode m, const char* why) {
    State& s = g_state;
    if (!g_build || !inGameWorld()) return false;
    if (s.holdAsked) return false;  // 3.0.22-beta.30: Pause holds it
    if (m == s.wanted) return true;
    s.wanted = m;
    LOG_INFO("3D view: %s - the view %s", why, view3d::nameOf(m));
    return true;
}

bool active() { return g_state.mode != view3d::Mode::Off; }

float onTopStep() { return g_state.onTopStep; }

bool behind() { return g_state.mode == view3d::Mode::Behind; }

bool freeCamera() { return g_state.mode != view3d::Mode::Off && g_state.free; }
int flightPoints() { return int(g_state.points.size()); }  // 3.0.22-beta.24
bool flying() { return g_state.mode != view3d::Mode::Off && g_state.free && g_state.flying; }
bool takeNote(int& text, int& arg) {
    if (!g_state.noteText) return false;
    text = g_state.noteText;
    arg = g_state.noteArg;
    g_state.noteText = 0;
    return true;
}

int skillWait() { return g_state.mode != view3d::Mode::Off ? skillWaitKind() : 0; }

uint32_t clickCasts() { return g_state.casts; }

bool looking() { return g_state.mode != view3d::Mode::Off && g_state.looking; }

bool fromTheEyes() { return g_state.mode == view3d::Mode::First; }

bool freeOn(bool on) {
    State& s = g_state;
    if (s.mode == view3d::Mode::Off) return false;
    if (s.free != on && !s.wantFree) s.wantFree = true;  // at the next build, Ctrl+Shift+C's way
    return true;
}

bool freeMove(float forward, float right, float up) {
    State& s = g_state;
    if (s.mode == view3d::Mode::Off || (!s.free && !s.wantFree)) return false;
    s.moveForward += forward;
    s.moveRight += right;
    s.moveUp += up;
    s.movePending = true;
    return true;
}

bool flight(const std::string& action) {
    State& s = g_state;
    if (s.mode == view3d::Mode::Off) return false;
    if (action == "clear") {
        s.wantClear = true;
        return true;
    }
    if (!s.free && !s.wantFree) return false;
    if (action == "point") s.wantPoint = true;
    else if (action == "play" && !s.flying) s.wantFly = true;
    else if (action == "stop" && s.flying) s.wantFly = true;
    return true;
}

void flightNow(int& points, bool& flying, float& seconds) {
    const State& s = g_state;
    points = int(s.points.size());
    flying = s.flying;
    seconds = s.points.size() >= 2 ? view3d::flightSeconds(s.points, view3d::kFreeSpeed * s.freeScale) : 0.0f;
}

bool look(float& yaw, float& pitch) {
    const State& s = g_state;
    if (s.mode == view3d::Mode::Off) return false;
    yaw = s.shown.yaw;
    pitch = s.shown.pitch;
    return true;
}

bool setLook(float yaw, float pitch) {
    State& s = g_state;
    if (s.mode == view3d::Mode::Off) {  // the view asked, not on yet: put at its first build
        if (s.wanted == view3d::Mode::Off) return false;
        s.lookPending = true;
        s.pendingYaw = yaw;
        s.pendingPitch = pitch;
        return true;
    }
    if (std::isfinite(yaw)) {
        const float y = std::fmod(yaw, 360.0f);
        s.look.yaw = y < 0.0f ? y + 360.0f : y;
    }
    if (std::isfinite(pitch)) s.look.pitch = std::clamp(pitch, -view3d::kPitchMost, view3d::kPitchMost);
    s.shown = s.look;  // put, not eased
    LOG_INFO("3D view: the look put at yaw %.1f, pitch %.1f (the MCP)", s.look.yaw, s.look.pitch);
    return true;
}

bool ownHidden() { return ownHiddenNow(); }

bool viewDepthOf(const float at[3], float& depth) {
    const State& s = g_state;
    if (s.mode == view3d::Mode::Off || !s.viewKnown) return false;
    depth = camera::viewZ(s.viewNow, at);
    return std::isfinite(depth);
}

uint32_t builds() { return uint32_t(g_state.builds); }
uint32_t lastView() { return g_lastView; }

bool seenActor(uint32_t id, const float to[3]) {
    State& s = g_state;
    if (s.mode == view3d::Mode::Off || !s.eyeKnown) return true;
    // 3.0.19-beta.39: the models on the way (the behind camera's tree walk), each actor's answer kept kSightEveryMs;
    // .43: the ground's too - the drawn ground's heights (a cave's walls), the walk map's where it has none, the higher
    constexpr DWORD kSightEveryMs = 200, kSightForgetMs = 5000;
    const DWORD now = GetTickCount();
    if (now - s.sightsSwept > kSightForgetMs) {
        s.sightsSwept = now;
        for (auto it = s.sights.begin(); it != s.sights.end();)
            it = now - it->second.at > kSightForgetMs ? s.sights.erase(it) : std::next(it);
    }
    State::Sight& sight = s.sights[id];
    if (sight.at == 0 || now - sight.at >= kSightEveryMs) {
        sight.at = now;
        const uint32_t ground = groundNow(), world = s.worldNow;
        const auto heightAt = [ground, world](float x, float z) { return heightNow(ground, world, x, z); };
        if ((ground || world) && !view3d::groundLetsSee(s.eyeNow, to, heightAt)) {
            sight.seen = false;
            return false;
        }
        if (!world) {
            sight.seen = true;
            return true;
        }
        std::vector<view3d::Box> boxes;
        std::vector<uint32_t> looked;
        int budget = 800;
        boxesOnTheWay(s.worldNow + kWorldTree, 0, to, s.eyeNow, boxes, looked, budget);
        sight.seen = std::none_of(boxes.begin(), boxes.end(),
                                  [&](const view3d::Box& b) { return view3d::boxHides(b, s.eyeNow, to); });
    }
    return sight.seen;
}

bool viewAxes(float right[2], float forward[2]) {
    const State& s = g_state;
    if (s.mode == view3d::Mode::Off || !s.viewKnown) return false;
    const float* m = s.viewNow;  // 4 x 3 for row vectors: its columns the view's axes in the world
    const float rl = std::hypot(m[0], m[6]), fl = std::hypot(m[2], m[8]);
    if (rl < 1e-4f || fl < 1e-4f) return false;
    right[0] = m[0] / rl;
    right[1] = m[6] / rl;
    forward[0] = m[2] / fl;
    forward[1] = m[8] / fl;
    return true;
}

// 3.0.19-beta.31: the game mode waiting for a skill's target (its class's vtable 0x69F388; FUN_00562580 - a click on
// a monster): its state +0x3C4 (2 an enemy, 4 a friend), the skill +0x3C8, its range +0x3CC, its level +0x3D0; the
// world +0xC4, its player +0x3C, whose SendMsg (vtable + 8) takes 0x29 (the target, the skill, the range) then 0x5A
// (the level); the mode's SendMsg (vtable + 0x14) 0x4E ends the wait (FUN_00566c80 after every left press). The weight
// past 89 % of the most: no skill (the game says it, message 0x133), 10000 aside
bool castWaiting(bool shift) {
    State& s = g_state;
    if (s.mode == view3d::Mode::Off || !s.looking || !cursorOverWorld()) return false;  // a game's window: its press
    constexpr uint32_t kModeClass = 0x69F388, kWait = 0x3C4, kSkill = 0x3C8, kRange = 0x3CC, kLevel = 0x3D0;
    constexpr uint32_t kPlayer = 0x3C, kOwnAccount = 0x771F7C, kWeight = 0x772048, kWeightMost = 0x772050;
    constexpr int kUseOn = 0x29, kUseLevel = 0x5A, kWaitEnds = 0x4E;
    using ActorMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
    using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, int msg, int a, int b, int c);
    const uint32_t mode = readGameMode();
    uint32_t vtable = 0, wait = 0, own = 0, world = 0, player = 0, playerTable = 0, fn = 0, modeFn = 0;
    int skill = 0, range = 0, level = 0, weight = 0, most = 0;
    if (!mode || !readGame(mode, &vtable, 4) || vtable != kModeClass || !readGame(mode + kWait, &wait, 4) ||
        (wait != 2 && wait != 4) || !readGame(mode + kSkill, &skill, 4) || view3d::companionSkill(skill))
        return false;
    if (skill != 10000 && readGame(kWeight, &weight, 4) && readGame(kWeightMost, &most, 4) &&
        weight * 100 / std::max(most, 1) > 89)
        return false;  // the game's own press: it says why
    if (!readGame(kOwnAccount, &own, 4) || !own) return false;
    const uint32_t target = view3d::skillTargetOf(int(wait), pointedActor(), pointedClass(), s.aimed, own, shift);
    if (!target || !readGame(mode + kRange, &range, 4) || !readGame(mode + kLevel, &level, 4) ||
        !readGame(mode + kModeWorld, &world, 4) || !world || !readGame(world + kPlayer, &player, 4) || !player ||
        !readGame(player, &playerTable, 4) || !readGame(playerTable + 8, &fn, 4) ||
        !readGame(vtable + 0x14, &modeFn, 4))
        return false;
    const auto inExe = [](uint32_t f) { return f >= 0x401000 && f < 0x6A0000; };
    if (!inExe(fn) || !inExe(modeFn)) return false;
    const auto use = reinterpret_cast<ActorMsgFn>(uintptr_t(fn));
    use(player, 0, kUseOn, int(target), skill, range);
    use(player, 0, kUseLevel, level, 0, 0);
    reinterpret_cast<ModeMsgFn>(uintptr_t(modeFn))(mode, kWaitEnds, 0, 0, 0);
    ++s.casts;  // 3.0.21-beta.13
    if (s.castsTold < 10) {
        ++s.castsTold;
        LOG_INFO("3D view: the skill %d Lv %d the game waited a target for (%s) on %s %u at the press%s", skill, level,
                 wait == 2 ? "an enemy" : "a friend", target == own ? "the character itself" : "the crosshair's",
                 target, shift ? " (Shift)" : "");
    }
    return true;
}

}  // namespace uagfx::win::view3dwin
