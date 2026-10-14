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
constexpr uint32_t kModeMap = 0x60;      // the map's file name, "prontera.rsw" (copied there as the world loads)
constexpr uint32_t kViewMatrix = 0x94;   // 4 x 3 floats: world -> view for row vectors (D3D), the last row a move
constexpr uint32_t kConstants = 0x695740;  // pi, far, near, horizontal and vertical field of view in degrees
constexpr float kPi = 3.141592025756836f;  // as the exe holds it
constexpr float kFar = 1500.0f;
constexpr float kNear = 10.0f;
constexpr float kFov = 15.0f;

// Whether the five floats at kConstants are this build's: then the addresses above are right.
bool knownConstants(const float values[5]);

struct Projection {
    float f0 = 0, f1 = 0;  // view x, y per z -> pixels
    float cx = 0, cy = 0;  // the screen's centre, whole pixels
    float nearZ = kNear, farZ = kFar;
};
// As SetSize makes it for the game's size.
Projection projectionFor(int width, int height);

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
