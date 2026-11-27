#pragma once
#include <string>

#include "core/json.hpp"
#include "core/mcp.hpp"

// 3.0.24-beta.35 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 7; the owner 27.11: «давай все через
// mcp»): the settings app's own MCP - the watchdog beside the game, on the game's port + 1 (127.0.0.1, the same
// tunnel): the game started, closed, killed; the computer's load; uaro-graphics\'s logs, dumps, ini
namespace uagfx::mcpwatch {

const mcp::Catalog& catalog();
// A name of uaro-graphics\'s own files: the folder itself or its logs\, a log, text, ini, json, csv or dump - no way
// out of it (no drive, no «..», no other folder)
bool fileAllowed(const std::string& name);
// nvidia-smi's line (--query-gpu=name,utilization.gpu,temperature.gpu,clocks.gr,clocks.mem,power.draw,memory.used,
// memory.total,pstate,clocks_throttle_reasons.active --format=csv,noheader,nounits): its numbers; null for none
json::Value nvidiaSmi(const std::string& line);
constexpr const char* kNvidiaQuery =
    "--query-gpu=name,utilization.gpu,temperature.gpu,clocks.gr,clocks.mem,power.draw,memory.used,memory.total,"
    "pstate,clocks_throttle_reasons.active --format=csv,noheader,nounits";

}  // namespace uagfx::mcpwatch
