// 3.0.22-beta.32 (the owner 20.11: «мсп сервер, через который можно было бы управлять клиентом», «чтобы ты мог ходить и
// делать правильные сцены»; docs/superpowers/specs/2026-11-20-mcp-design.md): the MCP's tools on the game's thread -
// win/mcp_server takes the requests, core/mcp checked their arguments; the screenshot is the server's own
#include <cmath>
#include <string>

#include "core/settings/schema.hpp"
#include "core/version.hpp"
#include "core/view3d.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_wheel.hpp"
#include "win/code_page.hpp"
#include "win/context.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/view3d.hpp"

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

}  // namespace

mcp::CallResult Context::mcpCall(const std::string& tool, const json::Value& args) {
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
        s.set("view", view);
        s.set("pause", !enhanced_);
        s.set("photo", photo_);
        json::Value picture = json::Value::object();
        picture.set("clock", token(config, "effects.clock"));
        picture.set("weather", token(config, "effects.weather_kind"));
        picture.set("season", token(config, "effects.season"));
        s.set("picture", picture);
        return said(json::dump(s));
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
