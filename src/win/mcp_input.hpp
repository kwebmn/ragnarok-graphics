#pragma once
#include <windows.h>

#include <string>

#include "core/mcp.hpp"

namespace uagfx::win::mcpinput {

// 3.0.22-beta.35 (the owner 20.11: «проверяй» - the login's cards, the game's windows): the MCP's keys and clicks.
// Real input (SendInput - the game reads its keys and buttons through DirectInput too), so only with the game's window
// in front: else it would go to the window the player works in. Run on the MCP server's thread - the game's frames go
// on meanwhile and see the key held, the cursor moved, the button pressed.

struct Game {
    HWND window = nullptr;
    int width = 0, height = 0;  // the game's own picture - its cursor's pixels
};

// "" - the game's window shown and in front; else why the input is refused
std::string ready(HWND window);
// The key with its modifiers pressed and let go, held for a few frames
mcp::CallResult key(const Game& g, const mcp::KeyCombo& k, const std::string& words);
// The cursor at (x, y) of a screenshot `width` wide, the button pressed and let go (twice for a double click); the
// cursor stays there. The answer tells where the game's own cursor went.
mcp::CallResult click(const Game& g, double x, double y, int width, bool right, bool twice);

}  // namespace uagfx::win::mcpinput
