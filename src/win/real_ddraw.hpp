#pragma once
#include <windows.h>

#include "core/computer.hpp"
#include "core/config.hpp"
#include "core/renderer_choice.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::win {

// What the first call of any export decides, once: the settings, the log and the renderer.
struct Startup {
    Config config;
    RendererChoice choice;
    int cardLevel = 1;  // 2.2: the card's for the automatic effects set (effect_groups::cardLevel)
    // 3.0.4-beta.35: its memory's alone (a weak one's grass thinned), and whether its speed was judged already
    // (core/card_speed.hpp: the judged level is cardLevel - the game's own judge goes on from it)
    int memoryLevel = 1;
    bool cardDecided = false;
    // 2.2.1: the card's name, maker and video memory (the statistics, core/usage.hpp)
    std::string adapter;
    unsigned vendor = 0;
    unsigned long long vramMb = 0;
    Computer computer;  // 2.5.15: the processor, the memory, the power (the log, the statistics)
    settings::Lang lang = settings::Lang::En;  // 2.15: the player's (the settings app's choice, else Windows')
    bool graphicsTest = false;  // 3.0.5-beta.7: the settings app's «Тест графіки» started it (UARO_GRAPHICS_TEST)
};
const Startup& startup();

// The system's DirectDraw: where the calls go when the uaRO renderer is not used, and its fallback (2.2: dgVoodoo is
// gone). Loaded on the first call that needs it, never in DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing
// The same, but never starts loading: for the private exports that d3dim700.dll calls
// while the real DirectDraw is being set up. nullptr until something has loaded it.
FARPROC loadedProc(const char* name);
// Called from DllMain on DLL_PROCESS_DETACH: which DirectDraw modules the game ended up with.
void logModulesAtExit();
// 2.13: Ctrl+Shift+F10 - uaro-graphics.exe --report "<shot>" --map <map>: «Нове звернення» over the game, with the
// screenshot the press took (written by the renderer a frame later: the app waits for it)
void startReport(const std::string& shot, const std::string& map);
// 3.0.5-beta.6: «Тест графіки»'s result (its table in `file`) sent by the settings app without a window
// (uaro-graphics.exe --test-report, app/test_report); false when it could not start
bool startTestReport(const std::string& file);

}  // namespace uagfx::win
