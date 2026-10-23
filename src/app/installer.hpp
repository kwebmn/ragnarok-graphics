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
// 3.0.1-beta.5: our own libraries in place of the game's old ones (core/client_libs.hpp): the game's root holds ours
// (the package's uaro-graphics\clientlibs) where their switch is on ([client] cps, bink...), the game's own kept in
// uaro-graphics\original; off - or `off` (the removal) - puts the game's own back. Nothing moves while the game runs
// (the next sync does it): the install, an update, the app's start and its Apply sync.
void syncClientLibs(const std::wstring& dir, bool off, Result& r);
// 3.0.1: the names this folder's uaRO Graphics goes by - its client's (the build's uaro-client.json, else «uaRO»):
// the desktop shortcut «uaRO HD» and the Apps entry «uaRO Graphics (uaRO HD)»; the old client's «uaRO», «uaRO Graphics»
struct Names {
    std::wstring shortcut, apps;
};
Names namesOf(const std::wstring& dir);
// 3.0.1, at the app's every start in a game's folder: a client unpacked from its archive (never installed) gets its
// desktop shortcut once, as an install would; this folder's Apps entry is put right (written when missing or of
// another version, an entry of this folder under an old name gone). A name another copy holds is left to it.
void atStart(const std::wstring& dir);
// 2.9: a 2.x uaRO Pad dinput.dll goes (the pad is inside the driver) and what it moved aside comes back
// (uaro-pad.moved: ROExt as dinput8.dll, or dinput.dll.before-pad), as its own switch-off did.
void removeOldPad(const std::wstring& dir);
// install.bat, update.bat, uninstall.bat, get-dlss.bat, get-models.bat, use-dgvoodoo.bat, use-uaro.bat - each when
// its bytes name "uaRO Graphics" - and the folders older scripts left (uaro-graphics-update and the downloads').
void removeOldScripts(const std::wstring& dir);

}  // namespace uagfx::app::installer
