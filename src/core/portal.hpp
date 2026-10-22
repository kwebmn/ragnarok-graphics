#pragma once
#include <cstdint>
#include <vector>

#include "core/game_camera.hpp"

namespace uagfx::portal {

// 2.7.54, the map warps (effect 321, uaRO.exe FUN_00607500; the driver's record of their draws, R-FP2F4H): the game's
// own draws shaded anew where it draws them (portal_draw_ps) - no portal of the driver's own to place (2.7.48-2.7.52
// drew one over its place, guessed from the draws: twice, or off its place). Each frame, at 60 ticks a second: a floor
// of 20 triangles round its middle, 15 units out (effect\alpha_down.tga, white, alpha 0.5, blended over); two prims of
// four rings (effect\ring_blue.tga, added, 20 faces of two triangles each): walls leaning out from a circle on the
// ground, coming in by 0.05 a tick from 10, standing up as they come (9 x (10 - r) degrees, r wide), their alpha up by
// one a tick to 70 (FUN_005e4ee0); a spark every 10 ticks rising for 70 (particle1.spr, a quad facing the screen,
// blended over).
constexpr int kWarpEffect = 321;

enum class Part : uint8_t { None, Ring, Floor, Spark };

// Which part of a warp a draw is, by its effect, blending (D3D7 factors) and vertices; None for anything else - another
// effect taken for a portal, a state the record has not shown: as the game draws it.
Part partOf(int effectId, bool blend, uint8_t srcBlend, uint8_t dstBlend, uint32_t vertexCount);

// 2.7.55 (the tester on 2.7.54's lilac walls: «аккуратнее ... более похожим на тот что в оригинале»): whether a part is
// shaded anew - the rings and the floor (2.7.56: the game's pictures drawn anew at full resolution); the sparks stay
// the game's own.
inline bool reshaded(Part p) { return p == Part::Ring || p == Part::Floor; }


// A corner of a draw: where it lies on the screen (game pixels), its rhw and its texture's v.
struct Corner {
    float x, y, rhw, v;
};

// The middle of a warp's floor from one of its triangles: the corner whose v is 1 (its texture's bottom - every
// triangle has it); false when none has.
bool floorMiddle(const Corner c[3], Corner& out);

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

}  // namespace uagfx::portal
