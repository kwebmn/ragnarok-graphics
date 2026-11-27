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

// 3.0.22-beta.35 (the owner: «проверяй»; the login's cards, the game's windows): a key with its modifiers, a click at
// a place of the screenshot - their words read, their arguments checked; the place in the game's window
TEST(mcp_keys_and_clicks) {
    mcp::KeyCombo k;
    std::string why;
    CHECK(mcp::keyCombo("alt+e", k, why) && k.alt && !k.ctrl && !k.shift && k.vk == 'E');
    CHECK(mcp::keyCombo("Ctrl+Shift+V", k, why) && k.ctrl && k.shift && !k.alt && k.vk == 'V');
    CHECK(mcp::keyCombo("enter", k, why) && k.vk == 0x0D && !k.extended);
    CHECK(mcp::keyCombo("esc", k, why) && k.vk == 0x1B);
    CHECK(mcp::keyCombo("f12", k, why) && k.vk == 0x7B);
    CHECK(mcp::keyCombo("up", k, why) && k.vk == 0x26 && k.extended);
    CHECK(mcp::keyCombo("7", k, why) && k.vk == '7');
    CHECK(mcp::keyCombo("space", k, why) && k.vk == 0x20);
    CHECK(!mcp::keyCombo("", k, why) && !why.empty());
    CHECK(!mcp::keyCombo("a+b", k, why));  // one key with its modifiers
    CHECK(!mcp::keyCombo("hyper+x", k, why));
    CHECK(!mcp::keyCombo("ctrl", k, why));  // a modifier alone
    CHECK(!mcp::keyCombo("alt+", k, why));
    json::Value v;
    std::string e;
    const auto args = [&](const char* text) {
        json::parse(text, v, e);
        return v;
    };
    CHECK(mcp::checkArgs("key", args(R"({"keys":"alt+e"})")).empty());
    CHECK(!mcp::checkArgs("key", args(R"({"keys":"alt+"})")).empty());
    CHECK(mcp::checkArgs("click", args(R"({"x":510,"y":300,"width":1020})")).empty());
    CHECK(mcp::checkArgs("click", args(R"({"x":10,"y":10,"width":1020,"button":"right","double":true})")).empty());
    CHECK(!mcp::checkArgs("click", args(R"({"x":1100,"y":10,"width":1020})")).empty());  // past the picture
    CHECK(!mcp::checkArgs("click", args(R"({"x":10,"y":10})")).empty());                  // the picture's width
    CHECK(!mcp::checkArgs("click", args(R"({"x":10,"y":10,"width":1020,"button":"middle"})")).empty());
    // the place in the window: the screenshot 1020 wide of a 2040 x 1147 window
    int cx = 0, cy = 0;
    CHECK(mcp::clickPlace(510, 286, 1020, 2040, 1147, cx, cy) && cx == 1020 && cy == 572);
    CHECK(mcp::clickPlace(0, 0, 1020, 2040, 1147, cx, cy) && cx == 0 && cy == 0);
    CHECK(!mcp::clickPlace(10, 600, 1020, 2040, 1147, cx, cy));  // under the picture
    CHECK(!mcp::clickPlace(10, 10, 1020, 0, 0, cx, cy));         // no window
}

// 3.0.22-beta.37 (the owner 20.11: «я хочу чтобы клиент мог работать в фоне, когда ты проверки делаешь»): a key told to
// the game's window as Windows tells it - its repeat count, scan code, extended key, Alt (bit 29), the let go's bits
TEST(mcp_key_messages_carry_the_key_as_windows_does) {
    CHECK_EQ(mcp::keyMessageBits(0x12, false, true, false), 0x20120001u);  // Alt+E down: E's scan 0x12, Alt
    CHECK_EQ(mcp::keyMessageBits(0x12, false, true, true), 0xE0120001u);   // let go: was down, now up
    CHECK_EQ(mcp::keyMessageBits(0x01, false, false, false), 0x00010001u);  // Esc
    CHECK_EQ(mcp::keyMessageBits(0x48, true, false, false), 0x01480001u);  // Up: extended
}

// 3.0.22-beta.40 (the owner over the MCP: «навожу» - the tips needed his hand): the game's cursor held at a place
// without a click, its tips shown
TEST(mcp_hover_holds_the_cursor_at_a_place) {
    json::Value v;
    std::string e;
    const auto args = [&](const char* text) {
        json::parse(text, v, e);
        return v;
    };
    CHECK(mcp::checkArgs("hover", args(R"({"x":245,"y":135,"width":1020})")).empty());
    CHECK(mcp::checkArgs("hover", args(R"({"x":245,"y":135,"width":1020,"seconds":20})")).empty());
    CHECK(!mcp::checkArgs("hover", args(R"({"x":245,"y":135,"width":1020,"seconds":0})")).empty());
    CHECK(!mcp::checkArgs("hover", args(R"({"x":245,"y":135,"width":1020,"seconds":31})")).empty());
    CHECK(!mcp::checkArgs("hover", args(R"({"x":2000,"y":135,"width":1020})")).empty());
    bool listed = false;
    for (const mcp::Tool& t : mcp::tools()) listed = listed || std::string(t.name) == "hover";
    CHECK(listed);
}

// 3.0.22-beta.41: the driver's log read over the MCP - its last lines, those with a word only
TEST(mcp_log_tail_with_a_word) {
    const std::string text = "a one\r\nb two\nc one\n\nd three\ne one";
    CHECK_EQ(mcp::tailLines(text, 2, ""), std::string("d three\ne one"));
    CHECK_EQ(mcp::tailLines(text, 5, "one"), std::string("a one\nc one\ne one"));
    CHECK_EQ(mcp::tailLines(text, 1, "ONE"), std::string("e one"));  // any letters' size
    CHECK_EQ(mcp::tailLines(text, 3, "nothing"), std::string());
    json::Value v;
    std::string e;
    json::parse(R"({"lines":50,"match":"items:"})", v, e);
    CHECK(mcp::checkArgs("log", v).empty());
    json::parse(R"({"lines":5000})", v, e);
    CHECK(!mcp::checkArgs("log", v).empty());
}

// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 3): the log tool reads the renderer's and
// the pad's logs too, and only the lines since a moment - what an action wrote
TEST(mcp_log_since_and_files) {
    const std::string text = "12:00:01.000 INFO  a\n12:00:05.000 INFO  b\n  its next line\n12:00:09.250 WARN  c\n";
    CHECK_EQ(mcp::tailLines(text, 60, "", "12:00:05"), std::string("12:00:05.000 INFO  b\n  its next line\n"
                                                                    "12:00:09.250 WARN  c"));
    CHECK_EQ(mcp::tailLines(text, 60, "warn", "12:00:05"), std::string("12:00:09.250 WARN  c"));
    CHECK_EQ(mcp::tailLines(text, 60, "", "12:00:09.5"), std::string());
    CHECK_EQ(mcp::tailLines(text, 2, "", ""), std::string("  its next line\n12:00:09.250 WARN  c"));
    json::Value a = json::Value::object();
    a.set("file", "render64");
    a.set("since", "12:00:05");
    CHECK(mcp::checkArgs("log", a).empty());
    a.set("file", "pad");
    a.set("since", "12:00:05.250");
    CHECK(mcp::checkArgs("log", a).empty());
    a.set("file", "c:\\windows");
    CHECK(!mcp::checkArgs("log", a).empty());
    a.set("file", "graphics");
    a.set("since", "noon");
    CHECK(!mcp::checkArgs("log", a).empty());
    // the other logs' names beside the driver's (a second game window's end in -2)
    CHECK_EQ(mcp::logNamed("C:\\g\\uaro-graphics\\logs\\uaro-graphics.log", "render64"),
             std::string("C:\\g\\uaro-graphics\\logs\\uaro-render64.log"));
    CHECK_EQ(mcp::logNamed("C:\\g\\uaro-graphics\\logs\\uaro-graphics-2.log", "pad"),
             std::string("C:\\g\\uaro-graphics\\logs\\uaro-pad-2.log"));
    CHECK_EQ(mcp::logNamed("C:\\g\\uaro-graphics\\logs\\uaro-graphics.log", "graphics"),
             std::string("C:\\g\\uaro-graphics\\logs\\uaro-graphics.log"));
}
