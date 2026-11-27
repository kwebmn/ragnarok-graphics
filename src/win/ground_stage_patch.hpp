#pragma once

namespace uagfx::win {

// 3.0.24-beta.31 (core/ground_stage_exe; R-F4GZ3T «побіліла земля»): the game's ground's light map stage set whole
// before its draws - its arguments no longer another draw's; once, after this build's bytes are checked
bool installGroundStageFix();

}  // namespace uagfx::win
