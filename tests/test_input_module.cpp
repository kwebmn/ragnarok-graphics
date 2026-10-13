#include <cstring>

#include "core/settings/compat_flags.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/input_module.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

std::vector<uint8_t> dll(std::initializer_list<const char*> strings) {
    std::vector<uint8_t> d(64, 0x90);  // some code first
    for (const char* s : strings) {
        d.push_back(0);
        d.insert(d.end(), s, s + std::strlen(s));
        d.push_back(0);
    }
    return d;
}

}  // namespace

TEST(input_module_old_roext_names_its_ini) {
    // The ROExt of the original client: dinput8.dll, ".\dinput8.ini" in its strings.
    const ModuleInfo m = identifyModule("dinput8.dll", dll({"ROExt", ".\\dinput8.ini", "WindowLock"}));
    CHECK(m.kind == ModuleKind::Roext);
    CHECK(m.ini == "dinput8.ini");
    // A newer build loaded as dinput.dll names dinput.ini.
    CHECK(identifyModule("dinput.dll.bak", dll({"ROExt", ".\\dinput.ini"})).ini == "dinput.ini");
    // No ini string: the DLL's own name.
    CHECK(identifyModule("dinput.dll", dll({"ROExt"})).ini == "dinput.ini");
}

TEST(input_module_tells_uaro_pad_and_others) {
    const ModuleInfo pad = identifyModule("dinput.dll", dll({"uaro-pad.ini", "uaRO Pad %s", "ROExt"}));
    CHECK(pad.kind == ModuleKind::UaroPad);  // uaRO Pad mentions ROExt in its log lines: its own name wins
    CHECK(pad.ini == "uaro-pad.ini");
    CHECK(identifyModule("dinput.dll", dll({"DirectInputCreateA"})).kind == ModuleKind::Other);
    CHECK(loadedByGame("DINPUT.dll"));
    CHECK(loadedByGame("dinput8.dll"));
    CHECK(!loadedByGame("dinput.dll.bak"));
}

TEST(input_module_reads_roexts_window) {
    const RoextWindow a = roextWindow(readIni("[ROExt]\r\nWindowLock=1\r\nWindowWidth=1600\r\nWindowHeight=863\r\n"));
    CHECK(a.active);
    CHECK(a.lock);
    CHECK(a.size == (Size{1600, 863}));
    const RoextWindow b = roextWindow(readIni("[ROExt]\nWindowLock = 0\nWindowWidth = 1600\nWindowHeight = 0\n"));
    CHECK(!b.lock);
    CHECK(b.size == (Size{0, 0}));  // ignored when either side is 0
}

TEST(compat_flags_parse_and_remove) {
    CompatLayers l = parseLayers("~ RUNASADMIN HIGHDPIAWARE");
    CHECK(l.tilde);
    CHECK_EQ(l.flags.size(), size_t(2));
    CHECK(removeLayer(l, "runasadmin"));
    CHECK(formatLayers(l) == "~ HIGHDPIAWARE");
    CHECK(!removeLayer(l, "WINXPSP3"));
    CHECK(removeLayer(l, "HIGHDPIAWARE"));
    CHECK(formatLayers(l) == "");  // nothing left: the value goes
    CHECK(formatLayers(parseLayers("WINXPSP3")) == "WINXPSP3");
    CHECK(formatLayers(parseLayers("  ")) == "");
}

TEST(compat_flags_say_what_they_break) {
    CHECK(layerEffect("RUNASADMIN") == LayerEffect::RunAsAdmin);
    CHECK(layerEffect("winxpsp3") == LayerEffect::OldWindows);
    CHECK(layerEffect("WIN7RTM") == LayerEffect::OldWindows);
    CHECK(layerEffect("DPIUNAWARE") == LayerEffect::Dpi);
    CHECK(layerEffect("GDIDPISCALING") == LayerEffect::Dpi);
    CHECK(layerEffect("16BITCOLOR") == LayerEffect::Colors);
    CHECK(layerEffect("DISABLEDXMAXIMIZEDWINDOWEDMODE") == LayerEffect::Other);
    CHECK(samePath("C:\\Games\\uaRO3\\gp\\uaRO\\uaRO.exe", "c:/games/uaro3/gp/uaro/UARO.EXE"));
    CHECK(!samePath("C:\\Games\\uaRO.exe", "C:\\Games\\uaRO2.exe"));
}
