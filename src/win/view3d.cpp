#include "win/view3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/game_camera.hpp"
#include "core/view3d.hpp"
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
    bool walkBefore = false;  // the walk mode as it was before the 3D view
    bool centred = false;
    bool looking = false;
    bool clipped = false;
    bool skipChar = false;
    int frames = 0;
    DWORD toldAt = 0;
};

State g_state;  // the game's thread: its window's keys, its frame, its camera build
uintptr_t g_build = 0;

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
        if (fov != s.fov)
            LOG_INFO("3D view: the field of view %.0f degrees - the projection's f1 %.1f -> %.1f (%dx%d)", fov, f[1],
                     want.f1, w, h);
        f[0] = want.f0;
        f[1] = want.f1;
        f[2] = want.slopeH;
        f[3] = want.slopeV;
    }
    s.fov = fov;
    camera::setFov(fov);
}

// The mouse's move from the screen's middle turns the view; the game's cursor held there (its clicks and its hover at
// the crosshair). Alt held, or the window not in front: the cursor free.
void mouse(State& s) {
    const HWND game = uaui::gameWindow();
    s.looking = game && GetForegroundWindow() == game && GetKeyState(VK_MENU) >= 0;
    if (!s.looking) {
        s.centred = false;
        return;
    }
    int w = 0, h = 0;
    if (!readGame(kGameW, &w, 4) || !readGame(kGameH, &h, 4) || w <= 0 || h <= 0) return;
    int* x = reinterpret_cast<int*>(uintptr_t(camera::kMouse + camera::kMouseX));
    int* y = reinterpret_cast<int*>(uintptr_t(camera::kMouse + camera::kMouseY));
    const int cx = w / 2, cy = h / 2;
    if (s.centred) {
        const int dx = *x - cx, dy = *y - cy;
        if (dx || dy) view3d::turn(s.look, float(dx), float(dy), kDegreesPerCount * s.mouseSpeed, s.sign);
    }
    *x = cx;
    *y = cy;
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
        s.walkBefore = walkmodewin::on();
        walkmodewin::setOn(true);
    } else if (to == view3d::Mode::Off) {
        walkmodewin::setOn(s.walkBefore);
    } else if (to == view3d::Mode::First) {
        s.look.pitch = std::clamp(s.look.pitch + 12.0f, -view3d::kPitchMost, view3d::kPitchMost);
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
}

void __attribute__((thiscall)) build(uint32_t view) {
    State& s = g_state;
    const auto original = reinterpret_cast<BuildFn>(g_build);
    if (s.wanted != s.mode) switchTo(s, view);
    if (s.mode == view3d::Mode::Off) {
        original(view);
        if (s.fov != view3d::kGameFov) applyFov(s, view3d::kGameFov);
        return;
    }
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
    const float want = view3d::behindDistanceOf(*at(view, kDistanceTo));  // beta.7: the wheel
    view3d::place(s.mode, feet, flat, up, s.look.pitch, eye, target, want);
    const uint32_t ground = groundNow();
    if (s.mode == view3d::Mode::Behind && ground) {  // 3.0.19-beta.10: a wall, a cliff behind - the eye before it
        const float free = view3d::freeDistance(eye, target, want,
                                                [ground](float x, float z) { return groundY(ground, x, z); });
        s.behindNow = s.behindNow <= 0.0f || free < s.behindNow ? free : s.behindNow + (free - s.behindNow) * 0.12f;
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

}  // namespace uagfx::win::view3dwin
