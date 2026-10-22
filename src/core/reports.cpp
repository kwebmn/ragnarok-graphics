#include "core/reports.hpp"

#include <cstdio>

#include "core/json.hpp"
#include "core/update_check.hpp"

namespace uagfx::reports {

namespace {

Kind kindOf(const std::string& token) {
    if (token == "graphics") return Kind::Graphics;
    if (token == "crash") return Kind::Crash;
    if (token == "slow") return Kind::Slow;
    if (token == "other") return Kind::Other;
    return Kind::None;
}

// A shot's name goes into a path of the site's: only the plain names the site makes
bool plainName(const std::string& name) {
    if (name.empty() || name.size() > 84 || name[0] == '.') return false;
    for (char ch : name) {
        const bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                        ch == '.' || ch == '_' || ch == '-';
        if (!ok) return false;
    }
    return true;
}

bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysIn(int y, int m) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && leap(y) ? 29 : kDays[m - 1];
}

// Minutes added to a civil time, the date carried over (the offsets are under a day)
void addMinutes(Civil& c, int minutes) {
    int total = c.hour * 60 + c.minute + minutes;
    while (total < 0) {
        total += 24 * 60;
        if (--c.day < 1) {
            if (--c.month < 1) {
                c.month = 12;
                --c.year;
            }
            c.day = daysIn(c.year, c.month);
        }
    }
    while (total >= 24 * 60) {
        total -= 24 * 60;
        if (++c.day > daysIn(c.year, c.month)) {
            c.day = 1;
            if (++c.month > 12) {
                c.month = 1;
                ++c.year;
            }
        }
    }
    c.hour = total / 60;
    c.minute = total % 60;
}

}  // namespace

bool parseList(const std::string& text, std::vector<Summary>& out, std::string& error) {
    json::Value v;
    if (!json::parse(text, v, error)) return false;
    if (v.type() != json::Value::Object || v["reports"].type() != json::Value::Array) {
        error = "not the site's list of reports";
        return false;
    }
    std::vector<Summary> list;
    for (const json::Value& r : v["reports"].items()) {
        Summary s;
        s.code = r["code"].asString();
        if (s.code.empty()) continue;
        s.title = r["title"].asString();
        s.reason = r["reason"].asString();
        s.fixedIn = r["fixed_in"].asString();
        s.at = r["at"].asString();
        s.updated = r["updated"].asString();
        s.status = statusOf(r["status"].asString());
        s.kind = kindOf(r["kind"].asString());
        s.automatic = r["auto"].asBool();
        s.unread = int(r["unread"].asInt());
        list.push_back(s);
    }
    out = list;
    return true;
}

bool parseConversation(const std::string& text, Conversation& out, std::string& error) {
    json::Value v;
    if (!json::parse(text, v, error)) return false;
    if (v.type() != json::Value::Object || v["code"].asString().empty() ||
        v["messages"].type() != json::Value::Array) {
        error = "not the site's report";
        return false;
    }
    Conversation c;
    c.code = v["code"].asString();
    c.reason = v["reason"].asString();
    c.fixedIn = v["fixed_in"].asString();
    c.version = v["version"].asString();
    c.at = v["at"].asString();
    c.status = statusOf(v["status"].asString());
    c.kind = kindOf(v["kind"].asString());
    c.automatic = v["auto"].asBool();
    for (const json::Value& m : v["messages"].items()) {
        Message msg;
        msg.fromDev = m["author"].asString() == "dev";
        msg.text = m["text"].asString();
        msg.at = m["at"].asString();
        msg.hasStatus = m["status"].type() == json::Value::String;
        msg.status = statusOf(m["status"].asString());
        for (const json::Value& shot : m["shots"].items())
            if (plainName(shot.asString())) msg.shots.push_back(shot.asString());
        c.messages.push_back(msg);
    }
    out = c;
    return true;
}

const char* kindToken(Kind k) {
    switch (k) {
        case Kind::Graphics: return "graphics";
        case Kind::Crash: return "crash";
        case Kind::Slow: return "slow";
        case Kind::Other: return "other";
        case Kind::None: break;
    }
    return "";
}

const char* statusToken(Status s) {
    switch (s) {
        case Status::Read: return "read";
        case Status::NeedInfo: return "need_info";
        case Status::Fixing: return "fixing";
        case Status::Fixed: return "fixed";
        case Status::Closed: return "closed";
        case Status::New: break;
    }
    return "new";
}

Status statusOf(const std::string& token) {
    if (token == "read") return Status::Read;
    if (token == "need_info") return Status::NeedInfo;
    if (token == "fixing") return Status::Fixing;
    if (token == "fixed") return Status::Fixed;
    if (token == "closed") return Status::Closed;
    return Status::New;
}

int unreadOf(const std::vector<Summary>& list) {
    int n = 0;
    for (const Summary& s : list) n += s.unread > 0 ? s.unread : 0;
    return n;
}

bool canConfirm(const Conversation& c, const std::string& installedVersion) {
    if (c.status != Status::Fixed || c.fixedIn.empty()) return false;
    const std::string here = update::bare(installedVersion);
    return here == update::bare(c.fixedIn) || update::newer(here, c.fixedIn);
}

bool utcOf(const std::string& iso, Civil& out) {
    Civil c;
    char sign = 0;
    int oh = 0, om = 0, used = 0;
    if (std::sscanf(iso.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%n", &c.year, &c.month, &c.day, &c.hour, &c.minute,
                    &c.second, &used) != 6)
        return false;
    if (c.month < 1 || c.month > 12 || c.day < 1 || c.day > daysIn(c.year, c.month) || c.hour > 23 || c.minute > 59)
        return false;
    const std::string rest = iso.substr(size_t(used));
    if (rest == "Z" || rest.empty()) {
        out = c;
        return true;
    }
    if (std::sscanf(rest.c_str(), "%c%2d:%2d", &sign, &oh, &om) != 3 || (sign != '+' && sign != '-')) return false;
    addMinutes(c, (sign == '+' ? -1 : 1) * (oh * 60 + om));
    out = c;
    return true;
}

std::string shown(const Civil& local) {
    char text[32];
    std::snprintf(text, sizeof text, "%02d.%02d.%04d %02d:%02d", local.day, local.month, local.year, local.hour,
                  local.minute);
    return text;
}

}  // namespace uagfx::reports
