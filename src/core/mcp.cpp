#include "core/mcp.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace uagfx::mcp {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

std::string base64(const std::string& bytes) {
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
        const uint32_t n = uint32_t(uint8_t(bytes[i])) << 16 | uint32_t(uint8_t(bytes[i + 1])) << 8 |
                           uint32_t(uint8_t(bytes[i + 2]));
        out += kAlphabet[n >> 18 & 63];
        out += kAlphabet[n >> 12 & 63];
        out += kAlphabet[n >> 6 & 63];
        out += kAlphabet[n & 63];
    }
    if (i < bytes.size()) {
        uint32_t n = uint32_t(uint8_t(bytes[i])) << 16;
        if (i + 1 < bytes.size()) n |= uint32_t(uint8_t(bytes[i + 1])) << 8;
        out += kAlphabet[n >> 18 & 63];
        out += kAlphabet[n >> 12 & 63];
        out += i + 1 < bytes.size() ? kAlphabet[n >> 6 & 63] : '=';
        out += '=';
    }
    return out;
}

const char* statusText(int status) {
    switch (status) {
        case 200: return "OK";
        case 202: return "Accepted";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Internal Server Error";
    }
}

const std::vector<Tool>& table() {
    static const std::vector<Tool> kTools = {
        {"state",
         "Where the character is and what the client shows now: the map, the character's cell (x, y on the map's "
         "grid), the camera (the 3D view's mode, its yaw and pitch), Pause (the old client to compare), the photo "
         "mode, the time of day the picture uses, the game's window (its size, its frame, in front or not).",
         R"({"type":"object","properties":{}})"},
        {"screenshot",
         "The frame as the player sees it now, as a JPEG (1280 pixels wide unless `width` says, 320-1920). Look after "
         "every change of the scene. The game's window must be shown (not minimised).",
         R"({"type":"object","properties":{"width":{"type":"integer","minimum":320,"maximum":1920}}})"},
        {"chat",
         "A line typed into the game's chat and sent, as the player would: words to the players near, or a command - "
         "@warp prontera 156 204 (a GM's), /sit, /emotion... At most 200 characters, one line.",
         R"({"type":"object","properties":{"line":{"type":"string","minLength":1,"maxLength":200}},)"
         R"("required":["line"]})"},
        {"walk",
         "The character walks to the cell (x, y) of this map, as a click on the ground asks; the server finds the "
         "way. Read `state` for where it is; a few seconds for a long way.",
         R"({"type":"object","properties":{"x":{"type":"integer","minimum":0,"maximum":1023},)"
         R"("y":{"type":"integer","minimum":0,"maximum":1023}},"required":["x","y"]})"},
        {"view",
         "The camera: `mode` off (the game's own, from above), behind (the 3D view over the shoulder) or first (from "
         "the eyes); in the 3D view `yaw` (degrees, where it looks) and `pitch` (degrees, up positive, -89..89).",
         R"({"type":"object","properties":{"mode":{"type":"string","enum":["off","behind","first"]},)"
         R"("yaw":{"type":"number"},"pitch":{"type":"number","minimum":-89,"maximum":89}}})"},
        {"pause",
         "The old client to compare with, as Pause or Scroll Lock: on - every improvement off (the game's own "
         "picture and windows; the 3D view held), off - all of it back as it was.",
         R"({"type":"object","properties":{"on":{"type":"boolean"}},"required":["on"]})"},
        {"photo",
         "The photo mode (Ctrl+Alt+F12): on - the interface, the cursor and the windows' panel out of the picture.",
         R"({"type":"object","properties":{"on":{"type":"boolean"}},"required":["on"]})"},
        {"setting",
         "One line of uaro-graphics.ini, applied live: `key` section.name, `value` its text. E.g. effects.clock = "
         "auto|server|dawn|morning|day|sunset|night|deep_night; effects.weather_kind = auto|clear|rain|snow|leaves|"
         "sand; effects.season = auto|spring|summer|autumn|winter; frames.fps_counter = 0|1.",
         R"({"type":"object","properties":{"key":{"type":"string","pattern":"^[a-z0-9_]+\\.[a-z0-9_]+$"},)"
         R"("value":{"type":"string","maxLength":200}},"required":["key","value"]})"},
        // 3.0.22-beta.33 (the owner: «сделай» - stage 2): the cinema camera and the time running fast
        {"camera",
         "The 3D view's free camera (Ctrl+Shift+C): `free` on - off the character (it stays where it is), off - back "
         "with it; `move` it by cells along its look: forward (with the pitch), right, up - at most 200 a call, kept "
         "out of walls and the ground. Turn it with `view` yaw and pitch.",
         R"({"type":"object","properties":{"free":{"type":"boolean"},"move":{"type":"object","properties":)"
         R"({"forward":{"type":"number"},"right":{"type":"number"},"up":{"type":"number"}}}}})"},
        {"flight",
         "The cinema camera's flight (Ctrl+Shift+P, O, L) in the free camera: `point` - its place and look now as "
         "the next point (9 at most); `play` - a smooth flight through them ([view3d] free_speed sets its pace); "
         "`stop`; `clear` - the points gone. Record while it plays.",
         R"({"type":"object","properties":{"action":{"type":"string","enum":["point","play","stop","clear"]}},)"
         R"("required":["action"]})"},
        {"timelapse",
         "The time of day running fast (Ctrl+Shift+T): `run` - from now, [effects] timelapse_speed minutes a "
         "second; `hold` - stopped where it is; `off` - the clock's own again.",
         R"({"type":"object","properties":{"action":{"type":"string","enum":["run","hold","off"]}},)"
         R"("required":["action"]})"},
        // 3.0.22-beta.35 (the owner: «проверяй»): the login's cards, the game's windows - by keys and clicks
        {"key",
         "A key pressed and let go in the game, as the player would, with ctrl+, shift+ or alt+ before it: alt+e "
         "(the inventory), enter, esc, tab, space, f1-f24, up, down, left, right, a-z, 0-9... Given to the game from "
         "inside: its window may stay behind other windows (not minimised); the player's keyboard untouched.",
         R"({"type":"object","properties":{"keys":{"type":"string","minLength":1,"maxLength":40}},)"
         R"("required":["keys"]})"},
        // 3.0.22-beta.41: the driver's log - why something is as it is, without asking the player for a report
        {"log",
         "The driver's log (uaro-graphics\\logs): its last `lines` (1-300, 60 unless said), those with `match` in "
         "them only when given (letters' size aside) - e.g. match \"items:\" or \"mcp:\".",
         R"({"type":"object","properties":{"lines":{"type":"integer","minimum":1,"maximum":300},)"
         R"("match":{"type":"string","maxLength":100}}})"},
        // 3.0.22-beta.40 (the owner: «навожу» - the tips needed his hand)
        {"hover",
         "The game's cursor put at (x, y) of a screenshot `width` wide and held there `seconds` (1-30, 8 unless "
         "said) without a click - the tips and highlights of what it points at show; look with `screenshot`. A key "
         "or a click ends it. Given from inside the game, as `click`.",
         R"({"type":"object","properties":{"x":{"type":"number","minimum":0},"y":{"type":"number","minimum":0},)"
         R"("width":{"type":"integer","minimum":160,"maximum":7680},"seconds":{"type":"integer","minimum":1,)"
         R"("maximum":30}},"required":["x","y","width"]})"},
        // 3.0.22-beta.39: what the interface is made of now - to find why a part shows where it should not
        {"windows",
         "The interface's parts now: the movable windows the top first with their boxes, the window whose corner "
         "grip shows, the uaUI documents shown in their order (name, layer, z-index, box) and the element the "
         "game's cursor is over. Boxes in the picture's pixels: a screenshot `width` wide shows them times width / "
         "the picture's width.",
         R"({"type":"object","properties":{}})"},
        {"click",
         "A mouse click at (x, y) of a screenshot `width` wide (the width the screenshot came in), as the player "
         "would: `button` left (the default) or right, `double` for a double click. Given to the game from inside: "
         "its own cursor goes there, its window may stay behind other windows (not minimised), the player's mouse "
         "untouched.",
         R"({"type":"object","properties":{"x":{"type":"number","minimum":0},"y":{"type":"number","minimum":0},)"
         R"("width":{"type":"integer","minimum":160,"maximum":7680},"button":{"type":"string",)"
         R"("enum":["left","right"]},"double":{"type":"boolean"}},"required":["x","y","width"]})"},
    };
    return kTools;
}

const Tool* find(const std::string& name) {
    for (const Tool& t : table())
        if (name == t.name) return &t;
    return nullptr;
}

bool wholeIn(const json::Value& v, long lo, long hi) {
    if (v.type() != json::Value::Number) return false;
    const double n = v.asNumber();
    return std::isfinite(n) && n == std::floor(n) && n >= double(lo) && n <= double(hi);
}

bool oneLine(const std::string& s) { return s.find('\n') == std::string::npos && s.find('\r') == std::string::npos; }

bool keyName(const std::string& k) {
    const size_t dot = k.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 == k.size() || k.find('.', dot + 1) != std::string::npos)
        return false;
    for (char c : k)
        if (c != '.' && c != '_' && !std::islower(static_cast<unsigned char>(c)) &&
            !std::isdigit(static_cast<unsigned char>(c)))
            return false;
    return true;
}

json::Value resultOf(const CallResult& r) {
    json::Value content = json::Value::array();
    for (const Content& c : r.content) {
        json::Value item = json::Value::object();
        item.set("type", c.type);
        if (c.type == "image") {
            item.set("data", base64(c.bytes));
            item.set("mimeType", c.mime);
        } else {
            item.set("text", c.text);
        }
        content.push(item);
    }
    json::Value out = json::Value::object();
    out.set("content", content);
    out.set("isError", r.isError);
    return out;
}

std::string answer(const json::Value& id, const char* member, json::Value v) {
    json::Value out = json::Value::object();
    out.set("jsonrpc", "2.0");
    out.set("id", id);
    out.set(member, std::move(v));
    return json::dump(out);
}

std::string errorOf(const json::Value& id, int code, const std::string& message) {
    json::Value e = json::Value::object();
    e.set("code", code);
    e.set("message", message);
    return answer(id, "error", e);
}

}  // namespace

namespace {

struct KeyName {
    const char* name;
    int vk;
    bool extended;
};
constexpr KeyName kKeyNames[] = {
    {"enter", 0x0D, false},    {"return", 0x0D, false},  {"esc", 0x1B, false},      {"escape", 0x1B, false},
    {"tab", 0x09, false},      {"space", 0x20, false},   {"backspace", 0x08, false}, {"delete", 0x2E, true},
    {"del", 0x2E, true},       {"insert", 0x2D, true},   {"ins", 0x2D, true},        {"home", 0x24, true},
    {"end", 0x23, true},       {"pageup", 0x21, true},   {"pgup", 0x21, true},       {"pagedown", 0x22, true},
    {"pgdn", 0x22, true},      {"up", 0x26, true},       {"down", 0x28, true},       {"left", 0x25, true},
    {"right", 0x27, true},     {"pause", 0x13, false},   {"scrolllock", 0x91, false}, {"printscreen", 0x2C, true},
    {"minus", 0xBD, false},    {"equals", 0xBB, false},  {"comma", 0xBC, false},     {"period", 0xBE, false},
    {"slash", 0xBF, false},    {"backslash", 0xDC, false}, {"semicolon", 0xBA, false}, {"quote", 0xDE, false},
    {"bracketleft", 0xDB, false}, {"bracketright", 0xDD, false}, {"grave", 0xC0, false},
};

bool keyOf(const std::string& word, KeyCombo& k) {
    if (word.size() == 1 && std::isalnum(static_cast<unsigned char>(word[0]))) {  // a-z, 0-9
        k.vk = std::toupper(static_cast<unsigned char>(word[0]));
        return true;
    }
    if (word.size() >= 2 && word.size() <= 3 && word[0] == 'f') {  // f1-f24
        int n = 0;
        for (size_t i = 1; i < word.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(word[i]))) return false;
            n = n * 10 + (word[i] - '0');
        }
        if (n < 1 || n > 24) return false;
        k.vk = 0x70 + n - 1;
        return true;
    }
    if (word.size() == 7 && word.rfind("numpad", 0) == 0 && std::isdigit(static_cast<unsigned char>(word[6]))) {
        k.vk = 0x60 + (word[6] - '0');
        return true;
    }
    for (const KeyName& n : kKeyNames)
        if (word == n.name) {
            k.vk = n.vk;
            k.extended = n.extended;
            return true;
        }
    return false;
}

}  // namespace

bool keyCombo(const std::string& text, KeyCombo& out, std::string& why) {
    const char* kHow = "keys: one key, ctrl+, shift+ or alt+ before it (alt+e, ctrl+shift+v, enter, f12, up)";
    KeyCombo k;
    bool key = false;
    size_t at = 0;
    const std::string t = lower(trim(text));
    if (t.empty()) {
        why = kHow;
        return false;
    }
    while (at <= t.size()) {
        const size_t plus = t.find('+', at);
        const std::string word = trim(t.substr(at, plus == std::string::npos ? std::string::npos : plus - at));
        if (word == "ctrl" || word == "control") {
            k.ctrl = true;
        } else if (word == "shift") {
            k.shift = true;
        } else if (word == "alt") {
            k.alt = true;
        } else if (word.empty() || key) {
            why = kHow;
            return false;
        } else if (!keyOf(word, k)) {
            why = "keys: no key named \"" + word + "\" - a-z, 0-9, f1-f24, enter, esc, tab, space, backspace, "
                  "delete, insert, home, end, pageup, pagedown, up, down, left, right, numpad0-9";
            return false;
        } else {
            key = true;
        }
        if (plus == std::string::npos) break;
        at = plus + 1;
    }
    if (!key) {
        why = kHow;
        return false;
    }
    out = k;
    return true;
}

std::string tailLines(const std::string& text, size_t n, const std::string& match) {
    const std::string want = lower(match);
    std::vector<std::string> kept;
    size_t at = 0;
    while (at <= text.size() && n > 0) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && (want.empty() || lower(line).find(want) != std::string::npos)) kept.push_back(line);
        at = end + 1;
    }
    const size_t from = kept.size() > n ? kept.size() - n : 0;
    std::string out;
    for (size_t i = from; i < kept.size(); ++i) out += (out.empty() ? "" : "\n") + kept[i];
    return out;
}

uint32_t keyMessageBits(int scan, bool extended, bool alt, bool up) {
    uint32_t bits = 1u | (uint32_t(scan & 0xFF) << 16) | (extended ? 1u << 24 : 0u) | (alt ? 1u << 29 : 0u);
    return up ? bits | 1u << 30 | 1u << 31 : bits;
}

bool clickPlace(double x, double y, int width, int clientW, int clientH, int& outX, int& outY) {
    if (width <= 0 || clientW <= 0 || clientH <= 0 || !(x >= 0) || !(y >= 0)) return false;
    const double f = double(clientW) / double(width);  // the screenshot keeps the window's proportions
    const long px = std::lround(x * f), py = std::lround(y * f);
    if (px >= clientW || py >= clientH) return false;
    outX = int(px);
    outY = int(py);
    return true;
}

std::string HttpRequest::header(const std::string& lowerName) const {
    for (const auto& h : headers)
        if (h.first == lowerName) return h.second;
    return "";
}

int parseHttp(const std::string& data, HttpRequest& out) {
    const size_t end = data.find("\r\n\r\n");
    if (end == std::string::npos) return data.size() > kMostRequest ? -1 : 0;
    const size_t lineEnd = data.find("\r\n");
    const std::string line = data.substr(0, lineEnd);
    const size_t a = line.find(' '), b = a == std::string::npos ? a : line.find(' ', a + 1);
    if (a == std::string::npos || b == std::string::npos || a == 0 || b == a + 1) return -1;
    HttpRequest r;
    r.method = line.substr(0, a);
    r.path = line.substr(a + 1, b - a - 1);
    size_t at = lineEnd + 2;
    while (at < end) {
        const size_t next = data.find("\r\n", at);
        const std::string h = data.substr(at, (next == std::string::npos ? end : next) - at);
        const size_t colon = h.find(':');
        if (colon != std::string::npos)
            r.headers.emplace_back(lower(trim(h.substr(0, colon))), trim(h.substr(colon + 1)));
        if (next == std::string::npos || next >= end) break;
        at = next + 2;
    }
    size_t length = 0;
    const std::string cl = r.header("content-length");
    if (!cl.empty()) {
        for (char c : cl)
            if (!std::isdigit(static_cast<unsigned char>(c))) return -1;
        if (cl.size() > 8) return -1;
        length = size_t(std::strtoul(cl.c_str(), nullptr, 10));
    }
    if (end + 4 + length > kMostRequest) return -1;
    if (data.size() < end + 4 + length) return 0;
    r.body = data.substr(end + 4, length);
    out = std::move(r);
    return int(end + 4 + length);
}

bool allowed(const HttpRequest& r, int, std::string& why) {
    if (!r.header("origin").empty()) {
        why = "a web page's request (Origin " + r.header("origin") + ") - not taken";
        return false;
    }
    std::string host = lower(r.header("host"));
    if (host.empty()) {
        why = "no Host";
        return false;
    }
    if (host[0] == '[') {  // [::1]:port
        const size_t close = host.find(']');
        host = close == std::string::npos ? host : host.substr(0, close + 1);
    } else if (const size_t colon = host.find(':'); colon != std::string::npos) {
        host = host.substr(0, colon);
    }
    if (host != "127.0.0.1" && host != "localhost" && host != "[::1]") {
        why = "Host " + r.header("host") + " - only 127.0.0.1 or localhost";
        return false;
    }
    return true;
}

std::string httpResponse(int status, const std::string& contentType, const std::string& body) {
    std::string out = "HTTP/1.1 " + std::to_string(status) + " " + statusText(status) + "\r\n";
    if (!contentType.empty()) out += "Content-Type: " + contentType + "\r\n";
    out += "Content-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
    return out + body;
}

const std::vector<Tool>& tools() { return table(); }

std::string checkArgs(const std::string& tool, const json::Value& args) {
    if (args.type() != json::Value::Object) return "the arguments must be an object";
    if (tool == "screenshot") {
        if (args.has("width") && !wholeIn(args["width"], 320, 1920)) return "width: a whole number, 320-1920";
    } else if (tool == "chat") {
        const json::Value& l = args["line"];
        if (l.type() != json::Value::String || l.asString().empty() || l.asString().size() > 200 ||
            !oneLine(l.asString()))
            return "line: one line of 1-200 characters";
    } else if (tool == "walk") {
        if (!wholeIn(args["x"], 0, 1023) || !wholeIn(args["y"], 0, 1023)) return "x and y: whole numbers, 0-1023";
    } else if (tool == "view") {
        if (!args.has("mode") && !args.has("yaw") && !args.has("pitch")) return "mode, yaw or pitch - at least one";
        if (args.has("mode")) {
            const std::string m = args["mode"].asString();
            if (m != "off" && m != "behind" && m != "first") return "mode: off, behind or first";
        }
        if (args.has("yaw") && (args["yaw"].type() != json::Value::Number || !std::isfinite(args["yaw"].asNumber())))
            return "yaw: a number of degrees";
        if (args.has("pitch") && (args["pitch"].type() != json::Value::Number || !(args["pitch"].asNumber() >= -89.0) ||
                                  !(args["pitch"].asNumber() <= 89.0)))
            return "pitch: degrees, -89..89";
    } else if (tool == "pause" || tool == "photo") {
        if (args["on"].type() != json::Value::Bool) return "on: true or false";
    } else if (tool == "camera") {
        if (!args.has("free") && !args.has("move")) return "free or move - at least one";
        if (args.has("free") && args["free"].type() != json::Value::Bool) return "free: true or false";
        if (args.has("move")) {
            const json::Value& m = args["move"];
            if (m.type() != json::Value::Object || m.members().empty()) return "move: forward, right or up (cells)";
            for (const auto& [k, n] : m.members())
                if ((k != "forward" && k != "right" && k != "up") || n.type() != json::Value::Number ||
                    !(std::fabs(n.asNumber()) <= 200.0))
                    return "move: forward, right, up - numbers of cells, 200 at most";
        }
    } else if (tool == "flight") {
        const std::string a = args["action"].asString();
        if (a != "point" && a != "play" && a != "stop" && a != "clear") return "action: point, play, stop or clear";
    } else if (tool == "timelapse") {
        const std::string a = args["action"].asString();
        if (a != "run" && a != "hold" && a != "off") return "action: run, hold or off";
    } else if (tool == "key") {  // 3.0.22-beta.35
        KeyCombo k;
        std::string why;
        if (args["keys"].type() != json::Value::String) return "keys: a key's words (alt+e, enter, f12...)";
        if (!keyCombo(args["keys"].asString(), k, why)) return why;
    } else if (tool == "click" || tool == "hover") {
        if (!wholeIn(args["width"], 160, 7680)) return "width: the screenshot's width the place is in, 160-7680";
        if (tool == "hover" && args.has("seconds") && !wholeIn(args["seconds"], 1, 30)) return "seconds: 1-30";
        const json::Value &x = args["x"], &y = args["y"];
        if (x.type() != json::Value::Number || y.type() != json::Value::Number || !(x.asNumber() >= 0) ||
            !(y.asNumber() >= 0) || !(x.asNumber() < args["width"].asNumber()) || !std::isfinite(y.asNumber()))
            return "x and y: a place of the screenshot, 0 up, x under its width";
        if (args.has("button") && args["button"].asString() != "left" && args["button"].asString() != "right")
            return "button: left or right";
        if (args.has("double") && args["double"].type() != json::Value::Bool) return "double: true or false";
    } else if (tool == "log") {  // 3.0.22-beta.41
        if (args.has("lines") && !wholeIn(args["lines"], 1, 300)) return "lines: 1-300";
        const json::Value& m = args["match"];
        if (args.has("match") && (m.type() != json::Value::String || m.asString().size() > 100 || !oneLine(m.asString())))
            return "match: one line of words, 100 at most";
    } else if (tool == "setting") {
        if (!keyName(args["key"].asString())) return "key: section.name in small letters";
        const json::Value& v = args["value"];
        if (v.type() != json::Value::String || v.asString().size() > 200 || !oneLine(v.asString()))
            return "value: one line of text, 200 characters at most";
    }
    return "";
}

Content Content::textOf(std::string t) {
    Content c;
    c.type = "text";
    c.text = std::move(t);
    return c;
}

Content Content::imageOf(std::string bytes, std::string mime) {
    Content c;
    c.type = "image";
    c.bytes = std::move(bytes);
    c.mime = std::move(mime);
    return c;
}

std::string handle(const std::string& body, const CallFn& call, const std::string& version) {
    json::Value msg;
    std::string error;
    if (!json::parse(body, msg, error)) return errorOf(json::Value(), -32700, "parse error: " + error);
    if (msg.type() != json::Value::Object)
        return errorOf(json::Value(), -32600, "one JSON-RPC message a request (no batches)");
    const bool hasId = msg.has("id");
    if (msg["method"].type() != json::Value::String) return hasId ? errorOf(msg["id"], -32600, "no method") : "";
    if (!hasId) return "";  // a notification (initialized, cancelled): nothing to answer
    const json::Value id = msg["id"];
    const std::string method = msg["method"].asString();
    if (method == "initialize") {
        const std::string asked = msg["params"]["protocolVersion"].asString();
        const bool known = asked == "2025-06-18" || asked == "2025-03-26" || asked == "2024-11-05";
        json::Value tools = json::Value::object();
        tools.set("listChanged", false);
        json::Value caps = json::Value::object();
        caps.set("tools", tools);
        json::Value info = json::Value::object();
        info.set("name", "uaro-graphics");
        info.set("title", "uaRO Graphics");
        info.set("version", version);
        json::Value r = json::Value::object();
        r.set("protocolVersion", known ? asked : std::string(kLatestVersion));
        r.set("capabilities", caps);
        r.set("serverInfo", info);
        r.set("instructions",
              "The uaRO client (Ragnarok Online with uaRO Graphics) of the player who turned this on. Change one "
              "thing, then look with `screenshot`. Cells are the map's grid (`state` gives the character's); @warp "
              "works for a GM account only. Pause shows the old client; the photo mode hides the interface.");
        return answer(id, "result", r);
    }
    if (method == "ping") return answer(id, "result", json::Value::object());
    if (method == "tools/list") {
        json::Value list = json::Value::array();
        for (const Tool& t : table()) {
            json::Value schema;
            std::string bad;
            json::parse(t.inputSchema, schema, bad);
            json::Value item = json::Value::object();
            item.set("name", t.name);
            item.set("description", t.description);
            item.set("inputSchema", schema);
            list.push(item);
        }
        json::Value r = json::Value::object();
        r.set("tools", list);
        return answer(id, "result", r);
    }
    if (method == "tools/call") {
        const json::Value& p = msg["params"];
        const std::string name = p["name"].asString();
        if (!find(name)) return errorOf(id, -32602, "no such tool: " + name);
        const json::Value args = p.has("arguments") ? p["arguments"] : json::Value::object();
        const std::string bad = checkArgs(name, args);
        CallResult r;
        if (!bad.empty()) {
            r.isError = true;
            r.content.push_back(Content::textOf(bad));
        } else {
            r = call(name, args);
        }
        return answer(id, "result", resultOf(r));
    }
    return errorOf(id, -32601, "no such method: " + method);
}

}  // namespace uagfx::mcp
