#pragma once
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"

namespace uagfx::app {

// Where the game's key lives (stage 2 spec §2.1). A 32-bit game on 64-bit Windows uses WOW6432Node; the app is
// 32-bit too, but it has a manifest, so Windows does not virtualise it and it opens both copies itself.
struct GameKeyPaths {
    std::wstring personal;    // under HKCU: Software\Classes\VirtualStore\MACHINE\SOFTWARE\[WOW6432Node\]Gravity Soft\Ragnarok
    std::string personalRoot; // the same as a .reg path: HKEY_CURRENT_USER\...
    std::string commonRoot;   // HKEY_LOCAL_MACHINE\SOFTWARE\[WOW6432Node\]Gravity Soft\Ragnarok
};
GameKeyPaths gameKeyPaths();

settings::RegCopy readPersonal();
settings::RegCopy readCommon();
// One change of the personal copy; "" or "NAME (error N)".
std::string writePersonalValue(const settings::RegWrite& w);
// The personal copy becomes `copy` (a restored backup): its whole tree goes, then every key and value is written, each
// on its own (spec §4.5); "" or the ones that failed, "NAME (error N), ...".
std::string replacePersonal(const settings::RegCopy& copy);

// AppCompatFlags\Layers values for the game's exe (stage 2 spec §7).
struct CompatEntry {
    bool allUsers = false;  // HKLM: changed only from the file's properties, as administrator
    std::string flags;      // "~ RUNASADMIN"
};
std::vector<CompatEntry> readCompat(const std::wstring& exePath);
// The user's value for the exe; "" deletes it. false when Windows refused.
bool writeUserCompat(const std::wstring& exePath, const std::string& flags);

// 2.10: uaRO Graphics in Windows' Apps (HKCU ...\CurrentVersion\Uninstall\uaRO Graphics, no administrator needed):
// its Uninstall runs "<dir>\uaro-graphics.exe" --uninstall. false when Windows refused.
bool writeAppsEntry(const std::wstring& dir, const std::string& version);
bool removeAppsEntry();

}  // namespace uagfx::app
