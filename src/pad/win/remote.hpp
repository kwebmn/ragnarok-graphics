#pragma once
#include <cstdint>

namespace uapad::remote {

// 3.0.22-beta.37 (the owner 20.11: «я хочу чтобы клиент мог работать в фоне, когда ты проверки делаешь, чтобы не
// ставить его на передний план»): the MCP's input given to the game from inside - the game's cursor driven to a point
// and its buttons through its DirectInput mouse (made up while the window is behind others and the device is not the
// game's), Ctrl, Shift and Alt as the game asks GetAsyncKeyState; the player's own mouse and keyboard untouched. Set
// from the MCP server's thread, read on the game's.

struct Mouse {
    bool on = false;
    int x = 0, y = 0;     // the game thread's client points (GetClientRect there)
    uint8_t buttons = 0;  // the device's: 1 left, 2 right
};
void setMouse(const Mouse& m);
Mouse mouse();

void setMods(bool ctrl, bool shift, bool alt);
bool modDown(int vk);  // VK_CONTROL, VK_SHIFT, VK_MENU and their left and right ones

// The game's mouse device polled through the pad's wrapper (its GetDeviceState) - the MCP's mouse can work
void notePolled();
bool polled();

}  // namespace uapad::remote
