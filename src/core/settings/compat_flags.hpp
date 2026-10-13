#pragma once
#include <string>
#include <vector>

namespace uagfx::settings {

// The value Windows keeps for an exe under AppCompatFlags\Layers (stage 2 spec §7): "~ RUNASADMIN HIGHDPIAWARE".
struct CompatLayers {
    bool tilde = false;  // "~": set on the file's Compatibility tab
    std::vector<std::string> flags;
};
CompatLayers parseLayers(const std::string& value);
std::string formatLayers(const CompatLayers& l);  // "" when no flag is left: the value is deleted then
bool removeLayer(CompatLayers& l, const std::string& flag);  // case-insensitive; false when it was not there

enum class LayerEffect {
    RunAsAdmin,  // the game does not see the personal copy of its registry key
    OldWindows,  // a Windows XP/7/8 mode: Direct3D 12 and Windows scaling are not what the driver expects
    Dpi,         // Windows scaling overridden: the picture is no longer at the window's real pixels
    Colors,      // 256 colours, 16 bits, 640x480
    Other
};
LayerEffect layerEffect(const std::string& flag);

// Whether a value name under Layers is this exe's path: case-insensitive, '/' and '\' alike.
bool samePath(const std::string& a, const std::string& b);

}  // namespace uagfx::settings
