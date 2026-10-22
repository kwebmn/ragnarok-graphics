#pragma once
#include <string>

namespace uagfx {

// 2.10: install.bat's additions to an older uaro-graphics.ini (0.3-0.39), now the installer's - each block of keys the
// file lacks goes at its end with the values that keep what that version showed, in install.bat's order (each check on
// the text as the ones before left it); the file's own lines stay, but 0.17's note and min_size = 64. For a 2.x root
// file before it moves: a 2.8 file in uaro-graphics\ is never upgraded.
std::string upgradeIni(const std::string& text);

}  // namespace uagfx
