#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/settings/screen.hpp"

namespace uagfx::settings {

enum class ModuleKind { Roext, UaroPad, Other };

// What a dinput DLL in the client folder is (stage 2 spec §2.3), told by its bytes, not by its name.
struct ModuleInfo {
    ModuleKind kind = ModuleKind::Other;
    std::string ini;  // the file it reads: ROExt names it in the DLL (".\dinput8.ini"), else <its name>.ini
};
ModuleInfo identifyModule(const std::string& fileName, const std::vector<uint8_t>& dll);

// Whether the game loads this file: dinput.dll and dinput8.dll in its folder; a renamed copy (".bak") is off.
bool loadedByGame(const std::string& fileName);

// ROExt's window settings (spec §4.4) from its ini as readIni gives it.
RoextWindow roextWindow(const std::map<std::string, std::string>& ini);

}  // namespace uagfx::settings
