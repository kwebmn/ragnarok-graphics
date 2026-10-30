#include "core/first_game.hpp"

#include <algorithm>
#include <cstdlib>

#include "core/log_files.hpp"
#include "core/report_upload.hpp"

namespace uagfx::firstgame {

std::string format(const Outcome& o) {
    return "version=" + o.version + "\nclass=" + o.cls + "\nsafe=" + (o.safe ? "1" : "0") + "\nmap=" +
           (o.map ? "1" : "0") + "\nfps=" + std::to_string(o.fps) + "\nlevel=" + std::to_string(o.level) +
           "\nend=" + o.end + "\nminutes=" + std::to_string(o.minutes) + "\n";
}

bool parse(const std::string& text, Outcome& out) {
    Outcome o;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "version") o.version = value;
        else if (key == "class") o.cls = value;
        else if (key == "safe") o.safe = value == "1";
        else if (key == "map") o.map = value == "1";
        else if (key == "fps") o.fps = std::atoi(value.c_str());
        else if (key == "level") o.level = std::atoi(value.c_str());
        else if (key == "end") o.end = value;
        else if (key == "minutes") o.minutes = std::atoi(value.c_str());
    }
    if (o.version.empty()) return false;
    out = o;
    return true;
}

bool complete(const Outcome& o) { return !o.end.empty(); }

std::string endOfLog(const std::string& log) {
    if (log.empty()) return "";
    const std::string reason = report::reasonOfLog(log);
    return reason.empty() ? "fine" : reason;
}

int minutesOfLog(const std::string& log) {
    int first = -1, last = -1;
    for (size_t at = 0; at < log.size();) {
        const int s = logfiles::secondsAt(log, at);
        if (s >= 0) {
            if (first < 0) first = s;
            last = s;
        }
        const size_t end = log.find('\n', at);
        if (end == std::string::npos) break;
        at = end + 1;
    }
    if (first < 0) return -1;
    return (last >= first ? last - first : last + 86400 - first) / 60;
}

namespace {

std::string words(const std::string& s) {  // what the site takes: letters, digits, spaces, dots and dashes
    std::string out;
    for (const char c : s)
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' ||
            c == '-')
            out += c;
    return out.substr(0, 24);
}

std::string number(int v, int most) { return v < 0 ? "null" : std::to_string(std::min(v, most)); }

}  // namespace

std::string json(const Outcome& o) {
    return "{\"class\":\"" + words(o.cls) + "\",\"safe\":" + (o.safe ? "1" : "0") + ",\"map\":" + (o.map ? "1" : "0") +
           ",\"fps\":" + number(o.fps, 999) + ",\"level\":" + number(o.level, 3) + ",\"end\":" +
           (o.end.empty() ? std::string("null") : "\"" + words(o.end) + "\"") + ",\"minutes\":" +
           number(o.minutes, 99999) + "}";
}

}  // namespace uagfx::firstgame
