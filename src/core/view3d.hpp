#pragma once
#include <cstdint>
#include <functional>
#include <vector>

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
// 3.0.19-beta.10: by the settings ([view3d] fov_first, fov_behind)
float fovFor(Mode m, int first, int behind);

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
// 3.0.19-beta.12 (R-ZQWGVD: behind, the eye went into Morocc's buildings): the game's boxes of things - a model's
// (its +0x130: the half sizes, the centre, three axes - uaRO.exe FUN_004fa0f0; the axes may carry a scale, the half
// sizes in their units), a ground node's (+0x2c the centre, +0x38 the half sizes, the world's axes)
struct Box {
    float centre[3] = {0, 0, 0};
    float axis[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    float half[3] = {0, 0, 0};
};
// Where the way from `from` to `to` goes into the box: its share of the way (0-1]; -1: it does not, or `from` is in it
float entersAt(const Box& b, const float from[3], const float to[3]);
// Whether any of the way is in the box (`from` in it too)
bool crosses(const Box& b, const float from[3], const float to[3]);
// Behind: the free distance back from the head before the first box the way back enters (a step before it,
// kPullNearest at least); a box the head stands in left out
float freeOfBoxes(const float eye[3], const float target[3], float distance, const std::vector<Box>& boxes);

// 3.0.19-beta.7: the game's wheel zoom (CView +0x3c, 230-2040, its start 300) as the distance behind the head;
// 3.0.19-beta.15 (R-YCGGJR): kBehindDistance whatever the zoom the 3D view began at (`atEntry`), the wheel moving it
// by its share from there, kBehindNearest-kBehindFarthest
constexpr float kBehindNearest = 22.0f, kBehindFarthest = 100.0f;
float behindDistanceOf(float gameDistance, float atEntry);

// 3.0.19-beta.10: behind - how far back from the head the eye can be along the view (`eye` and `target` as place
// made them for `distance`): the first point of the way back closer than kPullMargin over the ground (`groundY(x, z)`
// its y there, y down: a wall, a cliff, the ground when looking up) stops it a step before, kPullNearest at least
constexpr float kPullStep = 2.0f, kPullMargin = 2.0f, kPullNearest = 5.0f;
float freeDistance(const float eye[3], const float target[3], float distance,
                   const std::function<float(float x, float z)>& groundY);

// 3.0.19-beta.11 (the concept's decision: Tab locks, the mouse stays free - the camera never turns by itself, only a
// light pull of the crosshair near the target): the monsters' boxes on the game's screen (its pick list, the game's
// pixels) and the point its cursor is put at - on the monster under the crosshair as it is; one within `reach` pixels
// pulled into (its nearest point, a fifth of its size inside); the locked one from `lockedReach`. Under the crosshair
// first (the locked one of those), then the locked one, then the nearest.
struct AimBox {
    uint32_t id = 0;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
struct Aim {
    float x = 0, y = 0;
    uint32_t id = 0;  // the monster aimed at, 0 none
};
Aim aimAt(const std::vector<AimBox>& boxes, float cx, float cy, float reach, uint32_t locked, float lockedReach);
// Tab: the next to lock after `current` - the monsters by how near the crosshair, round; `current` gone: the nearest;
// none on the screen: 0
uint32_t nextLock(const std::vector<AimBox>& boxes, float cx, float cy, uint32_t current);
// The game's classes it names monsters (as mob_hp's): 1001-3999
bool monsterClass(int cls);
// 3.0.19-beta.31 (the testers: in the 3D view «на себе неможливо накласти бафи, аля хіл ітд, і неможливо скастувати на
// моба скіл який вимагає кліка по мобу» - the own picture is left out, and the crosshair pulled onto a monster moves
// the game's cursor each frame, which forgets the monster it hovered, uaRO.exe FUN_00562580): whom a skill the game
// waits a target for goes to at a left press. `wait` the game mode's state (+0x3C4: 2 an enemy, 4 a friend),
// `pointed`/`pointedClass` the actor the game's pick finds under the crosshair (0 none), `aimed` the monster the
// crosshair is pulled onto, `own` the character, `shift` held. An enemy's skill: a monster or a player under it, else
// the pulled one; a friend's: a player, a homunculus or a mercenary under it, a monster with Shift (as the game wants
// it), else the character itself. 0: the press is the game's as it came (no wait, a place's or a trap's, none)
uint32_t skillTargetOf(int wait, uint32_t pointed, int pointedClass, uint32_t aimed, uint32_t own, bool shift);
// The homunculus' (8000-8016) and the mercenary's (8200-8237) own skills - the game hands them to those
// (FUN_00562580), never the character's
bool companionSkill(int skill);

// 3.0.19-beta.18 (R-57KBSF «некоторые нпс утоплены в пол»): the game keeps a sprite a few units nearer than its feet
// by its rhw (uaRO.exe FUN_00546c50: n * 0x6D5EF4 + 0x6D5EF8 * scale - ~4.7 units at its own distance 300); from the
// eyes that is a hundredth of a unit and the ground in front of the feet covers them. In the 3D view a character's, a
// monster's, an NPC's sprite vertex: its depth (the game's z of rhw, FUN_00412E30) kSpriteNearer of its distance
// nearer, kSpriteNearerMost at most, never past the near plane; one nearer than that plane or behind the eye as it is
constexpr float kSpriteNearer = 0.04f, kSpriteNearerMost = 8.0f;
float spriteDepth(float z, float rhw);

// 3.0.19-beta.18 (R-57KBSF «вижу свою или голову пеко ... мигает»): the character's own picture left out - from the
// eyes always; behind, under kOwnNear from the head, and once out shown again only past kOwnShownAgain (the eye pulled
// in at a wall and let out by steps no longer turns it on and off); `behind` 0 - not known yet
constexpr float kOwnNear = 12.0f, kOwnShownAgain = 16.0f;
bool ownOut(Mode m, float behind, bool outBefore);

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
// 3.0.19-beta.21 (R-4JGBJS): the haze has the world whole (the horizon's colour) this far past the reach (sky3d_ps) -
// past it the own ground ends (own_ground::mostDepthOf), and the game's own far plane (1500) is not seen as an edge
constexpr float kHazeWholePast = 100.0f;
// 3.0.19-beta.24 (R-T9FPZZ «лучше но еще не то»: from the eyes the game drew all to 1500 ahead - the renderer's
// drawing 14.4 ms a frame, 8.9 of it the game's draws, the game's thread 11.3): [view3d] distance, the 3D view's sight
// - the game culls its world there (its frustum's far, uaRO.exe 0x69C5DC, 1500 its own), the haze is whole there
constexpr int kSightDefault = 1200, kSightLeast = 800, kSightMost = 1500;
// The haze's reach in the 3D view for a sight: whole kHazeWholePast past it at the sight, a nearer sky's kept
float hazeReach(float skyReach, float sight);
Sky skyOf(float night, bool vault);

}  // namespace uagfx::view3d
