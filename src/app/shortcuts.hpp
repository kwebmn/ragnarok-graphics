#pragma once
#include <string>
#include <vector>

namespace uagfx::app {

// 2.11: desktop shortcuts (COM IShellLinkW): the installer's «uaRO», the launcher, in place of the game's own
// (decision 16). The tests' stand-ins keep a shortcut as a file "lnk:<target>".
struct Shortcut {
    std::wstring path, target;
};
std::wstring desktopFolder();              // the user's desktop, "" when Windows does not say
std::vector<Shortcut> desktopShortcuts();  // its .lnk files and where each one points
bool makeShortcut(const std::wstring& path, const std::wstring& target, const std::wstring& folder,
                  const std::wstring& icon);

}  // namespace uagfx::app
