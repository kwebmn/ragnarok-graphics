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
// Every file under `staged` (its tree as the game's folder's: uaro-graphics\dlss\...) into `dir`, replacing; the
// replaced ones wait in uaro-graphics\update\replaced for the undo and go. All or nothing (DLSS, the AI models).
Result putFiles(const std::wstring& dir, const std::wstring& staged);
bool canRollBack(const std::wstring& dir);              // uaro-graphics\previous\ holds files
std::string previousVersion(const std::wstring& dir);   // its version.txt's first word, "" none
// uaro-graphics\previous\ back in place of the files it replaced (all or nothing); previous\ goes.
Result rollBack(const std::wstring& dir);

// What install.bat did after the files: a 2.x root settings file upgraded (core/ini_upgrade) and the 2.x layout moved
// (core/layout_migration); the folders of layout::kMadeAtStart; dgVoodoo's D3DImm.dll and a copy of its dgVoodoo.conf
// into uaro-graphics\dgvoodoo\; the settings file from the package's template when there is none; an old uaRO Pad dll
// and its folder away; our old .bat files away when `scripts` (not while an older update.bat runs this); the Apps
// entry.
Result finishInstall(const std::wstring& dir, bool scripts);
// uaRO Graphics out of the game's folder: our ddraw.dll; dgVoodoo's files back from uaro-graphics\dgvoodoo\ and the
// 2.x dgvoodoo\ (their dgVoodoo.conf copy deleted); the root uaro-graphics.exe (the running one cannot go: `log` says
// "installer: self" and the caller deletes it later); uaro-graphics\ whole - or, `keep`, all but the settings, the
// pad's, the copies of them (backup\) and the texture pack (textures\); our old scripts; the Apps entry. With nothing
// kept the folder is as it was before the install.
Result uninstall(const std::wstring& dir, bool keep);
// 2.9: a 2.x uaRO Pad dinput.dll goes (the pad is inside the driver) and what it moved aside comes back
// (uaro-pad.moved: ROExt as dinput8.dll, or dinput.dll.before-pad), as its own switch-off did.
void removeOldPad(const std::wstring& dir);
// install.bat, update.bat, uninstall.bat, get-dlss.bat, get-models.bat, use-dgvoodoo.bat, use-uaro.bat - each when
// its bytes name "uaRO Graphics" - and the folders older scripts left (uaro-graphics-update and the downloads').
void removeOldScripts(const std::wstring& dir);

}  // namespace uagfx::app::installer
