#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::roext {

// 3.0.1-beta.8 (our own libraries in place of the client's old ones, step 4): ROExt's work - the client's 8.7 KB
// dinput8.dll, which Windows' DirectInput loads from the game's folder - in the driver (win/roext.cpp): the game's
// window, its mouse buttons and keys, its CPU behind other windows, its text's code page. The installer moves
// dinput8.dll aside and brings dinput8.ini's settings into uaro-graphics.ini [roext].
struct Settings {  // the defaults: the uaRO client's own dinput8.ini
    bool windowLock = true;    // borderless over the screen's work area (all but the taskbar)
    bool windowOnTop = false;  // above the other windows
    int windowX = 0, windowY = 0, windowWidth = 0, windowHeight = 0;  // unlocked: this client size, both set
    bool mouseButtons = false;  // the 3rd-5th mouse buttons as F24, F23, F22 (bindable in the game)
    bool altF4 = false;         // Alt+F4 as F4 in the game, Alt+Print Screen closes it
    bool minimizeKey = false;   // Ctrl+W minimizes the game
    int freeCpu = 4;            // ms waiting for input while the window is behind and has nothing to do; 0 none
    int codePage = -1;          // the game's text's code page; -1 Windows' own, 65001 UTF-8
};

// dinput8.ini as ROExt reads it (GetPrivateProfileInt in [ROExt], its own defaults for a key not there: its mouse
// buttons on, AutoFreeCPU 250 ms), the width rounded up to 4 as ROExt does
Settings fromDinput8Ini(const std::string& text);
// uaro-graphics.ini's [roext] keys and values for them
std::vector<std::pair<std::string, std::string>> iniValues(const Settings& s);

// The game's main window as uaRO.exe makes it (windowed or full screen; ROExt's test)
bool isGameWindow(uint32_t exStyle, uint32_t style, bool parent, bool menu, bool param);

struct WindowPlan {
    uint32_t style = 0, exStyle = 0;
    bool place = false;  // false: the game's own place and size
    int x = 0, y = 0, width = 0, height = 0;
};
// Its window's style: borderless when locked, else captioned
uint32_t styleOf(const Settings& s);
// The game's window as ROExt makes it: both sizes set - that client size at its place (the frame of styleOf's
// `frameWidth` x `frameHeight` around it), locked or not; else locked - over `workArea` (left, top, right, bottom);
// else the game's own
WindowPlan windowPlan(const Settings& s, const int workArea[4], int frameWidth, int frameHeight);

// A message as the game takes it from its queue (PeekMessageA)
struct Message {
    uint32_t message;
    uint32_t wParam;
    int32_t lParam;
};
// As ROExt changes it: the mouse buttons, Alt+F4, Ctrl+W; false - dropped (Scroll Lock's keys, always)
bool transform(Message& m, const Settings& s, bool ctrlDown);

}  // namespace uagfx::roext
