#pragma once
#include <cstdio>
#include <string>

// 3.0.3-beta.1: the logs' slots for the pad's log too - no LOG_ macros here (pad/win/log.hpp has its own). In
// win/log.cpp.
namespace uagfx::wlog {

// A log of `path`'s slots (core/log_files slotted), the first one no running game writes (its writing shared with no
// one), from `firstSlot` on: emptied, the previous game's text first copied to .prev.log when `keepPrevious`; its
// file and slot into `opened` and `slot`. nullptr when none opens.
FILE* openFree(const std::string& path, bool keepPrevious, int firstSlot, std::string& opened, int& slot);
int slot();  // the driver's log's slot, 0 before it opens (or with none)

}  // namespace uagfx::wlog
