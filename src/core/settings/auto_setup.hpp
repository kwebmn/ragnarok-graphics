#pragma once
#include <map>
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
    bool vramCapped = false;  // 0.51.1: DXGI's 3072 MB for a 32-bit program and no better number: at least that
    std::string adapter;
    int screenWidth = 0, screenHeight = 0;  // the main monitor's pixels
    bool pack = false;                      // uaro-textures.zip next to uaRO.exe
    bool hdr = false;                       // 1.3.3: the main monitor shows HDR now (Windows' HDR on)
    bool integrated = false;  // 2.12: the main adapter shares the computer's memory (integrated graphics)
    bool battery = false;     // 2.12: a laptop's battery is there
};

enum class Tier {
    NoDriver,  // no graphics driver (Windows' basic display adapter) or no Direct3D 11: the game's own look only
    Old,       // Direct3D 11 without 12 (Radeon HD 5000/6000, GeForce 400-600 on old drivers)
    Low,       // Direct3D 12 with less than 4 GB (integrated graphics, small cards)
    Middle,    // Direct3D 12, 4 to 8 GB
    High,      // Direct3D 12, 8 GB or more
    // 2.12: integrated graphics with Direct3D 12 - weak (Intel HD/UHD, Radeon Vega 3-8): the super-light mode; strong
    // (Iris Xe, Arc in Core Ultra, Radeon 680M/780M/880M/890M): the low set with 2x MSAA
    IntegratedWeak,
    IntegratedStrong
};

struct AutoSetup {
    Tier tier = Tier::NoDriver;
    Preset preset = Preset::Plain;
    std::vector<IniEdit> edits;  // the preset's and the rest, every one of uaro-graphics.ini
    bool ai = false;             // the AI textures on (the pack is there and the card carries them)
    bool aiWorld = false;        // the ground's, the objects' and the effects' too (not only sprites and icons)
    // 1.3.3: the interface's size on the Screen page - a whole number of times the game's pixels (the crispest),
    // the game at about 1000 lines or more: 100 % up to 1440p, 200 % at 4K
    int ui = 100;
    std::string dlss;  // 1.3.3: NVIDIA RTX - "dlaa" up to 2560x1440, "quality" above; "" untouched
    bool soundEngine = false;  // Windows 10 and later: the sound placed around the player
};

AutoSetup autoSetup(const Machine& m);
// 2.12: integrated by its memory - a little of its own (1 GB or less) and more of the computer's; strong by its name.
bool integratedByMemory(unsigned long long dedicatedMb, unsigned long long sharedMb);
bool strongIntegrated(const std::string& adapter);

// 2.12: the app's start - a fresh install (the app never ran: no language, no statistics' answer) is set up for its
// card at once; a used one's card and screen are only recorded ([utility] machine); a changed one is asked about.
enum class FirstStart { Nothing, Apply, Ask, Record };
std::string machineKey(const Machine& m);  // "adapter|vramMb|WxH"
FirstStart firstStart(bool usedBefore, const std::string& kept, const std::string& now);
// "high", "middle", ...: for the log and the tests.
const char* tierName(Tier t);
// 3.0.9-beta.7 (the owner, R-3FS6YX «розмазує модель чара»): the world whole for everyone - a [quality] world_scale
// under 100 (the old cards' 75 % of the automatic setup since 2.5.16) back to 100 once, with [utility]
// world_scale_reset marking it done, so that a share the player picks after it stays. The edits for a file's keys
// (readIni); none once marked. The driver at the game's start and the settings app when it opens.
std::vector<IniEdit> worldScaleReset(const std::map<std::string, std::string>& ini);

}  // namespace uagfx::settings
