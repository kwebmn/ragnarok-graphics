#include "core/mcp_watch.hpp"

#include <cstdlib>
#include <regex>
#include <vector>

#include "core/settings/schema.hpp"

namespace uagfx::mcpwatch {
namespace {

const std::vector<mcp::Tool>& tools() {
    static const std::vector<mcp::Tool> t = {
        {"game",
         "The game on this computer: `action` status (whether uaRO.exe runs and since when, whether its window "
         "answers, whether its own MCP - the port below this one - answers, the renderer's process), start (as the "
         "launcher's play: the game with uaRO Graphics; refused while one runs unless `another`), stop (closed as "
         "its own close does, waited for up to 15 s), kill (its processes ended at once - for a hang), update "
         "(uaRO Graphics updated to its channel's newest as «ОНОВИТИ» does: the app closes, updates and opens again "
         "in a minute or so, this MCP back with it; the game closed first; a check asked first when there is none), "
         "front (the game's window brought in front: at full screen the game draws nothing while it is behind - "
         "status says inFront; a start brings it in front by itself).",
         R"({"type":"object","properties":{"action":{"type":"string",)"
         R"("enum":["status","start","stop","kill","update","front"]},)"
         R"("another":{"type":"boolean"}},"required":["action"]})"},
        {"system",
         "The computer's load over `seconds` (1-10, 2 unless said): the processor (all of it, the game's, the "
         "renderer's), memory (free; the game's and the renderer's), the power (mains or battery, its per cent, the "
         "plan); an NVIDIA card's load, temperature, clocks, power, video memory and why it slows (nvidia-smi) when "
         "there is one.",
         R"({"type":"object","properties":{"seconds":{"type":"integer","minimum":1,"maximum":10}}})"},
        {"files",
         "uaro-graphics\\'s own files - the ini, logs\\*, the crash dumps: `action` list (names, sizes, times), tail "
         "(a text file's last `lines`, 1-500, 100 unless said; `since` \"hh:mm:ss\" - those from then), get (a file "
         "whole as base64, 8 MB at most - a dump). `name` as list gives it (e.g. logs\\uaro-render64.log).",
         R"({"type":"object","properties":{"action":{"type":"string","enum":["list","tail","get"]},)"
         R"("name":{"type":"string","maxLength":120},"lines":{"type":"integer","minimum":1,"maximum":500},)"
         R"("since":{"type":"string","maxLength":12}},"required":["action"]})"},
        {"ini",
         "uaro-graphics.ini: `action` get (a key's value, «section.name») or set (`value`, checked as Alt+O would). "
         "The game takes most at once, some at its next start (renderer.api: game stop, then start).",
         R"({"type":"object","properties":{"action":{"type":"string","enum":["get","set"]},)"
         R"("key":{"type":"string","maxLength":60},"value":{"type":"string","maxLength":200}},)"
         R"("required":["action","key"]})"},
    };
    return t;
}

bool whole(const json::Value& v, long lo, long hi) {
    return v.type() == json::Value::Number && v.asNumber() == double(v.asInt()) && v.asInt() >= lo && v.asInt() <= hi;
}

std::string check(const std::string& tool, const json::Value& args) {
    if (args.type() != json::Value::Object) return "the arguments must be an object";
    if (tool == "game") {
        const std::string a = args["action"].asString();
        if (a != "status" && a != "start" && a != "stop" && a != "kill" && a != "update" && a != "front")
            return "action: status, start, stop, kill, update or front";
        if (args.has("another") && args["another"].type() != json::Value::Bool) return "another: true or false";
    } else if (tool == "system") {
        if (args.has("seconds") && !whole(args["seconds"], 1, 10)) return "seconds: 1-10";
    } else if (tool == "files") {
        const std::string a = args["action"].asString();
        if (a != "list" && a != "tail" && a != "get") return "action: list, tail or get";
        if (a != "list" && !fileAllowed(args["name"].asString()))
            return "name: one of uaro-graphics\\'s files as list gives it (the ini, logs\\..., a dump)";
        if (args.has("lines") && !whole(args["lines"], 1, 500)) return "lines: 1-500";
        static const std::regex time("^[0-2][0-9]:[0-5][0-9](:[0-5][0-9](\\.[0-9]{1,3})?)?$");
        if (args.has("since") && !std::regex_match(args["since"].asString(), time))
            return "since: hh:mm, hh:mm:ss or hh:mm:ss.mmm";
    } else if (tool == "ini") {
        const std::string a = args["action"].asString();
        if (a != "get" && a != "set") return "action: get or set";
        const settings::IniSetting* s = settings::findSetting(args["key"].asString());
        if (!s) return "key: a setting's «section.name» (the game's settings tool lists them)";
        const json::Value& v = args["value"];
        if (a == "set" && (v.type() != json::Value::String || !settings::validToken(*s, v.asString())))
            return "value: one the setting takes (the game's settings tool says which)";
    }
    return "";
}

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : line) {
        if (ch == '\r' || ch == '\n') break;
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else if (!(ch == ' ' && cur.empty())) {
            cur += ch;
        }
    }
    if (!cur.empty() || !out.empty()) out.push_back(cur);
    for (std::string& f : out)
        while (!f.empty() && f.back() == ' ') f.pop_back();
    return out;
}

std::string throttleWords(unsigned long long bits) {
    static const std::pair<unsigned long long, const char*> kinds[] = {
        {0x1, "idle"},         {0x2, "app clocks"}, {0x4, "power cap"},   {0x8, "hardware slowdown"},
        {0x20, "thermal (software)"}, {0x40, "thermal (hardware)"}, {0x80, "power brake"}, {0x100, "display clocks"}};
    std::string out;
    for (const auto& [bit, word] : kinds)
        if (bits & bit) out += (out.empty() ? "" : ", ") + std::string(word);
    return out.empty() ? "none" : out;
}

}  // namespace

const mcp::Catalog& catalog() {
    static const mcp::Catalog c{
        "uaro-graphics-watch", "uaRO Graphics watchdog",
        "The uaRO Graphics settings app beside the game on the player's computer: start, stop or kill the game "
        "(game), see the computer's load (system), read its logs and dumps (files), change uaro-graphics.ini (ini). "
        "The game itself answers at the port below this one.",
        &tools(), &check};
    return c;
}

bool fileAllowed(const std::string& raw) {
    std::string name = raw;
    for (char& ch : name)
        if (ch == '/') ch = '\\';
    if (name.empty() || name.size() > 120 || name.find(':') != std::string::npos || name[0] == '\\') return false;
    std::vector<std::string> parts;
    size_t at = 0;
    while (at <= name.size()) {
        const size_t e = std::min(name.find('\\', at), name.size());
        parts.push_back(name.substr(at, e - at));
        at = e + 1;
    }
    if (parts.size() > 2 || (parts.size() == 2 && parts[0] != "logs")) return false;
    const std::string& file = parts.back();
    if (file.empty() || file == "." || file == ".." || file.find("..") != std::string::npos) return false;
    static const std::regex kinds("^[A-Za-z0-9_.-]+\\.(ini|log|txt|dmp|json|csv)(-[0-9]+)?$");
    return std::regex_match(file, kinds);
}

json::Value nvidiaSmi(const std::string& line) {
    const std::vector<std::string> f = fields(line);
    if (f.size() < 10) return json::Value();
    json::Value g = json::Value::object();
    g.set("name", f[0]);
    const auto number = [&](size_t i, const char* key, bool real) {
        if (f[i].empty() || f[i][0] == '[') return;
        char* end = nullptr;
        const double v = std::strtod(f[i].c_str(), &end);
        if (end == f[i].c_str()) return;
        if (real) g.set(key, v);
        else g.set(key, int64_t(v));
    };
    number(1, "gpuPercent", false);
    number(2, "temperatureC", false);
    number(3, "clockMhz", false);
    number(4, "memClockMhz", false);
    number(5, "powerW", true);
    number(6, "memoryUsedMb", false);
    number(7, "memoryTotalMb", false);
    g.set("pstate", f[8]);
    g.set("throttle", throttleWords(std::strtoull(f[9].c_str(), nullptr, 16)));
    return g;
}

UpdateWhat updateStep(bool gameRuns, Check check, const std::string& installed, const std::string& latest,
                      const std::string& offer, const std::string& error, bool fresh) {
    if (gameRuns) return {UpdateStep::Refuse, "the game runs - close it first (game stop), then update"};
    switch (check) {
        case Check::Idle: return {UpdateStep::Check, "the update check asked now - ask again in a few seconds"};
        case Check::Failed:
            return {UpdateStep::Check, "the last update check failed (" + error + ") - asked again now, ask again in "
                                       "a few seconds"};
        case Check::Checking: return {UpdateStep::Wait, "the update check runs - ask again in a few seconds"};
        case Check::Done: break;
    }
    if (offer.empty() && !fresh)
        return {UpdateStep::Check, "the last update check is old - asked again now, ask again in a few seconds"};
    if (offer.empty())
        return {UpdateStep::Newest, "uaRO Graphics " + installed + " is its channel's newest (" + latest + ")"};
    return {UpdateStep::Update, "updating " + installed + " to " + offer + ": the app closes, updates and opens again "
                                "in a minute or so - this MCP back with it (game status to see)"};
}

}  // namespace uagfx::mcpwatch
