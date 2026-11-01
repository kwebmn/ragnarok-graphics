#pragma once

namespace uagfx::win {

// 3.0.11-beta.148 (R-DEHAHA: uaRO.exe crashed at 0x416C29 two seconds into prt_in): the game's four sorts of its
// see-through faces by depth (std::sort 0x415E20) called through the driver first - a depth not a number made the
// farthest (core/depth_sort), which would have run the sort past its list's end; logged. False, with a log line, when
// the exe is another build.
bool installSortGuard();

}  // namespace uagfx::win
