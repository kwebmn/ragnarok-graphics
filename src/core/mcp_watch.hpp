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

// 3.0.24-beta.37 (the laptop's run: a beta out, the laptop could not take it without a press on «ОНОВИТИ»): the game
// tool's update - as «ОНОВИТИ» (the app updated in a window of its own, opened again after it), never over a game that
// runs; a check asked first when there is none to go by
enum class Check { Idle, Checking, Done, Failed };  // the app's update check (app/update.hpp's UpdateState::Kind)
enum class UpdateStep { Refuse, Check, Wait, Newest, Update };
struct UpdateWhat {
    UpdateStep step;
    std::string words;  // the answer
};
// 3.0.24-beta.39: `fresh` - the check's result is of the last minute (the app checks at its start alone: «the newest»
// an hour on was not); a done check that is not - a new one first
UpdateWhat updateStep(bool gameRuns, Check check, const std::string& installed, const std::string& latest,
                      const std::string& offer, const std::string& error = "", bool fresh = true);
constexpr const char* kNvidiaQuery =
    "--query-gpu=name,utilization.gpu,temperature.gpu,clocks.gr,clocks.mem,power.draw,memory.used,memory.total,"
    "pstate,clocks_throttle_reasons.active --format=csv,noheader,nounits";

}  // namespace uagfx::mcpwatch
