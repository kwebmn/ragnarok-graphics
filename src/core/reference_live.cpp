#include "core/reference_live.hpp"

#include <cctype>

#include "core/json.hpp"

namespace uagfx::reflive {

bool fromSite(const std::string& text, long long now, Online& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["byMap"].type() != json::Value::Array)
        return false;
    Online o;
    o.at = now;
    o.online = int(v["online"].asInt(0));
    for (const json::Value& m : v["byMap"].items()) {
        const std::string code = m["code"].asString();
        const int count = int(m["count"].asInt(0));
        if (!code.empty() && count > 0) o.maps[code] += count;
    }
    out = std::move(o);
    return true;
}

std::string write(const Online& o) {
    std::string s = "{\"v\":1,\"at\":" + std::to_string(o.at) + ",\"online\":" + std::to_string(o.online) +
                    ",\"maps\":{";
    bool first = true;
    for (const auto& [code, count] : o.maps) {
        std::string safe;  // a map's code: letters, digits, _ and - (nothing to escape; anything else left out)
        for (char c : code)
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') safe += c;
        if (safe.empty()) continue;
        s += (first ? "\"" : ",\"") + safe + "\":" + std::to_string(count);
        first = false;
    }
    return s + "}}";
}

bool read(const std::string& text, Online& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["v"].asInt(0) != 1) return false;
    Online o;
    o.at = v["at"].asInt(0);
    o.online = int(v["online"].asInt(0));
    for (const auto& m : v["maps"].members()) o.maps[m.first] = int(m.second.asInt(0));
    out = std::move(o);
    return true;
}

bool fresh(const Online& o, long long now) { return o.at > 0 && now >= o.at - 60 && now - o.at <= kFreshSeconds; }

}  // namespace uagfx::reflive
