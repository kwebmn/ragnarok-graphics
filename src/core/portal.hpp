#pragma once
#include <array>
#include <vector>

#include "core/game_camera.hpp"

namespace uagfx::portal {

// 2.7.48, the new portal (the user's pick «Б» of the options page: «такой же волнами и объёмный, но лучше»): the
// game's warp zone (uaRO.exe FUN_005ce290, effect 61) drawn anew in the shaders (portal_ps). The game's own, at 60
// ticks a second: a ring of light every 28 ticks, each 80 ticks long, a wall 5 units tall standing on a circle of 14
// that closes in by 0.15 a tick, its alpha 200; a column of 15 round them; sparks every 10 ticks rising for 130.
constexpr float kTick = 1.0f / 60.0f;
constexpr float kRingEvery = 28.0f * kTick;
constexpr float kRingLife = 80.0f * kTick;
constexpr float kRingRadius = 14.0f;
constexpr float kRingShrink = 0.15f;  // a tick
constexpr float kRingHeight = 5.0f;
constexpr float kRingAlpha = 200.0f / 255.0f;
constexpr float kColumnRadius = 15.0f;
constexpr int kRings = 3;  // alive at once: 80 / 28

struct Ring {
    float radius = 0.0f;  // world units
    float alpha = 0.0f;   // 0-kRingAlpha
};

// The rings at time t (seconds), the newest first: each closes in from kRingRadius, comes in over its first eighth
// and goes over its last fifth (FUN_005ce290's fade from 0x198 - 0x198 / 5); one past its life is gone (alpha 0).
std::array<Ring, kRings> ringsAt(double t);

// 2.7.49 (the tester on 2.7.48: «порталы не нарисовались» - the game's left out, the new one never drawn): whether a
// draw of the game's portal is left out - only while the new one is wanted and was drawn the frame before, so that
// there is never no portal at all.
inline bool hidesGame(bool wanted, bool drawnLastFrame) { return wanted && drawnLastFrame; }

// A draw of a portal this frame (the renderers' notePortal): its middle in the view and on the screen (game
// pixels), its size there; 2.7.51: its span across and its bottom on the screen.
struct Seen {
    float view[3];
    float gx, gy, size;
    float left, right, bottom;
};

// A portal followed from frame to frame (0.39.1: its glow held steady while its rings rise and come and go).
struct Track {
    float view[3];
    float gx, gy, size;
    float strength;  // 0-1: up over 0.4 s while seen, down over 1.2 s while not
    float left, right, bottom;  // 2.7.51: its draws' span on the screen (game pixels)
};

// This frame's draws gathered into what stands there (within 80 game pixels, or 0.8 of the larger size: one portal,
// its middle theirs), then the tracks followed by `step` seconds: each takes the nearest gathering within 140 pixels -
// its place eased towards it, its strength up - or fades; a gathering no track took starts one (16 at most) and faded
// ones go. 2.7.50 (the tester on 2.7.49: still the game's portal): 0.39.1 started a track at 0 and dropped those at 0
// in the same frame - no portal was ever followed, nor its 0.38 glow lit; one starts a step's rise strong now.
// 2.7.51: a draw over 600 pixels is no part of a portal (its corners behind the camera) and is left out.
void follow(std::vector<Track>& tracks, const std::vector<Seen>& draws, float step);

// 2.7.51 (the tester on 2.7.50: «два портала, один портал двигается»): where a followed portal stands on the screen
// (game pixels) - the middle of its draws' span, up from their bottom by half the span squashed as a circle lying on
// the ground is (squash: |the world's up's view z|). Its draws (a fan of triangles round its middle) give it better
// than their middles' mean, a draw or two of its sparks off to a side.
void anchorOf(const Track& t, float squash, float out[2]);

// 2.7.52 (the tester on 2.7.51: one on a tower's roof - the ground 2.7.51 looked for in the depth under where it stands
// was the tower in front of it): its foot in the view - where it stands on the screen at its draws' own depth (the
// game's vertices lie on the ground, the fan 465-474 deep round its middle; the 299 deep one 2.7.50 told was another
// portal, off the screen).
void footOf(const Track& t, float squash, const camera::Projection& p, float out[3]);

// The screen box (game pixels: x0, y0, x1, y1) round a portal standing at foot (the view) with the world's up (the
// view's): its column, a fifth wider than kColumnRadius, `height` tall; false when any of it is behind the camera.
bool screenBox(const float foot[3], const float up[3], const camera::Projection& p, float height, float box[4]);

}  // namespace uagfx::portal
