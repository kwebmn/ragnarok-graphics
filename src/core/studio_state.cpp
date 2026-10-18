#include "core/studio_state.hpp"

#include <algorithm>
#include <cstdio>

namespace uagfx::studio {

json::Value toJson(const Session& s) {
    json::Value v = json::Value::object();
    v.set("tab", s.tab);
    v.set("nav", s.nav);
    v.set("filter", s.filter);
    v.set("sort", s.sort);
    v.set("sortDown", s.sortDown);
    json::Value selected = json::Value::array();
    for (const std::string& k : s.selected) selected.push(k);
    v.set("selected", selected);
    v.set("shown", s.shown);
    v.set("zoom", s.zoom);
    v.set("view", s.view);
    v.set("x", s.x);
    v.set("y", s.y);
    v.set("w", s.w);
    v.set("h", s.h);
    v.set("paused", s.paused);
    return v;
}

Session sessionOf(const json::Value& v) {
    Session s;
    const auto text = [&v](const char* name, std::string& to) {
        if (v[name].type() == json::Value::String) to = v[name].asString();
    };
    text("tab", s.tab);
    text("nav", s.nav);
    text("filter", s.filter);
    text("sort", s.sort);
    text("shown", s.shown);
    text("view", s.view);
    s.sortDown = v["sortDown"].asBool(s.sortDown);
    s.paused = v["paused"].asBool(s.paused);
    for (const json::Value& k : v["selected"].items())
        if (k.type() == json::Value::String) s.selected.push_back(k.asString());
    s.zoom = int(std::clamp<int64_t>(v["zoom"].asInt(s.zoom), 1, 16));
    s.x = int(v["x"].asInt(s.x));
    s.y = int(v["y"].asInt(s.y));
    s.w = int(v["w"].asInt(s.w));
    s.h = int(v["h"].asInt(s.h));
    return s;
}

int64_t Queue::add(const std::string& op, json::Value args, bool writesPack) {
    Job j;
    j.id = next_++;
    j.op = op;
    j.args = std::move(args);
    j.writesPack = writesPack;
    jobs_.push_back(std::move(j));
    return jobs_.back().id;
}

Job* Queue::find(int64_t id) {
    for (Job& j : jobs_)
        if (j.id == id) return &j;
    return nullptr;
}

Job* Queue::next(bool packFree) {
    for (Job& j : jobs_)
        if (j.state == Job::Queued && (packFree || !j.writesPack)) return &j;
    return nullptr;
}

void Queue::started(int64_t id) {
    if (Job* j = find(id)) {
        j->state = Job::Running;
        ++j->tries;
    }
}

void Queue::done(int64_t id) {
    if (Job* j = find(id)) j->state = Job::Done;
}

void Queue::failed(int64_t id, const std::string& why) {
    if (Job* j = find(id)) {
        j->state = Job::Failed;
        j->why = why;
    }
}

void Queue::engineLost(int64_t id) {
    Job* j = find(id);
    if (!j) return;
    if (j->tries < 2) {
        j->state = Job::Queued;
    } else {
        j->state = Job::Failed;
        j->why = "the engine stopped twice";
    }
}

json::Value Queue::toJson() const {
    json::Value v = json::Value::object();
    v.set("next", next_);
    json::Value list = json::Value::array();
    for (const Job& j : jobs_) {
        if (j.state != Job::Queued && j.state != Job::Running) continue;
        json::Value o = json::Value::object();
        o.set("id", j.id);
        o.set("op", j.op);
        o.set("args", j.args);
        o.set("writesPack", j.writesPack);
        list.push(o);
    }
    v.set("jobs", list);
    return v;
}

Queue Queue::fromJson(const json::Value& v) {
    Queue q;
    for (const json::Value& o : v["jobs"].items()) {
        if (o["op"].asString().empty() || o["id"].asInt() <= 0) continue;
        Job j;
        j.id = o["id"].asInt();
        j.op = o["op"].asString();
        j.args = o["args"];
        j.writesPack = o["writesPack"].asBool();
        q.next_ = std::max(q.next_, j.id + 1);
        q.jobs_.push_back(std::move(j));
    }
    q.next_ = std::max(q.next_, v["next"].asInt(1));
    return q;
}

std::string engineName(const std::string& versionLine, uint64_t size, int64_t time) {
    std::string version = versionLine.substr(0, versionLine.find(' '));
    bool plain = !version.empty() && version.front() != '.' && version.back() != '.';
    for (size_t i = 0; i < version.size() && plain; ++i)
        plain = (version[i] >= '0' && version[i] <= '9') || (version[i] == '.' && version[i + 1] != '.');
    if (!plain) version = "x";
    uint32_t h = 2166136261u;  // FNV-1a over the line, the size and the time
    const auto mix = [&h](const void* p, size_t n) {
        for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const uint8_t*>(p)[i]) * 16777619u;
    };
    mix(versionLine.data(), versionLine.size());
    mix(&size, sizeof size);
    mix(&time, sizeof time);
    char hex[16];
    std::snprintf(hex, sizeof hex, "%08x", h);
    return "engine-" + version + "-" + hex + ".exe";
}

std::vector<std::string> buildsToDrop(const std::vector<std::pair<std::string, int64_t>>& builds,
                                      const std::string& running, size_t keep) {
    std::vector<std::pair<std::string, int64_t>> sorted = builds;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<std::string> out;
    for (size_t i = keep; i < sorted.size(); ++i)
        if (sorted[i].first != running) out.push_back(sorted[i].first);
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace uagfx::studio
