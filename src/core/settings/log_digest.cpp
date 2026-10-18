#include "core/settings/log_digest.hpp"

#include <cctype>

namespace uagfx::settings {
namespace {

bool stamped(const std::string& l) {
    if (l.size() < 19) return false;
    for (size_t i : {0u, 1u, 3u, 4u, 6u, 7u, 9u, 10u, 11u})
        if (!std::isdigit(static_cast<unsigned char>(l[i]))) return false;
    return l[2] == ':' && l[5] == ':' && l[8] == '.' && l[12] == ' ';
}

}  // namespace

LogDigest digestLog(const std::string& text, size_t keep) {
    LogDigest d;
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
        pos = nl + 1;
    }
    d.found = !lines.empty();
    for (const std::string& l : lines) {
        if (!stamped(l)) continue;
        const std::string level = l.substr(13, 5);
        const std::string msg = l.substr(19);
        if (msg.rfind("uaRO Graphics ", 0) == 0) {  // a start: the lines after it are that run's
            const size_t end = msg.find(' ', 14);
            d.version = msg.substr(14, end == std::string::npos ? std::string::npos : end - 14);
            d.crisp.clear();
            d.crispScale.clear();
        } else if (msg.rfind("renderer: ", 0) == 0) {
            d.renderer = msg.substr(10);
        } else if (msg.rfind("map: ", 0) == 0) {  // 2.2.5
            d.map = msg.substr(5);
        } else if (msg.rfind("crisp text: ", 0) == 0) {
            const std::string what = msg.substr(12);
            if (what == "on" || what.rfind("off", 0) == 0)
                d.crisp = what;
            else if (what.rfind("k ", 0) == 0)
                d.crispScale = what;
        }
        const size_t frames = msg.find(" frames in ");
        const size_t open = msg.find(" (", frames == std::string::npos ? 0 : frames);
        const size_t close = msg.find(" fps)", open == std::string::npos ? 0 : open);
        if (frames != std::string::npos && open != std::string::npos && close != std::string::npos)
            d.fps = msg.substr(open + 2, close + 4 - (open + 2));
        if (level.rfind("ERROR", 0) == 0) {
            ++d.errorCount;
            d.errors.push_back(msg);
            if (d.errors.size() > 10) d.errors.erase(d.errors.begin());
        }
    }
    const size_t from = lines.size() > keep ? lines.size() - keep : 0;
    d.tail.assign(lines.begin() + long(from), lines.end());
    return d;
}

}  // namespace uagfx::settings
