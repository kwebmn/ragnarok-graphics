#pragma once
#include <cstdint>
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::app {

// 2.6: what the Main page and the status bar tell of the client folder.

struct PackSummary {
    bool exists = false;
    bool counted = false;  // its directory read (not while its builder writes it)
    uint64_t pictures = 0;
    uint64_t bytes = 0;
};
// uaro-textures.zip next to uaRO.exe: counted by its directory once a change of its size or time.
PackSummary packSummary(const std::wstring& dir);
// DLSS's files (get-dlss.bat puts uaro-graphics\dlss\ with its version.txt).
bool dlssInstalled(const std::wstring& dir);
// "What's new" in this version: its part of TEST.<language>.md next to uaRO.exe (the English one when that is
// missing), or a line saying there are none.
std::wstring notesText(const std::wstring& dir, settings::Lang l);

}  // namespace uagfx::app
