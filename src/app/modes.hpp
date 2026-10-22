#pragma once
#include <windows.h>

#include <string>

#include "app/args.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::app {

// 2.10: uaro-graphics.exe's modes but the settings app and the watcher (app/args): the update, the repair, the
// rollback, the removal and the install of an older update.bat, each with its own windows (app/update_window).
int runMode(const Args& a);

// 2.10: this exe is the setup - uaro-graphics.exe with the package appended (tools/package.sh).
bool carriesPackage();
// The game's folder: this exe's, or the one above it when the exe runs from uaro-graphics\ (install.bat's way).
std::wstring gameFolder();
settings::Lang playerLang(const std::wstring& dir);  // the app's choice in the settings file, else Windows'

// The settings app's way into a mode: this exe started with `a` and this process's id to wait for - as
// administrator when the game's folder needs it. "" or why not, in English.
std::string startMode(Args a);
// DLSS and the AI models from the site in a progress window over the settings app; "" or why not.
std::wstring getDlss(HWND owner, const std::wstring& dir, settings::Lang l);
std::wstring getModels(HWND owner, const std::wstring& dir, settings::Lang l);

}  // namespace uagfx::app
