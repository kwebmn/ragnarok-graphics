#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/sprite_cells.hpp"
#include "core/states.hpp"

namespace uagfx::win::sharpframes {

// 3.0.18-beta.178 (core/sharp_frames.hpp; [display] crisp_numbers): the damage digits, Miss, Guard, Critical!!, the
// crits' stars and Lucky drawn from sharp copies of the game's own frames. One copy a picture, kept for the game's
// run: noted as the game writes the frame into its sprite cache, made and sent to the card at its first draw.

// Surface7::Unlock: a number's frame the game just wrote into a cache cell - its texels as the card gets them (ARGB,
// w x h, the colour key's texels transparent). Its copy's key; 0: none (no size, too many pictures).
uint64_t note(const uint32_t* argb, int w, int h);

// Device7::record: a draw from a sprite cache whose cell at the quad holds a number's frame (cell.sharp) - drawn from
// the copy: its texture in cmd (sharpCopy), the draw's coordinates moved into it (the vertices copied into scratch),
// filtered smoothly through its levels. False - as the game draws it: crisp_numbers off, Pause, the quad not the
// frame's, no texture.
bool swap(const sprite_cells::Cell& cell, int texW, int texH, DrawCmd& cmd, DrawSetup& setup, const void*& vertices,
          uint32_t count, uint32_t fvf, std::vector<uint8_t>& scratch);

// The 30-second line's part ("; sharp numbers: ..."); "" until a number's frame was seen.
std::string stats();

}  // namespace uagfx::win::sharpframes
