#include "core/check_probe.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace uagfx::checkprobe {
namespace {

bool startsWith(const std::string& s, const char* head) { return s.rfind(head, 0) == 0; }

int intOf(const std::string& s, int fallback) {
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    return end && end != s.c_str() ? int(v) : fallback;
}

double round2(double v) { return std::round(v * 100.0) / 100.0; }

const char* phaseName(Phase p) {
    switch (p) {
        case Phase::Begin: return "begin";
        case Phase::Before: return "frame before";
        case Phase::Applied: return "written";
        case Phase::Settling: return "settling";
        case Phase::After: return "frame after";
        case Phase::Back: return "written back";
        case Phase::Ended: return "ended";
    }
    return "";
}

}  // namespace

std::string skipReason(const std::string& k) {
    if (startsWith(k, "mcp.")) return "the MCP's own line";
    if (startsWith(k, "connection.")) return "the way to the server";
    if (k == "renderer.api") return "a stage of its own (Direct3D 11)";
    if (startsWith(k, "launcher.")) return "the launcher";
    if (startsWith(k, "report.")) return "the reports sent to the developers";
    if (k == "stats.send") return "the statistics sent to the site";
    if (k == "debug.log") return "the log the check reads";
    return "";
}

std::string testValue(const settings::IniSetting& s, const std::string& now) {
    using settings::Control;
    switch (s.control) {
        case Control::Check:
            if (s.tokens.size() < 2) return "";
            return now == s.tokens[1] ? s.tokens[0] : s.tokens[1];
        case Control::Choice: {
            if (s.tokens.empty()) return "";
            for (size_t i = 0; i < s.tokens.size(); ++i)
                if (now == s.tokens[i]) return s.tokens[(i + 1) % s.tokens.size()];
            return s.tokens[0];
        }
        case Control::Slider: {
            const int step = std::max(1, s.step), v = std::clamp(intOf(now, s.min), s.min, s.max);
            const int quarter = std::max(step, (s.max - s.min) / 4 / step * step);
            return std::to_string(v + quarter <= s.max ? v + quarter : std::max(s.min, v - quarter));
        }
        case Control::Number: {
            const int step = std::max(1, s.step), v = std::clamp(intOf(now, s.min), s.min, s.max);
            return std::to_string(v + step <= s.max ? v + step : std::max(s.min, v - step));
        }
        case Control::Line: return "";
    }
    return "";
}

FrameDiff diff(const std::vector<uint32_t>& a, int aw, int ah, const std::vector<uint32_t>& b, int bw, int bh) {
    FrameDiff d;
    const size_t n = size_t(std::max(0, aw)) * size_t(std::max(0, ah));
    if (aw != bw || ah != bh || n == 0 || a.size() < n || b.size() < n) {
        d.sizeChanged = true;
        return d;
    }
    uint64_t moved = 0, sum = 0;
    for (size_t i = 0; i < n; ++i) {
        const uint32_t x = a[i], y = b[i];
        int most = 0;
        for (int shift = 0; shift <= 16; shift += 8) {
            const int c = std::abs(int((x >> shift) & 0xFF) - int((y >> shift) & 0xFF));
            sum += uint64_t(c);
            most = std::max(most, c);
        }
        if (most > 12) ++moved;
    }
    d.changed = 100.0 * double(moved) / double(n);
    d.mean = double(sum) / (3.0 * double(n));
    return d;
}

json::Value passesDelta(const std::map<std::string, double>& before, const std::map<std::string, double>& after) {
    json::Value added = json::Value::object(), gone = json::Value::array(), more = json::Value::object();
    bool any = false;
    for (const auto& [name, ms] : after) {
        const auto was = before.find(name);
        if (was == before.end()) {
            added.set(name, round2(ms));
            any = true;
        } else if (ms - was->second >= 0.15) {
            more.set(name, round2(ms - was->second));
            any = true;
        }
    }
    for (const auto& [name, ms] : before)
        if (!after.count(name)) {
            gone.push(name);
            any = true;
        }
    json::Value out = json::Value::object();
    if (!any) return out;
    if (!added.members().empty()) out.set("added", added);
    if (!gone.items().empty()) out.set("gone", gone);
    if (!more.members().empty()) out.set("more", more);
    return out;
}

int printsMoved(const std::map<std::string, uint64_t>& before, const std::map<std::string, uint64_t>& again,
                const std::map<std::string, uint64_t>& after) {
    int n = 0;
    for (const auto& [name, print] : after) {
        const auto was = before.find(name);
        if (was == before.end()) {  // came
            ++n;
            continue;
        }
        const auto second = again.find(name);
        const bool own = second == again.end() || second->second != was->second;  // moves by itself
        if (!own && print != was->second) ++n;
    }
    for (const auto& [name, print] : before)
        if (!after.count(name)) ++n;  // went
    return n;
}

int errorLines(const std::string& text) {
    int n = 0;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        // "hh:mm:ss.mmm ERROR ..." - the level at 13
        if (end - at > 18 && text[at + 2] == ':' && text[at + 8] == '.' && text.compare(at + 13, 5, "ERROR") == 0) ++n;
        at = end + 1;
    }
    return n;
}

json::Value rowJson(const Row& r) {
    json::Value o = json::Value::object();
    o.set("check", r.check);
    o.set("key", r.key);
    o.set("from", r.from);
    o.set("to", r.to);
    if (!r.skip.empty()) {
        o.set("skip", r.skip);
        return o;
    }
    o.set("reached", r.reached);
    o.set("restored", r.restored);
    if (r.reachMs > 0) o.set("reachMs", r.reachMs);
    if (r.loadMs > 0) o.set("loadMs", r.loadMs);
    if (r.framed) {
        json::Value f = json::Value::object();
        if (r.frame.sizeChanged) {
            f.set("sizeChanged", true);
        } else {
            f.set("changed", round2(r.frame.changed));
            f.set("mean", round2(r.frame.mean));
        }
        o.set("frame", f);
    }
    if (!r.passes.members().empty()) o.set("passes", r.passes);
    if (r.uiChanged > 0) o.set("ui", r.uiChanged);
    if (r.logErrors > 0) o.set("logErrors", r.logErrors);
    if (!r.error.empty()) o.set("error", r.error);
    return o;
}

Do step(Job& j, const Seen& s, const Timing& t) {
    if (j.at >= j.items.size()) {
        if (j.told) return Do::Nothing;
        j.told = true;
        j.phase = Phase::Ended;
        return Do::Finish;
    }
    if (s.minimised) {  // no frames come: the clocks held until the window is back
        if (!j.pausedAt) j.pausedAt = s.ms;
        return Do::Nothing;
    }
    if (j.pausedAt) {
        const uint64_t away = s.ms > j.pausedAt ? s.ms - j.pausedAt : 0;
        j.since += away;
        if (j.loadSince) j.loadSince += away;
        j.pausedAt = 0;
    }
    const Item& it = j.items[j.at];
    const auto record = [&j] {
        ++j.at;
        j.phase = Phase::Begin;
        return Do::Record;
    };
    const uint64_t in = s.ms > j.since ? s.ms - j.since : 0;
    switch (j.phase) {
        case Phase::Begin:
            j.row = Row{};
            j.row.key = it.key, j.row.from = it.from, j.row.to = it.to, j.row.skip = it.skip;
            if (j.row.skip.empty() && it.to.empty()) j.row.skip = "a line of text: no value to try";
            if (!j.row.skip.empty()) return record();
            j.phase = Phase::Before, j.since = s.ms;
            j.loadingAtStart = s.loading;  // the background's (AI textures in a town): not waited for
            return Do::ShotBefore;
        case Phase::Before:
            if (s.shotReady) {
                j.phase = Phase::Applied, j.since = s.ms;
                return Do::Write;
            }
            if (in > uint64_t(t.shotMs)) {
                j.row.error = "no frame";
                return record();
            }
            return Do::Nothing;
        case Phase::Applied:
            if (s.value == it.to) {
                j.row.reached = true;
                j.row.reachMs = int(in);
                j.phase = Phase::Settling, j.since = s.ms, j.serial = s.gpuSerial, j.loadSince = 0;
                return Do::Nothing;
            }
            if (in > uint64_t(t.reachMs)) {
                j.row.error = "not taken";
                j.phase = Phase::Back, j.since = s.ms;
                return Do::WriteBack;
            }
            return Do::Nothing;
        case Phase::Settling: {
            if (s.loading && !j.loadingAtStart) {  // a loading the change began: waited out
                if (!j.loadSince) j.loadSince = s.ms;
                if (s.ms - j.loadSince <= uint64_t(t.loadingMs)) return Do::Nothing;
                j.row.error = "still loading";
            } else if (j.loadSince) {  // loaded: settle again, measured anew
                j.row.loadMs = int(s.ms - j.loadSince);
                j.loadSince = 0;
                j.since = s.ms, j.serial = s.gpuSerial;
                return Do::Nothing;
            } else {
                const bool measured = s.gpuSerial != j.serial;
                if (in < uint64_t(t.settleMs) || (!measured && in < uint64_t(3 * t.settleMs))) return Do::Nothing;
            }
            j.phase = Phase::After, j.since = s.ms;
            return Do::ShotAfter;
        }
        case Phase::After:
            if (s.shotReady || in > uint64_t(t.shotMs)) {
                if (!s.shotReady) j.row.error = "no frame";
                j.phase = Phase::Back, j.since = s.ms;
                return Do::WriteBack;
            }
            return Do::Nothing;
        case Phase::Back:
            if (s.value == it.from) {
                j.row.restored = true;
                return record();
            }
            if (in > uint64_t(t.reachMs)) {
                if (j.row.error.empty()) j.row.error = "not written back";
                return record();
            }
            return Do::Nothing;
        case Phase::Ended: return Do::Nothing;
    }
    return Do::Nothing;
}

json::Value status(const Job& j) {
    json::Value o = json::Value::object();
    o.set("at", int64_t(j.at));
    o.set("of", int64_t(j.items.size()));
    if (j.at < j.items.size()) o.set("key", j.items[j.at].key);
    o.set("phase", phaseName(j.phase));
    if (j.pausedAt) o.set("paused", "the game's window is minimised");
    return o;
}

json::Value restartPlan(const std::vector<Item>& items) {
    json::Value list = json::Value::array();
    for (const Item& it : items) {
        json::Value o = json::Value::object();
        o.set("key", it.key);
        o.set("from", it.from);
        o.set("to", it.to);
        if (!it.skip.empty()) o.set("skip", it.skip);
        list.push(o);
    }
    json::Value plan = json::Value::object();
    plan.set("items", list);
    return plan;
}

std::vector<Row> verifyPlan(const json::Value& plan, bool back,
                            const std::function<std::string(const std::string&)>& valueOf) {
    std::vector<Row> rows;
    for (const json::Value& o : plan["items"].items()) {
        Row r;
        r.check = "restart";
        r.key = o["key"].asString(), r.from = o["from"].asString(), r.to = o["to"].asString();
        r.skip = o["skip"].asString();
        if (r.skip.empty()) {
            const std::string now = valueOf(r.key);
            if (back) {
                r.reached = true;  // the first restart's verdict is its own row
                r.restored = now == r.from;
                if (!r.restored) r.error = "not back after the restart: " + now;
            } else {
                r.reached = now == r.to;
                if (!r.reached) r.error = "not taken after the restart: " + now;
            }
        }
        rows.push_back(std::move(r));
    }
    return rows;
}

}  // namespace uagfx::checkprobe
