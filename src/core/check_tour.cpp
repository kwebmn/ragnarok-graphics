#include "core/check_tour.hpp"

#include <algorithm>

namespace uagfx::checktour {
namespace {

const char* phaseName(Phase p) {
    switch (p) {
        case Phase::Begin: return "begin";
        case Phase::Opening: return "opening";
        case Phase::Checked: return "checked";
        case Phase::Padded: return "pad stepped";
        case Phase::Closing: return "closing";
        case Phase::Ended: return "ended";
    }
    return "";
}

const Stop kHere{"here", ""};

void build(Job& j) {
    if (j.built) return;
    j.built = true;
    if (j.here) {
        j.list.push_back(&kHere);
        return;
    }
    for (const Stop& s : stops())
        if (j.only.empty() || std::find(j.only.begin(), j.only.end(), s.name) != j.only.end()) j.list.push_back(&s);
}

bool anyOpen(const std::vector<std::string>& opened, const std::vector<std::string>& docs) {
    for (const std::string& d : opened)
        if (std::find(docs.begin(), docs.end(), d) != docs.end()) return true;
    return false;
}

}  // namespace

const std::vector<Stop>& stops() {
    static const std::vector<Stop> kStops = {
        {"items", "alt+e"},      {"equip", "alt+q"},     {"stats", "alt+a"},       {"skills", "alt+s"},
        {"basic", "alt+v"},      {"detail", "alt+d"},    {"settings", "alt+o"},    {"cart", "alt+w"},
        {"guild", "alt+g"},      {"party", "alt+p"},     {"friends", "alt+z"},     {"whispers", "alt+h"},
        {"friend setup", "alt+i"}, {"macros", "alt+m"},  {"emotes", "alt+l"},      {"pet", "alt+j"},
        {"homunculus", "alt+r"}, {"menu", "esc"},
    };
    return kStops;
}

std::vector<std::string> appeared(const std::vector<std::string>& before, const std::vector<std::string>& after) {
    std::vector<std::string> out;
    for (const std::string& a : after)
        if (std::find(before.begin(), before.end(), a) == before.end()) out.push_back(a);
    return out;
}

bool inside(int x, int y, int w, int h, int screenW, int screenH) {
    return w > 0 && h > 0 && x >= 0 && y >= 0 && x + w <= screenW && y + h <= screenH;
}

json::Value rowJson(const Row& r) {
    json::Value o = json::Value::object();
    o.set("check", "window");
    o.set("stop", r.stop);
    json::Value opened = json::Value::array();
    for (const std::string& d : r.opened) opened.push(d);
    o.set("opened", opened);
    if (!r.problems.items().empty()) o.set("problems", r.problems);
    if (!r.opened.empty()) {
        o.set("inside", r.inside);
        o.set("pad", std::string(!r.padRing ? "no ring" : r.padMoves ? "moves" : "stays"));
        o.set("closed", r.closed);
    }
    if (!r.error.empty()) o.set("error", r.error);
    return o;
}

Do step(Job& j, const Seen& s, int openMs) {
    build(j);
    if (j.at >= j.list.size()) {
        if (j.told) return Do::Nothing;
        j.told = true;
        j.phase = Phase::Ended;
        return Do::Finish;
    }
    const auto record = [&j] {
        ++j.at;
        j.phase = Phase::Begin;
        return Do::Record;
    };
    const uint64_t in = s.ms > j.since ? s.ms - j.since : 0;
    switch (j.phase) {
        case Phase::Begin:
            j.row = Row{};
            j.row.stop = j.list[j.at]->name;
            j.since = s.ms;
            if (j.here) {
                j.row.opened = s.docs;
                if (j.row.opened.empty()) {
                    j.row.error = "nothing open";
                    return record();
                }
                j.phase = Phase::Checked;
                return Do::Check;
            }
            j.before = s.docs;
            j.phase = Phase::Opening;
            return Do::Keys;
        case Phase::Opening: {
            const std::vector<std::string> fresh = appeared(j.before, s.docs);
            if (!fresh.empty()) {
                j.row.opened = fresh;
                j.phase = Phase::Checked;
                return Do::Check;
            }
            if (in > uint64_t(openMs)) {  // no Esc: with nothing open it would bring the game's menu
                j.row.error = "nothing opened";
                return record();
            }
            return Do::Nothing;
        }
        case Phase::Checked:
            if (s.padFocus >= -1) {
                j.row.padRing = true;
                j.focusBefore = s.padFocus;
                j.phase = Phase::Padded;
                return Do::PadStep;
            }
            if (j.here) return record();
            j.phase = Phase::Closing, j.since = s.ms;
            return Do::Esc;
        case Phase::Padded:
            j.row.padMoves = s.padFocus != j.focusBefore;
            if (j.here) return record();
            j.phase = Phase::Closing, j.since = s.ms;
            return Do::Esc;
        case Phase::Closing:
            if (!anyOpen(j.row.opened, s.docs)) {
                j.row.closed = true;
                return record();
            }
            if (in > uint64_t(openMs)) {
                j.row.error = "Esc did not close it";
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
    o.set("of", int64_t(j.list.size()));
    if (j.at < j.list.size()) o.set("stop", j.list[j.at]->name);
    o.set("phase", phaseName(j.phase));
    return o;
}

}  // namespace uagfx::checktour
