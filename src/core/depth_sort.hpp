#pragma once
#include <cstdint>

namespace uagfx::depthsort {

// 3.0.11-beta.148 (R-DEHAHA: uaRO.exe crashed at 0x416C29 two seconds into prt_in): the game sorts its see-through
// faces by depth before drawing them (std::sort at 0x415E20 over {depth, face} pairs - four lists of its renderer, the
// calls at 0x40E992, 0x40E9FA, 0x40EA6F and 0x40EB35); its comparison (0x416C20: fcomp, then C0) finds a depth that is
// not a number less than every other one and every other one less than it, and the sort's unguarded scan ran past the
// list's end into memory not mapped. Such a depth is made the farthest (drawn first) before the game sorts.
struct Pair {
    float depth;
    uint32_t face;
};
static_assert(sizeof(Pair) == 8, "the game's pair");
// The pairs' depths that are not numbers made the farthest; how many
int mend(Pair* first, Pair* last);
// The game's comparison as it is: the depths (an unordered pair as less), then the faces
bool gameLess(const Pair& a, const Pair& b);

}  // namespace uagfx::depthsort
