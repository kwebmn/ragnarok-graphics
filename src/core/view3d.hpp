#pragma once
#include <cstdint>

namespace uagfx::view3d {

// 3.0.19-beta.6 (the owner: «давай попробуем, сделай побольше всего ... чтобы сразу получить качественную картинку»;
// concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy, plan docs/superpowers/plans/2026-11-14-view3d.md): the
// 3D view - the game's own camera (uaRO.exe 0x4faf00) put behind the character or in its eyes, the game then drawing
// the world, its sprites (perspective already: 0x412d60) and its clicks through that camera. World y points down.
enum class Mode : uint8_t { Off, Behind, First };
Mode next(Mode m);  // Ctrl+Shift+V: the game's own -> behind -> first person -> the game's own
const char* nameOf(Mode m);

constexpr float kGameFov = 15.0f;    // the game's own vertical field of view (degrees)
constexpr float kFirstFov = 70.0f;   // from the eyes
constexpr float kBehindFov = 60.0f;  // over the shoulder
float fovFor(Mode m);

constexpr float kEyeHeight = 13.0f;      // the eyes over the feet (a character is about 16 high)
constexpr float kHeadHeight = 14.0f;     // behind: the point the camera keeps in front of it
constexpr float kBehindDistance = 45.0f;
constexpr float kShoulder = 9.0f;        // behind: to the right of the head, the character left of the centre
constexpr float kPitchMost = 80.0f;      // up and down

struct Look {
    float yaw = 0.0f;    // degrees, the game's own (CView +0x20), 0-360
    float pitch = 0.0f;  // degrees, up positive
};
// The mouse's move in pixels: dx turns (sign: which way the game's yaw grows on the screen - turnSign), dy up looks up
void turn(Look& l, float dx, float dy, float degreesPerPixel, float sign);

// The way the game's view matrix looks (its forward column) laid flat: false when it looks straight down or up
bool ahead(const float view[12], float out[3]);
// +1 when the game's yaw growing turns the view to the screen's right (the matrix's right column), else -1
float turnSign(const float view[12], float yawDegrees);

// The eye and the point it looks at: `feet` the character's place, `flat` the way ahead laid flat, `up` the game's up
// (CView +0x88), pitch in degrees (up positive); behind: `distance` back from the head (beta.7: the game's wheel)
void place(Mode m, const float feet[3], const float flat[3], const float up[3], float pitch, float eye[3],
           float target[3], float distance = kBehindDistance);
// 3.0.19-beta.7: the game's wheel zoom (CView +0x3c, 230-2040, its start 300) as the distance behind the head
float behindDistanceOf(float gameDistance);

// The game's LookAt (0x409540: right = up x forward, then forward x right; 4 x 3 for row vectors, the last row the
// move) and its inverse (0x409860)
void lookAt(const float eye[3], const float at[3], const float up[3], float out[12]);
void inverse(const float m[12], float out[12]);

// The renderers' sky (sky3d_ps): the horizon's colour - a light haze by day, a deep blue at night, a dungeon's dark
// warm grey - and the haze's reach in world units (the far plane outdoors, nearer under a vault)
struct Sky {
    float horizon[3] = {0, 0, 0};
    float reach = 0;
};
Sky skyOf(float night, bool vault);

}  // namespace uagfx::view3d
