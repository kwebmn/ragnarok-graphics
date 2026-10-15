#pragma once
#include <string>
#include <vector>

#include "core/settings/ini_edit.hpp"
#include "core/settings/presets.hpp"

namespace uagfx::settings {

// 0.50: the settings app's "Set up for this computer" - a client handed to other players (old Radeons, laptops) gets
// what its graphics card can carry: one press, then the game.
struct Machine {
    bool windows10 = false;  // Direct3D 12 needs it
    bool d3d12 = false;      // a Direct3D 12 device was made on the main adapter
    bool d3d11 = false;
    unsigned d3d11Level = 0;  // D3D_FEATURE_LEVEL (0xb000 is 11_0)
    unsigned vendor = 0;      // PCI: 0x10de NVIDIA, 0x1002 AMD, 0x8086 Intel, 0x1414 Microsoft's software adapter
    unsigned long long vramMb = 0;  // dedicated video memory
    std::string adapter;
    int screenWidth = 0, screenHeight = 0;  // the main monitor's pixels
    bool pack = false;                      // uaro-textures.zip next to uaRO.exe
};

enum class Tier {
    NoDriver,  // no graphics driver (Windows' basic display adapter) or no Direct3D 11: the game's own look only
    Old,       // Direct3D 11 without 12 (Radeon HD 5000/6000, GeForce 400-600 on old drivers)
    Low,       // Direct3D 12 with less than 4 GB (integrated graphics, small cards)
    Middle,    // Direct3D 12, 4 to 8 GB
    High       // Direct3D 12, 8 GB or more
};

struct AutoSetup {
    Tier tier = Tier::NoDriver;
    Preset preset = Preset::Plain;
    std::vector<IniEdit> edits;  // the preset's and the rest, every one of uaro-graphics.ini
    bool ai = false;             // the AI textures on (the pack is there and the card carries them)
    bool aiWorld = false;        // the ground's, the objects' and the effects' too (not only sprites and icons)
};

AutoSetup autoSetup(const Machine& m);
// "high", "middle", ...: for the log and the tests.
const char* tierName(Tier t);

}  // namespace uagfx::settings
