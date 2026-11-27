#include "core/reference_live.hpp"

#include <algorithm>
#include <cctype>

#include "core/item_window.hpp"
#include "core/json.hpp"

namespace uagfx::reflive {
namespace {
std::string codeOf(const std::string& map);
}  // namespace

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
        const std::string safe = codeOf(code);  // nothing to escape
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

namespace {

std::string codeOf(const std::string& map) {  // a map's code: letters, digits, _ and - (anything else left out)
    std::string safe;
    for (char c : map)
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') safe += c;
    return safe;
}

bool readLive(const json::Value& v, long long at, Live& out) {
    if (v["mvp"].type() != json::Value::Array || v["market"].type() != json::Value::Array) return false;
    Live l;
    l.at = at;
    for (const json::Value& t : v["mvp"].items()) {  // [mob, map, killed]
        Timer timer{int(t[size_t(0)].asInt(0)), codeOf(t[size_t(1)].asString()), t[size_t(2)].asInt(0)};
        if (timer.mob > 0 && timer.killed > 0) l.timers.push_back(std::move(timer));
    }
    std::stable_sort(l.timers.begin(), l.timers.end(), [](const Timer& a, const Timer& b) {
        return a.killed > b.killed;
    });
    for (const json::Value& m : v["market"].items()) {  // [item, cheapest, lots, pieces, average, deals]
        Market market{int(m[size_t(0)].asInt(0)), m[size_t(1)].asInt(0), int(m[size_t(2)].asInt(0)),
                      int(m[size_t(3)].asInt(0)), m[size_t(4)].asInt(0), int(m[size_t(5)].asInt(0))};
        if (market.average <= 0) market.deals = 0;
        const bool lots = market.cheapest > 0 && market.lots > 0;
        if (!lots) market.cheapest = 0, market.lots = 0, market.pieces = 0;
        if (market.item > 0 && (lots || market.deals > 0)) l.market[market.item] = market;  // .29: deals alone too
    }
    out = std::move(l);
    return true;
}

}  // namespace

bool fromSite(const std::string& text, long long now, Live& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["v"].asInt(0) != 1) return false;
    return readLive(v, now, out);
}

std::string write(const Live& l) {
    std::string s = "{\"v\":1,\"at\":" + std::to_string(l.at) + ",\"mvp\":[";
    bool first = true;
    for (const Timer& t : l.timers) {
        s += (first ? "[" : ",[") + std::to_string(t.mob) + ",\"" + codeOf(t.map) + "\"," + std::to_string(t.killed) +
             "]";
        first = false;
    }
    s += "],\"market\":[";
    first = true;
    for (const auto& [item, m] : l.market) {
        s += (first ? "[" : ",[") + std::to_string(item) + "," + std::to_string(m.cheapest) + "," +
             std::to_string(m.lots) + "," + std::to_string(m.pieces) + "," + std::to_string(m.average) + "," +
             std::to_string(m.deals) + "]";
        first = false;
    }
    return s + "]}";
}

bool read(const std::string& text, Live& out) {
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error) || v.type() != json::Value::Object || v["v"].asInt(0) != 1) return false;
    return readLive(v, v["at"].asInt(0), out);
}

std::string marketLine(const Market& m, const std::string& lotsForm, const std::string& averageForm) {
    const auto fill = [](std::string form, const std::string& a, const std::string& b) {
        if (const size_t at = form.find("{1}"); at != std::string::npos) form.replace(at, 3, a);
        if (const size_t at = form.find("{2}"); at != std::string::npos) form.replace(at, 3, b);
        return form;
    };
    std::string out;
    if (m.cheapest > 0 && m.lots > 0) out = fill(lotsForm, itemwin::grouped(m.cheapest), std::to_string(m.lots));
    if (m.deals >= kAverageFrom && m.average > 0)
        out += (out.empty() ? "" : " \xC2\xB7 ") +
               fill(averageForm, itemwin::grouped(m.average), std::to_string(m.deals));
    return out;
}

bool fresh(const Live& l, long long now) { return l.at > 0 && now >= l.at - 60 && now - l.at <= kFreshSeconds; }

const Timer* timerOf(const Live& l, int mob, const std::string& map) {
    for (const Timer& t : l.timers)  // the latest first
        if (t.mob == mob && (map.empty() || t.map.empty() || t.map == map)) return &t;
    return nullptr;
}

}  // namespace uagfx::reflive
