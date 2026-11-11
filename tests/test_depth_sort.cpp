// 3.0.11-beta.148 (R-DEHAHA: uaRO.exe crashed at 0x416C29 two seconds into prt_in): the game's sort of its see-through
// faces by depth (core/depth_sort) - a depth not a number broke its order and its scan ran past the list's end
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "core/depth_sort.hpp"
#include "test.hpp"

using namespace uagfx;

