#pragma once
#include <string>

namespace uagfx::settings {

// 1.4: uaRO Pad (the gamepad: uaro-graphics\pad\dinput.dll in the package) and the game folder's dinput.dll.
enum class PadFile {
    None,   // no dinput.dll: the gamepad is off
    Same,   // the package's uaRO Pad: on
    Pad,    // another version of uaRO Pad: on, to be updated
    Other,  // another tool's (ROExt, as the uaRO client ships): moved to dinput8.dll when the gamepad goes on
};

// What `installed` (the game folder's dinput.dll, "" when there is none) is against `packaged` (the package's).
inline PadFile classifyPad(const std::string& installed, const std::string& packaged) {
    if (installed.empty()) return PadFile::None;
    if (!packaged.empty() && installed == packaged) return PadFile::Same;
    return installed.find("uaro-pad.log") != std::string::npos ? PadFile::Pad : PadFile::Other;
}

}  // namespace uagfx::settings
