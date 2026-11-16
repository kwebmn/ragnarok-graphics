#pragma once
#include <cstddef>

#include "core/game_models.hpp"

namespace uagfx::model_gaps {

// 3.0.20-beta.56 (R-2BX4BG, the owner in pay_dun00's 3D view: «и там скала прозрачная какая-то»): the cave's «mushroom
// rock» (외부소품\버섯바위.rsm) is, in its own file, a cap hung 3.9 over an open-topped base with nothing between them
// and no underside - a model made for the view from above, where the cap hides the gap; from the eyes' level the wall
// behind shows through it. Of some 2800 of the client's model files 74 have such a part under another
// (/root/uagfx-tools/audit/models/hatcensus.cpp: a pagoda's tiers, a guard post's walls under its roof).
// A still node's gaps closed (its vertices the world's, up along -y):
//  - its faces fall into parts by the corners they share (corners of one place one corner);
//  - a part's open top - a ring of edges one face has, at its top (within kRingBand of it), kLeastRing of them at
//    least - with another part straight over every corner of the ring, that part's bottom kLeastGap to kMostGap over
//    the ring, the ring kWide of that gap across both ways at least (a table's leg under its top, a lantern's post:
//    narrow - left as drawn);
//  - the ring raised: a wall of two faces an edge up to the part over it - to that part's open bottom's lid where it
//    has an open bottom (a ring of edges at its bottom the lower ring lies within; the lid: a fan of faces to the
//    ring's middle), else to its faces over each corner;
//  - a new corner wears what the corner under it wears (its picture's place, its colours, its light); the new faces
//    are their parts' own textures, drawn from both sides.
// The faces added (0: nothing to close - a moving node's never: its own space, the game moves it)
constexpr float kLeastGap = 0.4f, kMostGap = 12.0f, kMostRise = 40.0f, kRingBand = 0.6f, kWide = 1.5f;
constexpr int kLeastRing = 3;
size_t close(gamemodels::Node& n);

}  // namespace uagfx::model_gaps
