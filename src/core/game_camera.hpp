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

}  // namespace uagfx::camera
