#pragma once

namespace uagfx::win {

// 3.0.23-beta.4 (R-WS4ETW, R-NE7JVF: malangdo's entry): CResMgr's lookups through a guarded walk (core/res_tree_exe) -
// a node another thread is still putting in is «not found», not a fault. false: this build has other bytes there
bool installResTreeFix();

}  // namespace uagfx::win
