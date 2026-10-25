#pragma once
#include <cstdint>

namespace uagfx::camera {

// Part 3 (DLSS): the camera of uaRO.exe, which projects the world on the CPU (docs: the decompiled ProjectVertex
// 0x412cb0, SetSize 0x40d760, the CView build 0x4faf00). A view-space point (x, y, z) lands at
//   sx = x / z * f0 + cx,  sy = y / z * f1 + cy,  sz = far / (far - near) * (1 - near / z)
// with y pointing down, so every depth the game writes can be turned back into a point and shown where the previous
// frame's camera had it: the motion vectors DLSS needs.
constexpr uint32_t kModeMgr = 0x74B578;
constexpr uint32_t kModeCurrent = 0x04;  // CGameMode*
constexpr uint32_t kModeType = 0x58;     // 1 in the game, 0 at the login
constexpr uint32_t kModeView = 0xC8;     // CView* of the game mode
// 3.0.4-beta.5: CGameMode int[0xa0] - 1 outdoors, 0 on a map of data\indoorrswtable.txt (FUN_0055a230 at the map's
// start): the yaw clamped to 45 +-20, the tilt's and the zoom's ranges (300 at most) the building's
constexpr uint32_t kModeOutdoor = 0x280;
constexpr uint32_t kModeMap = 0x60;      // the map's file name, "prontera.rsw" (copied there as the world loads)
constexpr uint32_t kViewMatrix = 0x94;   // 4 x 3 floats: world -> view for row vectors (D3D), the last row a move
constexpr uint32_t kViewAt = 0x28;       // 3 floats: the point the camera looks at - the character (0x4faf00)
constexpr uint32_t kConstants = 0x695740;  // pi, far, near, horizontal and vertical field of view in degrees
// 2.3: the game's mouse (CMouse, read by DirectInput - Windows' cursor does not follow it in exclusive mode): its x
// and y in the game's pixels, clamped to its picture (uaRO.exe 0x419c60)
constexpr uint32_t kMouse = 0x6EE6F0;
constexpr uint32_t kMouseX = 0x14, kMouseY = 0x18;
// 3.0.4-beta.17: the outdoors' zoom limit (float 2040, in its .data; every zoom of the game clamps the camera's
// distance - CView kViewDistance, its target kViewDistanceTo - to it, uaRO.exe 0x55a314, 0x565d2f, ...) and the one
// the driver holds there: the character this side of the far plane, its height with it (R-35N5QN)
constexpr uint32_t kZoomMost = 0x6D66B4;
constexpr uint32_t kViewDistance = 0x24, kViewDistanceTo = 0x3C;
// 3.0.5-beta.6 (the graphics test): the view's tilt and turn (degrees) as shown and their targets, which the game eases
// them toward by a tenth a frame (FUN_004faca0: the turn kept within 0-360 and turned the short way)
constexpr uint32_t kViewTilt = 0x1C, kViewTiltTo = 0x34;
constexpr uint32_t kViewYaw = 0x20, kViewYawTo = 0x38;
// a map's camera limits of its own (CGameMode int[0xa1]; its distances from the short at +0x288 over the one at
// +0x28a - uaRO.exe 0x565f34), else 230 to 300 on a building map and to kZoomMost outdoors
constexpr uint32_t kModeOwnLimits = 0x284, kModeLimitFrom = 0x288, kModeLimitSpan = 0x28A;
constexpr float kZoomLimit = 1400.0f;
// 3.0.4-beta.20 (R-UBT67Q «отчет с водой у горизонта»): the world the view draws (CView+0x1B4, FUN_004fa8b0) - its
// ground (C3dGround: cells wide, high, units a cell, the water's level and its waves' height, FUN_004e59d0) and the
// root of its quadtree (FUN_004ec430: 6 levels, the leaves the 6th, each a node of 0x80 bytes - its four children, its
// box's centre and half, FUN_004fa060 culls by them against the view's planes, far 1500, FUN_004f9e60)
constexpr uint32_t kViewWorld = 0x1B4;
constexpr uint32_t kWorldGround = 0x38, kWorldRoot = 0x68;
constexpr uint32_t kGroundWidth = 0x08, kGroundHeight = 0x0C, kGroundZoom = 0x10;
constexpr uint32_t kGroundWaterLevel = 0x40, kGroundWaveHeight = 0x54;
constexpr uint32_t kNodeChildren = 0x04, kNodeCentre = 0x2C, kNodeHalf = 0x38;
constexpr int kTreeLevels = 6;
constexpr float kPi = 3.141592025756836f;  // as the exe holds it
constexpr float kFar = 1500.0f;
constexpr float kNear = 10.0f;
constexpr float kFov = 15.0f;

// Whether the five floats at kConstants are this build's: then the addresses above are right.
bool knownConstants(const float values[5]);

// 3.0.4-beta.20: a box of the game's quadtree (its height's centre and half; up is -y) raised to the water's top
// (its level less its waves) when all of it lies under it - the map's .rsw keeps its boxes by the ground and the models
// (FUN_004edac0), the water left out: a part of the sea's bed past the far plane went with the water over it, which
// was not. True when it was raised.
bool raiseToWater(float centre, float half, float waterTop, float& outCentre, float& outHalf);

struct Projection {
    float f0 = 0, f1 = 0;  // view x, y per z -> pixels
    float cx = 0, cy = 0;  // the screen's centre, whole pixels
    float nearZ = kNear, farZ = kFar;
};
// As SetSize makes it for the game's size.
Projection projectionFor(int width, int height);

// 1.4.2: how far in front of the camera the point `at` is (view-space z), by the view matrix.
inline float viewZ(const float view[12], const float at[3]) {
    return at[0] * view[2] + at[1] * view[5] + at[2] * view[8] + view[11];
}

// 2.2.2: the world height (y, down positive) of the point the camera looks at - the character's ground - by the view
// -> world affine (4 x 3, row vectors) and that point's view depth (viewZ; the point is on the view's axis).
inline float targetHeight(const float viewToWorld[12], float targetZ) {
    return targetZ * viewToWorld[7] + viewToWorld[10];
}

// 2.7.22: the world's up (y down positive: -y) in the view, a unit vector - the view's axes in the world are the
// affine's rows, so its view coordinates are their y's, negated.
inline void upInView(const float viewToWorld[12], float up[3]) {
    up[0] = -viewToWorld[1];
    up[1] = -viewToWorld[4];
    up[2] = -viewToWorld[7];
}

// The view matrix of a LookAt: an orthonormal rotation and a move; false for anything else (a stale pointer).
bool plausibleView(const float view[12]);

// cur -> prev in view space: a point seen by the current camera, as the previous one saw it (inverse(cur) * prev).
void reprojection(const float cur[12], const float prev[12], float out[12]);

// A screen point (game pixels, the game's depth) moved by `r`: where the previous camera put it.
void reproject(const Projection& p, const float r[12], float sx, float sy, float sz, float& px, float& py);

// Row-major 4 x 4 for Streamline (row vectors, no jitter): view -> clip as the game projects for its width x height
// (NDC y up), its inverse, and clip -> previous clip.
void viewToClip(const Projection& p, int width, int height, float out[16]);
bool inverse4(const float m[16], float out[16]);
void multiply4(const float a[16], const float b[16], float out[16]);
void affine4(const float m12[12], float out[16]);  // a 4 x 3 affine as 4 x 4

// Part 4 (RTX Remix): Remix path traces the world it is given in world space and takes the camera from Direct3D 9's
// view and projection, by D3D's conventions: a left-handed view space with y up, and a projection the right way up
// (it rejects a camera whose field of view comes out negative). The game's view space has y down, so its world is
// handed over turned: +y up (rtx.zUp = False) and left-handed (rtx.leftHandedCoordinateSystem = True), whichever
// way the game's own axes go. The same turn for the whole run: Remix keeps its history in world space.
struct RemixSpace {
    float flip[3] = {1.0f, -1.0f, 1.0f};  // the game's world -> Remix's, per axis
};
// From a camera of the game: its +y going down the screen means its y points down (uaRO's ground: up is -y).
RemixSpace remixSpaceFor(const float view[12]);
// The point the game drew at (sx, sy) with this 1/z, in Remix's world; false when 1/z is no camera point - a
// picture drawn flat on the screen (1/z of 1 is nearer than the near plane) or none at all.
bool remixPoint(const Projection& p, const float inverseView[16], const RemixSpace& s, float sx, float sy, float rhw,
                float out[3]);
// Row-major 4 x 4 for Direct3D 9's SetTransform (row vectors): Remix's world -> view (x right, y up, z forward),
// and view -> clip as the game projects for width x height.
void remixView(const float view[12], const RemixSpace& s, float out[16]);
void remixProjection(const Projection& p, int width, int height, float out[16]);

}  // namespace uagfx::camera
