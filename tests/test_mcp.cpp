// 3.0.22-beta.32 (the owner 20.11: «мсп сервер, через который можно было бы управлять клиентом и записывать видео»,
// «пускай будет мсп у всех»): the client's MCP server - its HTTP, its checks against web pages, its JSON-RPC, its tools
#include <string>

#include "core/json.hpp"
#include "core/mcp.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
json::Value parsed(const std::string& text) {
    json::Value v;
    std::string error;
    CHECK(json::parse(text, v, error));
    return v;
}
mcp::CallResult echoCall(const std::string& tool, const json::Value& args) {
    mcp::CallResult r;
    r.content.push_back(mcp::Content::textOf(tool + ":" + json::dump(args)));
    return r;
}
}  // namespace

TEST(mcp_http_request_whole_or_more_bytes) {
    const std::string body = R"({"jsonrpc":"2.0","id":1,"method":"ping"})";
    const std::string head = "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:8770\r\nContent-Type: application/json\r\n"
                             "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    mcp::HttpRequest r;
    CHECK_EQ(mcp::parseHttp(head.substr(0, 20), r), 0);           // the head not whole yet
    CHECK_EQ(mcp::parseHttp(head + body.substr(0, 5), r), 0);     // the body not whole yet
    CHECK_EQ(mcp::parseHttp(head + body, r), int(head.size() + body.size()));
    CHECK(r.method == "POST" && r.path == "/mcp" && r.body == body);
    CHECK(r.header("content-type") == "application/json" && r.header("host") == "127.0.0.1:8770");
    CHECK(mcp::parseHttp("GARBAGE\r\n\r\n", r) < 0);
    const std::string huge = "POST /mcp HTTP/1.1\r\nContent-Length: 99999999\r\n\r\n";
    CHECK(mcp::parseHttp(huge, r) < 0);  // over a megabyte: refused
}

TEST(mcp_web_pages_and_other_hosts_refused) {
    mcp::HttpRequest r;
    std::string why;
    r.headers = {{"host", "127.0.0.1:8770"}};
    CHECK(mcp::allowed(r, 8770, why));  // curl through the owner's tunnel, Claude Code
    r.headers = {{"host", "localhost:8770"}};
    CHECK(mcp::allowed(r, 8770, why));
    r.headers = {{"host", "127.0.0.1:8770"}, {"origin", "https://evil.example"}};
    CHECK(!mcp::allowed(r, 8770, why) && !why.empty());  // a web page in the player's browser
    r.headers = {{"host", "evil.example:8770"}};
    CHECK(!mcp::allowed(r, 8770, why));  // DNS rebinding: another name pointed at 127.0.0.1
    r.headers = {};
    CHECK(!mcp::allowed(r, 8770, why));  // no Host at all
}

TEST(mcp_initialize_agrees_a_version_and_says_its_tools) {
    const std::string answer = mcp::handle(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},)"
        R"("clientInfo":{"name":"curl","version":"1"}}})",
        echoCall, "3.0.22-beta.32");
    const json::Value v = parsed(answer);
    CHECK(v["jsonrpc"].asString() == "2.0" && v["id"].asInt() == 1);
    CHECK(v["result"]["protocolVersion"].asString() == "2025-03-26");  // one we know: the client's own
    CHECK(v["result"]["capabilities"].has("tools"));
    CHECK(v["result"]["serverInfo"]["name"].asString() == "uaro-graphics");
    CHECK(v["result"]["serverInfo"]["version"].asString() == "3.0.22-beta.32");
    const json::Value other = parsed(mcp::handle(
        R"({"jsonrpc":"2.0","id":"a","method":"initialize","params":{"protocolVersion":"1999-01-01"}})", echoCall,
        "x"));
    CHECK(other["id"].asString() == "a" && other["result"]["protocolVersion"].asString() == mcp::kLatestVersion);
}

TEST(mcp_tools_list_has_each_tool_with_its_schema) {
    const json::Value v = parsed(mcp::handle(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})", echoCall, "x"));
    const auto& list = v["result"]["tools"].items();
    CHECK_EQ(list.size(), mcp::tools().size());
    bool walk = false;
    for (const json::Value& t : list) {
        CHECK(!t["name"].asString().empty() && !t["description"].asString().empty());
        CHECK(t["inputSchema"]["type"].asString() == "object");
        if (t["name"].asString() == "walk") {
            walk = true;
            CHECK(t["inputSchema"]["properties"].has("x") && t["inputSchema"]["required"].items().size() == 2);
        }
    }
    CHECK(walk);
}

TEST(mcp_tools_call_runs_the_tool_and_checks_its_arguments) {
    const json::Value ok = parsed(mcp::handle(
        R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"walk","arguments":{"x":156,"y":204}}})",
        echoCall, "x"));
    CHECK(ok["result"]["isError"].asBool(true) == false);
    CHECK(ok["result"]["content"][0]["type"].asString() == "text");
    CHECK(ok["result"]["content"][0]["text"].asString() == R"(walk:{"x":156,"y":204})");
    // a wrong argument: the tool's own error, the game never asked
    const json::Value bad = parsed(mcp::handle(
        R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"walk","arguments":{"x":"far"}}})",
        echoCall, "x"));
    CHECK(bad["result"]["isError"].asBool() && !bad["result"]["content"][0]["text"].asString().empty());
    const json::Value pause = parsed(mcp::handle(
        R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"pause","arguments":{"on":"yes"}}})",
        echoCall, "x"));
    CHECK(pause["result"]["isError"].asBool());
    const json::Value view = parsed(mcp::handle(
        R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"view","arguments":{"mode":"sideways"}}})",
        echoCall, "x"));
    CHECK(view["result"]["isError"].asBool());  // only off, behind, first
    // no such tool: JSON-RPC's own error
    const json::Value none = parsed(mcp::handle(
        R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"fly_away","arguments":{}}})", echoCall,
        "x"));
    CHECK_EQ(none["error"]["code"].asInt(), -32602);
}

TEST(mcp_an_image_goes_as_base64_jpeg) {
    const auto shot = [](const std::string&, const json::Value&) {
        mcp::CallResult r;
        r.content.push_back(mcp::Content::imageOf(std::string("\xFF\xD8\xFF", 3), "image/jpeg"));
        return r;
    };
    const json::Value v = parsed(mcp::handle(
        R"({"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"screenshot"}})", shot, "x"));
    CHECK(v["result"]["content"][0]["type"].asString() == "image");
    CHECK(v["result"]["content"][0]["mimeType"].asString() == "image/jpeg");
    CHECK(v["result"]["content"][0]["data"].asString() == "/9j/");  // FF D8 FF in base64
}

TEST(mcp_notifications_get_no_answer_and_errors_are_json_rpc_s) {
    CHECK(mcp::handle(R"({"jsonrpc":"2.0","method":"notifications/initialized"})", echoCall, "x").empty());
    CHECK_EQ(parsed(mcp::handle("{not json", echoCall, "x"))["error"]["code"].asInt(), -32700);
    CHECK_EQ(parsed(mcp::handle(R"([{"jsonrpc":"2.0","id":1,"method":"ping"}])", echoCall, "x"))["error"]["code"]
                 .asInt(),
             -32600);  // no batches
    CHECK_EQ(parsed(mcp::handle(R"({"jsonrpc":"2.0","id":9,"method":"sing"})", echoCall, "x"))["error"]["code"]
                 .asInt(),
             -32601);
    const json::Value ping = parsed(mcp::handle(R"({"jsonrpc":"2.0","id":10,"method":"ping"})", echoCall, "x"));
    CHECK(ping["result"].type() == json::Value::Object && ping["id"].asInt() == 10);
}

TEST(mcp_http_answers) {
    const std::string a = mcp::httpResponse(200, "application/json", "{}");
    CHECK(a.find("HTTP/1.1 200 OK\r\n") == 0);
    CHECK(a.find("Content-Length: 2\r\n") != std::string::npos && a.find("\r\n\r\n{}") != std::string::npos);
    CHECK(mcp::httpResponse(202, "", "").find("HTTP/1.1 202 Accepted\r\n") == 0);
    CHECK(mcp::httpResponse(403, "text/plain", "no").find("HTTP/1.1 403 Forbidden\r\n") == 0);
}

// 3.0.22-beta.33 (the owner: «сделай» - the MCP's stage 2): the free camera moved by cells, the cinema camera's flight,
// the time of day running fast
TEST(mcp_stage_two_tools_and_their_arguments) {
    const auto has = [](const char* name) {
        for (const mcp::Tool& t : mcp::tools())
            if (std::string(t.name) == name) return true;
        return false;
    };
    CHECK(has("camera") && has("flight") && has("timelapse"));
    json::Value v;
    std::string e;
    const auto args = [&](const char* text) {
        json::parse(text, v, e);
        return v;
    };
    CHECK(mcp::checkArgs("camera", args(R"({"free":true})")).empty());
    CHECK(mcp::checkArgs("camera", args(R"({"move":{"forward":12.5,"up":-2}})")).empty());
    CHECK(!mcp::checkArgs("camera", args(R"({})")).empty());                        // nothing asked
    CHECK(!mcp::checkArgs("camera", args(R"({"move":{"forward":500}})")).empty());  // 200 cells at most
    CHECK(!mcp::checkArgs("camera", args(R"({"move":{}})")).empty());
    CHECK(!mcp::checkArgs("camera", args(R"({"free":"yes"})")).empty());
    CHECK(mcp::checkArgs("flight", args(R"({"action":"point"})")).empty());
    CHECK(mcp::checkArgs("flight", args(R"({"action":"play"})")).empty());
    CHECK(!mcp::checkArgs("flight", args(R"({"action":"loop"})")).empty());
    CHECK(mcp::checkArgs("timelapse", args(R"({"action":"hold"})")).empty());
    CHECK(!mcp::checkArgs("timelapse", args(R"({})")).empty());
}
