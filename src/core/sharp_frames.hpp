#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/image.hpp"

namespace uagfx::sharpframes {

// 3.0.18-beta.178 (the owner: «делай 1, только чтобы было полное соответсвие с игрой», then «как в игре, только
// чётко»): uaRO.exe's damage digits (이팩트\숫자.spr), Miss, Guard, Critical!!, the crits' stars and Lucky (msg.spr,
// bluemsg.spr) - frames of 6 x 11 to 112 x 18 pixels that the game draws from its sprite cache, magnified with the
// game: blurred. Each is drawn from a copy of its own made four times larger by xBR (core/xbr.hpp, as the windows'
// labels): the same picture, sharp. The game still places, colours, scales and moves every quad.

constexpr int kScale = 4;
constexpr int kMostSide = 256;  // a frame larger than this is none of theirs
// 3.0.18-beta.185: a frame this small at most (the digits, 6 x 11 to 10 x 13 - a 4K screen shows them 12 times their
// size) xBR'd twice: 16 times; the larger ones (Miss, Critical!!, the star) 4 times
constexpr int kSmallSide = 32;

// The copy: kScale times the frame's size (a small frame's kScale squared), then its halves down to the frame's own
// size - the card takes the level that fits how large the game shows it. Empty levels: no copy (no size, too large).
struct Copy {
    int width = 0, height = 0;  // of level 0
    std::vector<std::vector<uint32_t>> levels;
};
Copy makeCopy(const uint32_t* argb, int w, int h);

// The picture's key: its texels and its size (FNV-1a), never 0 - one copy a picture, whichever cell holds it.
uint64_t keyOf(const uint32_t* argb, int w, int h);

// A draw of the frame at `frame` (texels of a texW x texH sprite cache): each vertex's stage 0 coordinates (u, v at
// byte 24 of a TL vertex) moved from the cache into the copy's 0..1. False, the vertices untouched, when one of them
// lies past the frame by more than half a texel - a draw of something else.
bool remap(uint8_t* vertices, uint32_t count, uint32_t stride, int texW, int texH, const Rect& frame);

}  // namespace uagfx::sharpframes
