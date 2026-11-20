#pragma once
#include <functional>
#include <string>

#include "core/json.hpp"
#include "core/mcp.hpp"

namespace uagfx::win::mcpwin {

// 3.0.22-beta.32 (the owner 20.11: «мсп сервер, через который можно было бы управлять клиентом», «пускай будет мсп у
// всех»; docs/superpowers/specs/2026-11-20-mcp-design.md): the client's MCP server - a thread of its own takes the
// requests on 127.0.0.1 ([mcp] enabled, port; core/mcp the protocol and its checks), the game's thread runs each tool
// at its frame and the answer goes back (10 s at most). Winsock taken from the game's own ws2_32 (the driver imports
// none).

// A tool run on the game's thread (Context::mcpCall); `screenshot` is the server's own (shotWanted)
using RunFn = std::function<mcp::CallResult(const std::string& tool, const json::Value& args)>;

// Each frame on the game's thread: the server started, moved to another port or stopped as the setting says; a call
// waiting run through `run`; a screenshot asked - its file looked for
void frame(bool enabled, int port, const std::string& gameDir, const RunFn& run);
// The screenshot target for this present ("" none): the picture `pictureWidth` wide, made as small as the call asked
std::string shotWanted(int pictureWidth);
// The game's end: the server stopped
void stop();

}  // namespace uagfx::win::mcpwin
