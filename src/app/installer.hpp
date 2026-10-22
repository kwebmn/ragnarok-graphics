#pragma once
#include <string>
#include <vector>

namespace uagfx::app::installer {

// 2.10: uaRO Graphics puts itself into the game's folder - install, update, repair, rollback, removal - with no .bat.
// Each step that changes a file is a move (a rename: a running exe can be renamed), and a failed step undoes every
// step before it: the folder is as it was, or as it should be.
struct Result {
    bool ok = true;
    std::wstring error;             // why not, for the player (the file it could not change)
    std::vector<std::string> log;   // what was done, for the update's log
};

// The package's tree in `staged` (uaro-graphics\... as in the zip; its root files are not put) into the game's
// folder `dir`, the root getting ddraw.dll and uaro-graphics.exe from its uaro-graphics\. A file equal to the
// installed one stays; a differing one replaces it, the installed one going into uaro-graphics\previous\ (the
// rollback point, made anew only when something went there) - with `repair` it just goes (previous\ stays the version
// before). A root ddraw.dll that is not ours (dgVoodoo's) goes into uaro-graphics\dgvoodoo\DDraw.dll instead.
Result putPackage(const std::wstring& dir, const std::wstring& staged, bool repair);
bool canRollBack(const std::wstring& dir);              // uaro-graphics\previous\ holds files
std::string previousVersion(const std::wstring& dir);   // its version.txt's first word, "" none
// uaro-graphics\previous\ back in place of the files it replaced (all or nothing); previous\ goes.
Result rollBack(const std::wstring& dir);

}  // namespace uagfx::app::installer
