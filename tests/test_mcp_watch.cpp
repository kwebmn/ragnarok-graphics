// 3.0.24-beta.35 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 7; the owner: «давай все через mcp»): the
// settings app's own MCP - the watchdog beside the game (its port + 1): the game started, closed, killed; the
// computer's load; the logs, the dumps, the ini
#include <string>

#include "core/json.hpp"
#include "core/mcp.hpp"
#include "core/mcp_watch.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

json::Value answerOf(const std::string& out) {
    json::Value v;
    std::string error;
    json::parse(out, v, error);
    return v;
}

}  // namespace

TEST(mcp_watch_catalog) {
    const mcp::Catalog& w = mcpwatch::catalog();
    int calls = 0;
    const mcp::CallFn run = [&](const std::string& tool, const json::Value&) {
        ++calls;
        mcp::CallResult r;
        r.content.push_back(mcp::Content::textOf("ran " + tool));
        return r;
    };
    const json::Value init = answerOf(mcp::handle(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}})", run, "x", w));
    CHECK_EQ(init["result"]["serverInfo"]["name"].asString(), std::string("uaro-graphics-watch"));
    const json::Value list = answerOf(mcp::handle(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})", run, "x", w));
    std::string names;
    for (const json::Value& t : list["result"]["tools"].items()) names += t["name"].asString() + " ";
    CHECK_EQ(names, std::string("game system files ini "));
    // the game's own: as before
    const json::Value game = answerOf(mcp::handle(
        R"({"jsonrpc":"2.0","id":3,"method":"initialize","params":{"protocolVersion":"2025-06-18"}})", run, "x"));
    CHECK_EQ(game["result"]["serverInfo"]["name"].asString(), std::string("uaro-graphics"));
    // the watchdog's arguments checked before a call
    const json::Value bad = answerOf(mcp::handle(
        R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"game","arguments":{"action":"format"}}})",
        run, "x", w));
    CHECK(bad["result"]["isError"].asBool() && calls == 0);
    const json::Value ok = answerOf(mcp::handle(
        R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"game","arguments":{"action":"status"}}})",
        run, "x", w));
    CHECK(!ok["result"]["isError"].asBool() && calls == 1);
    // the game's tools are not the watchdog's
    const json::Value none = answerOf(mcp::handle(
        R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"screenshot","arguments":{}}})", run, "x",
        w));
    CHECK(none.has("error") && calls == 1);
    json::Value a = json::Value::object();
    a.set("seconds", 11);
    CHECK(!w.check("system", a).empty());
    a.set("seconds", 2);
    CHECK(w.check("system", a).empty());
    json::Value f = json::Value::object();
    f.set("action", "tail");
    f.set("name", "logs\\uaro-render64.log");
    CHECK(w.check("files", f).empty());
    f.set("name", "..\\..\\Windows\\win.ini");
    CHECK(!w.check("files", f).empty());
    json::Value i = json::Value::object();
    i.set("action", "set");
    i.set("key", "renderer.api");
    i.set("value", "d3d11");
    CHECK(w.check("ini", i).empty());
    json::Value noValue = json::Value::object();
    noValue.set("action", "set");
    noValue.set("key", "renderer.api");
    CHECK(!w.check("ini", noValue).empty());  // a set needs its value
}

TEST(mcp_watch_files_inside_alone) {
    CHECK(mcpwatch::fileAllowed("uaro-graphics.ini"));
    CHECK(mcpwatch::fileAllowed("logs\\uaro-graphics.log"));
    CHECK(mcpwatch::fileAllowed("logs/uaro-render64-2.log"));
    CHECK(mcpwatch::fileAllowed("crash-render64.dmp"));
    CHECK(mcpwatch::fileAllowed("logs\\uaro-disconnect.txt-2"));
    CHECK(!mcpwatch::fileAllowed("..\\..\\Windows\\win.ini"));
    CHECK(!mcpwatch::fileAllowed("logs\\..\\..\\x.ini"));
    CHECK(!mcpwatch::fileAllowed("C:\\Windows\\win.ini"));
    CHECK(!mcpwatch::fileAllowed("\\\\server\\share\\x.log"));
    CHECK(!mcpwatch::fileAllowed("uaro-graphics.exe"));
    CHECK(!mcpwatch::fileAllowed("cache\\deep\\x.log"));
    CHECK(!mcpwatch::fileAllowed(""));
}

TEST(mcp_watch_nvidia_smi_line) {
    const json::Value g = mcpwatch::nvidiaSmi(
        "NVIDIA GeForce RTX 3060 Laptop GPU, 45, 71, 1830, 7000, 85.32, 2345, 6144, P2, 0x0000000000000004\r\n");
    CHECK_EQ(g["name"].asString(), std::string("NVIDIA GeForce RTX 3060 Laptop GPU"));
    CHECK(g["gpuPercent"].asInt() == 45 && g["temperatureC"].asInt() == 71 && g["clockMhz"].asInt() == 1830);
    CHECK(g["memoryUsedMb"].asInt() == 2345 && g["memoryTotalMb"].asInt() == 6144);
    CHECK_EQ(g["pstate"].asString(), std::string("P2"));
    CHECK_EQ(g["throttle"].asString(), std::string("power cap"));  // 0x4: SW power cap
    CHECK(mcpwatch::nvidiaSmi("").type() == json::Value::Null);
    const json::Value idle = mcpwatch::nvidiaSmi("X, 0, 40, 210, 405, [N/A], 512, 6144, P8, 0x0000000000000001\n");
    CHECK(!idle.has("powerW") && idle["throttle"].asString() == "idle");
}

// 3.0.24-beta.37 (the laptop's run: beta.36 out, the laptop could not take it without a press on «ОНОВИТИ»): the
// watchdog's game update - as «ОНОВИТИ» (the app updated in a window of its own and opened again), never over a game
// that runs; a check asked first when there is none
TEST(mcp_watch_update_steps) {
    using mcpwatch::UpdateStep;
    const mcp::Catalog& w = mcpwatch::catalog();
    json::Value a = json::Value::object();
    a.set("action", "update");
    CHECK(w.check("game", a).empty());
    CHECK(mcpwatch::updateStep(true, mcpwatch::Check::Done, "3.0.24-beta.36", "3.0.24-beta.36", "").step ==
          UpdateStep::Refuse);  // the game runs: stop it first
    CHECK(mcpwatch::updateStep(false, mcpwatch::Check::Idle, "", "", "").step == UpdateStep::Check);
    CHECK(mcpwatch::updateStep(false, mcpwatch::Check::Failed, "", "", "").step == UpdateStep::Check);
    CHECK(mcpwatch::updateStep(false, mcpwatch::Check::Checking, "", "", "").step == UpdateStep::Wait);
    const mcpwatch::UpdateWhat newest =
        mcpwatch::updateStep(false, mcpwatch::Check::Done, "3.0.24-beta.37", "3.0.24-beta.37", "");
    CHECK(newest.step == UpdateStep::Newest);
    CHECK(newest.words.find("3.0.24-beta.37") != std::string::npos);
    const mcpwatch::UpdateWhat go =
        mcpwatch::updateStep(false, mcpwatch::Check::Done, "3.0.24-beta.36", "3.0.24-beta.37", "3.0.24-beta.37");
    CHECK(go.step == UpdateStep::Update);
    CHECK(go.words.find("3.0.24-beta.37") != std::string::npos);
}
