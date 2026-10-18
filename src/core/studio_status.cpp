#include "core/studio_status.hpp"

#include <string>

namespace uagfx::studio {

json::Value statusJson(const pack::Status& s) {
    // the status file's own lines (pack::statusText), each a member
    json::Value v = json::Value::object();
    const std::string text = pack::statusText(s);
    for (size_t at = 0; at < text.size();) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t eq = line.find('=');
        if (eq == std::string::npos || line.compare(0, eq, "thread") == 0) continue;
        v.set(line.substr(0, eq), line.substr(eq + 1));
    }
    json::Value threads = json::Value::array();
    for (const std::string& t : s.threads) threads.push(t);
    v.set("thread", threads);
    return v;
}

bool statusOfJson(const json::Value& v, pack::Status& out) {
    std::string text;
    for (const auto& m : v.members()) {
        if (m.first == "thread") {
            for (const json::Value& t : m.second.items()) text += "thread=" + t.asString() + "\n";
            continue;
        }
        text += m.first + "=" + m.second.asString() + "\n";
    }
    return pack::parseStatus(text, out);
}

}  // namespace uagfx::studio
