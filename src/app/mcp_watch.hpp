#pragma once
#include <windows.h>

#include <string>

#include "core/settings/strings.hpp"

// 3.0.24-beta.35 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 8; the owner 27.11: «давай все через
// mcp»): the settings app's own MCP server - the watchdog beside the game (core/mcp_watch its tools), on 127.0.0.1 at
// the game's [mcp] port + 1 while the app runs and [mcp] enabled is on. Its calls answered on its own thread; the
// game's start (it needs the app's model) on the window's (kCall).
namespace uagfx::app {

class Model;

namespace mcpwatch {

constexpr UINT kCall = WM_APP + 0x4D3;  // a call for the window's thread: its job in lParam (run() takes it)
// `iniPath` the app's (Paths::ini): its folder the files' (uaro-graphics\)
void start(HWND window, int port, const std::wstring& clientDir, const std::wstring& iniPath);
void stop();
void run(LPARAM job, Model& model, settings::Lang lang);

}  // namespace mcpwatch
}  // namespace uagfx::app
