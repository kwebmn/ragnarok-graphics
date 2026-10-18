#pragma once
#include <string>

namespace uagfx::studio {

// 2.3, uaRO Studio (G1, G2): where it lives. It ships in the game's uaro-graphics\ folder; started there it copies
// itself to %LOCALAPPDATA%\uaroGraphics\studio\<its build>\ and runs that copy, so that the driver's update can
// replace the shipped one while it runs. The last five builds' copies stay.
std::wstring exePath();
std::wstring studioRoot();   // %LOCALAPPDATA%\uaroGraphics\studio
std::wstring buildName();    // this exe's: "<version>-<8 hex of its size and time>"
std::wstring buildFolder();  // studioRoot()\buildName()
std::wstring linkFolder();   // studioRoot()\link: pick.txt, changed.txt (core/studio_link.hpp)
bool makeFolder(const std::wstring& path);  // and the folders above it
bool runningFromCopy();      // this exe is its build folder's copy
// The game's folder of an exe in its uaro-graphics folder ("" otherwise).
std::wstring gameDirOf(const std::wstring& exe);
// The copy made (when it is not there) and started with --game; true: it runs, this one ends.
bool startCopy(const std::wstring& gameDir, std::wstring& error);
void dropOldBuilds();        // the build folders past the last five (never this one)

}  // namespace uagfx::studio
