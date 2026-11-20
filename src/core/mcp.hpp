#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/json.hpp"

namespace uagfx::mcp {

// 3.0.22-beta.32 (the owner 20.11: «мсп сервер, через который можно было бы управлять клиентом и записывать видео»,
// «чтобы ты мог ходить и делать правильные сцены», «пускай будет мсп у всех»; docs/superpowers/specs/
// 2026-11-20-mcp-design.md): the client's MCP server - streamable HTTP's POST /mcp, one JSON-RPC 2.0 message a
// request and one JSON answer, on 127.0.0.1 alone ([mcp] enabled, port). This part has no Windows: the request read,
// the checks against web pages, the JSON-RPC, the tools' list and their arguments; win/mcp_server runs the game's side.
constexpr int kDefaultPort = 8770;
constexpr size_t kMostRequest = size_t(1) << 20;  // a request's head and body, a megabyte at most
constexpr const char* kLatestVersion = "2025-06-18";

struct HttpRequest {
    std::string method, path;
    std::vector<std::pair<std::string, std::string>> headers;  // the names in small letters
    std::string body;
    std::string header(const std::string& lowerName) const;  // "" none
};
// A request as far as it came: 0 more bytes needed; its whole length in `data` (read into `out`); below 0 a bad one
// (no head line, a length past kMostRequest)
int parseHttp(const std::string& data, HttpRequest& out);
// Refused (an answer 403 with `why`) - an Origin header (a web page in the player's browser), a Host other than
// 127.0.0.1 or localhost (a name of the web pointed at 127.0.0.1)
bool allowed(const HttpRequest& r, int port, std::string& why);
// An answer: the status line, Content-Type (none when ""), Content-Length, Connection: close, the body
std::string httpResponse(int status, const std::string& contentType, const std::string& body);

struct Tool {
    const char* name;
    const char* description;  // for the AI that calls it
    const char* inputSchema;  // JSON Schema, as text
};
const std::vector<Tool>& tools();
// The arguments' check before the game is asked: "" right, else what is wrong
std::string checkArgs(const std::string& tool, const json::Value& args);

// What a tool answers: a text or an image (its bytes, sent as base64)
struct Content {
    std::string type;  // "text" or "image"
    std::string text;
    std::string bytes;
    std::string mime;
    static Content textOf(std::string t);
    static Content imageOf(std::string bytes, std::string mime);
};
struct CallResult {
    std::vector<Content> content;
    bool isError = false;
};
// A tool's call, the arguments already checked (win/mcp_server: on the game's thread)
using CallFn = std::function<CallResult(const std::string& tool, const json::Value& args)>;

// One JSON-RPC message -> its answer's JSON; "" for a notification (HTTP 202 then). `version` - the client's own
std::string handle(const std::string& body, const CallFn& call, const std::string& version);

}  // namespace uagfx::mcp
