// 3.0.22-beta.32 (the owner 20.11: «мсп сервер, через который можно было бы управлять клиентом», «чтобы ты мог ходить и
// делать правильные сцены»; docs/superpowers/specs/2026-11-20-mcp-design.md): the MCP's tools on the game's thread -
// win/mcp_server takes the requests, core/mcp checked their arguments; the screenshot is the server's own
#include <cmath>
#include <string>

#include "core/mcp_perf.hpp"
#include "core/settings/schema.hpp"
#include "core/version.hpp"
#include "core/view3d.hpp"
#include "pad/win/remote.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_wheel.hpp"
#include "win/code_page.hpp"
#include "win/context.hpp"
#include "win/dpi.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/mcp_input.hpp"
#include "win/mcp_server.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/time_lapse.hpp"
#include "win/uaui.hpp"
#include "win/view3d.hpp"
#include "win/window_stack.hpp"
#include "win/window_mode.hpp"

namespace uagfx::win {
namespace {

mcp::CallResult said(const std::string& text, bool error = false) {
    mcp::CallResult r;
    r.isError = error;
    r.content.push_back(mcp::Content::textOf(text));
    return r;
}

const char* modeWord(view3d::Mode m) {
    return m == view3d::Mode::Behind ? "behind" : m == view3d::Mode::First ? "first" : "off";
}

double tenth(float v) { return std::round(double(v) * 10.0) / 10.0; }

std::string token(const Config& c, const char* key) {
    const settings::IniSetting* s = settings::findSetting(key);
    return s ? settings::tokenOf(*s, c) : std::string();
}

// The game's process's memory, MB (its working set; K32GetProcessMemoryInfo by name - the driver imports it not); -1
// unknown
double processMemoryMb() {
    struct Counters {
        DWORD cb;
        DWORD pageFaults;
        SIZE_T peakWorkingSet, workingSet, quotaPeakPaged, quotaPaged, quotaPeakNonPaged, quotaNonPaged, pagefile,
            peakPagefile;
    };
    using Fn = BOOL(WINAPI*)(HANDLE, Counters*, DWORD);
    static const Fn get = reinterpret_cast<Fn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo")));
    Counters c{};
    c.cb = sizeof c;
    if (!get || !get(GetCurrentProcess(), &c, sizeof c)) return -1;
    return double(c.workingSet) / (1024.0 * 1024.0);
}

}  // namespace

void Context::mcpFrame(bool shown) {
    mcpinput::tick();  // 3.0.22-beta.40: a hover's time up - the cursor the player's again
    mcpwin::Seen seen;
    seen.window = window;
    seen.shown = shown;
    seen.gameWidth = int(shadow.width());
    seen.gameHeight = int(shadow.height());
    mcpwin::frame(config.mcpEnabled, config.mcpPort, gameDir(), seen,
                  [this](const std::string& t, const json::Value& a) { return mcpCall(t, a); });
}

mcp::CallResult Context::mcpCall(const std::string& tool, const json::Value& args) {
    if (tool == "perf") {  // 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 2)
        const double seconds = args.has("seconds") ? double(args["seconds"].asInt()) : 10.0;
        mcpperf::Facts f;
        f.api = apiLabel_;
        f.adapter = renderer ? renderer->adapterName() : startup().adapter;
        f.map = map_;
        RECT rc{};
        if (window && GetClientRect(window, &rc))
            f.window = std::to_string(rc.right - rc.left) + "x" + std::to_string(rc.bottom - rc.top);
        SYSTEM_POWER_STATUS ps{};
        if (GetSystemPowerStatus(&ps) && ps.ACLineStatus <= 1) {
            f.mains = ps.ACLineStatus;
            if (ps.BatteryLifePercent <= 100) f.batteryPercent = ps.BatteryLifePercent;
        }
        f.plan = startup().computer.plan;
        f.gameMemoryMb = processMemoryMb();
        return said(json::dump(mcpperf::report(frameStats_.stats(uint64_t(nowMs() * 1000.0), seconds),
                                               lastGpuKnown_ ? &lastGpu_ : nullptr, f)));
    }
    if (tool == "state") {
        json::Value s = json::Value::object();
        s.set("client", std::string("uaRO Graphics ") + kVersion);
        s.set("map", readGameMap());
        int x = 0, y = 0, w = 0, h = 0;
        if (readPlayerCell(x, y, w, h)) {
            json::Value cell = json::Value::object();
            cell.set("x", x);
            cell.set("y", y);
            s.set("cell", cell);
            json::Value size = json::Value::object();
            size.set("width", w);
            size.set("height", h);
            s.set("mapSize", size);
        }
        json::Value view = json::Value::object();
        view.set("mode", modeWord(view3dwin::wanted()));
        float yaw = 0.0f, pitch = 0.0f;
        if (view3dwin::look(yaw, pitch)) {
            view.set("yaw", tenth(yaw));
            view.set("pitch", tenth(pitch));
        }
        view.set("freeCamera", view3dwin::freeCamera());
        int points = 0;
        bool flying = false;
        float seconds = 0.0f;
        view3dwin::flightNow(points, flying, seconds);
        json::Value flight = json::Value::object();
        flight.set("points", points);
        flight.set("flying", flying);
        flight.set("seconds", tenth(seconds));
        view.set("flight", flight);
        s.set("view", view);
        s.set("pause", !enhanced_);
        s.set("photo", photo_);
        json::Value picture = json::Value::object();
        picture.set("clock", token(config, "effects.clock"));
        picture.set("weather", token(config, "effects.weather_kind"));
        picture.set("season", token(config, "effects.season"));
        const int lapse = timelapsewin::state();
        picture.set("timelapse", lapse == 1 ? "running" : lapse == 2 ? "held" : "off");
        s.set("picture", picture);
        if (window) {  // 3.0.22-beta.35 (the owner's window without its title): the window as Windows has it
            // 3.0.22-beta.36: its real pixels - the picture screenshots come from (the game's thread is told points)
            const SIZE px = clientPixels(window, true);
            const LONG style = GetWindowLongA(window, GWL_STYLE);
            json::Value w = json::Value::object();
            w.set("width", int(px.cx));
            w.set("height", int(px.cy));
            w.set("frame", (style & WS_CAPTION) == WS_CAPTION);
            w.set("front", GetForegroundWindow() == window);
            w.set("minimised", IsIconic(window) != 0);
            w.set("gameFullScreen", gameFullscreen);
            w.set("game", std::to_string(shadow.width()) + "x" + std::to_string(shadow.height()));
            w.set("dpi", windowDpiAwareness(window));
            w.set("inputInside", uapad::remote::polled());  // 3.0.22-beta.37: keys and clicks behind other windows
            s.set("window", w);
        }
        return said(json::dump(s));
    }
    if (tool == "windows") {  // 3.0.22-beta.39
        json::Value out = json::Value::object();
        if (window) {
            const SIZE px = clientPixels(window, config.highResolution);
            out.set("picture", std::to_string(px.cx) + "x" + std::to_string(px.cy));
        }
        out.set("windowStack", windowstack::describe());
        out.set("uaui", uaui::describe());
        return said(json::dump(out));
    }
    if (tool == "chat") {
        const std::string line = args["line"].asString();
        if (!chatwheelwin::sendCommand(narrowed(utf8Wide(line), gameCodePage(config))))
            return said("no chat window in the game now - the character must be in the world", true);
        return said("sent: " + line);
    }
    if (tool == "walk") {
        const int x = int(args["x"].asInt()), y = int(args["y"].asInt());
        if (!uapad::runtime::walkTo(x, y))
            return said("a walk cannot be asked now - no map server connection the client can add to (the character "
                        "in the world, [pad] enabled = 1)",
                        true);
        return said("walking to " + std::to_string(x) + ", " + std::to_string(y) + " - see `state` in a moment");
    }
    if (tool == "view") {
        if (!enhanced_) return said("Pause is on - the old client's own camera; `pause` off first", true);
        std::string out;
        if (args.has("mode")) {
            const std::string m = args["mode"].asString();
            const view3d::Mode mode = m == "behind" ? view3d::Mode::Behind
                                      : m == "first" ? view3d::Mode::First
                                                     : view3d::Mode::Off;
            if (mode != view3d::Mode::Off && !view3dwin::available())
                return said("the 3D view does not work with this build of uaRO.exe", true);
            if (!view3dwin::ask(mode, "the MCP")) return said("the view cannot be asked now - not in the world", true);
            out = "the view " + m;
        }
        if (args.has("yaw") || args.has("pitch")) {
            const float yaw = args.has("yaw") ? float(args["yaw"].asNumber()) : NAN;
            const float pitch = args.has("pitch") ? float(args["pitch"].asNumber()) : NAN;
            if (!view3dwin::setLook(yaw, pitch))
                return said((out.empty() ? "" : out + "; ") + "a look needs the 3D view - mode behind or first", true);
            out += (out.empty() ? "" : ", ") + std::string("the look put");
        }
        return said(out + " - from the next frame");
    }
    if (tool == "pause") {
        const bool on = args["on"].asBool();
        if (on == !enhanced_) return said(on ? "Pause is on already" : "Pause is off already");
        toggleCompare();
        return said(on ? "Pause on - the old client's picture and windows, the 3D view held"
                       : "Pause off - the improvements back, the 3D view as it was");
    }
    if (tool == "photo") {
        const bool on = args["on"].asBool();
        if (photo_ != on) {
            photo_ = on;
            LOG_INFO("photo mode: %s (the MCP)", on ? "on - the game's interface, the cursor and the panel hidden"
                                                     : "off");
        }
        return said(on ? "the photo mode on - the interface out of the picture" : "the photo mode off");
    }
    if (tool == "camera") {  // 3.0.22-beta.33
        if (!enhanced_) return said("Pause is on - `pause` off first", true);
        std::string out;
        if (args.has("free")) {
            const bool on = args["free"].asBool();
            if (!view3dwin::freeOn(on))
                return said("the free camera needs the 3D view - `view` mode behind or first", true);
            out = on ? "the free camera on" : "the free camera off - back with the character";
        }
        if (args.has("move")) {
            const json::Value& m = args["move"];
            if (!view3dwin::freeMove(float(m["forward"].asNumber()), float(m["right"].asNumber()),
                                     float(m["up"].asNumber())))
                return said((out.empty() ? "" : out + "; ") + "a move needs the free camera - `camera` free true",
                            true);
            out += (out.empty() ? "" : ", ") + std::string("moved");
        }
        return said(out + " - from the next frame; look with `screenshot`");
    }
    if (tool == "flight") {
        const std::string action = args["action"].asString();
        if (!view3dwin::flight(action))
            return said("the flight needs the free camera in the 3D view - `camera` free true", true);
        int points = 0;
        bool flying = false;
        float seconds = 0.0f;
        view3dwin::flightNow(points, flying, seconds);
        if (action == "point") return said("a point added (" + std::to_string(points + 1) + " of 9 at most)");
        if (action == "play")
            return said(points >= 2 ? "flying through " + std::to_string(points) + " points, " +
                                          std::to_string(int(seconds + 0.5f)) + " s"
                                    : "two points at least - `flight` point",
                        points < 2);
        return said(action == "stop" ? "stopped" : "the points cleared");
    }
    if (tool == "timelapse") {
        const std::string action = args["action"].asString();
        timelapsewin::ask(action == "run" ? 1 : action == "hold" ? 2 : 0);
        return said(action == "run"    ? "the time runs fast ([effects] timelapse_speed minutes a second)"
                    : action == "hold" ? "the time stopped where it is"
                                       : "the clock's own time again");
    }
    if (tool == "setting") {
        const std::string key = args["key"].asString(), value = args["value"].asString();
        if (!settings::findSetting(key)) return said("no such setting: " + key, true);
        const size_t dot = key.find('.');
        if (!editIniFile(ourFile(layout::File::Ini), {{key.substr(0, dot), key.substr(dot + 1), value}}))
            return said("uaro-graphics.ini could not be written", true);
        return said("written: " + key + " = " + value + " - applied in a moment (a few settings need a restart)");
    }
    return said("no such tool: " + tool, true);
}

}  // namespace uagfx::win
