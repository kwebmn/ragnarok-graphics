#include "core/char_diary.hpp"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace uagfx::chardiary {
namespace {

std::string fill(std::string form, long long v) {
    const size_t at = form.find("{1}");
    if (at != std::string::npos) form.replace(at, 3, std::to_string(v));
    return form;
}

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    size_t from = 0;
    while (true) {
        const size_t tab = line.find('\t', from);
        out.push_back(line.substr(from, tab == std::string::npos ? std::string::npos : tab - from));
        if (tab == std::string::npos) break;
        from = tab + 1;
    }
    return out;
}

}  // namespace

Diary parse(const std::string& text) {
    Diary d;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::vector<std::string> f = fields(line);
        if (f.size() < 7 || f[0].empty()) continue;
        Entry e;
        e.lastSeen = std::atoll(f[1].c_str());
        e.map = f[2];
        e.level = std::atoi(f[3].c_str());
        e.jobLevel = std::atoi(f[4].c_str());
        e.job = std::atoi(f[5].c_str());
        e.played = std::atoll(f[6].c_str());
        d[f[0]] = e;
    }
    return d;
}

std::string text(const Diary& d) {
    std::string out;
    for (const auto& [name, e] : d)
        if (!name.empty() && name.find('\t') == std::string::npos && name.find('\n') == std::string::npos)
            out += name + '\t' + std::to_string(e.lastSeen) + '\t' + e.map + '\t' + std::to_string(e.level) + '\t' +
                   std::to_string(e.jobLevel) + '\t' + std::to_string(e.job) + '\t' + std::to_string(e.played) + '\n';
    return out;
}

std::string ago(long long now, long long then, const AgoTexts& t) {
    if (then <= 0 || now <= 0) return "";
    const long long s = now > then ? now - then : 0;
    if (s < 120) return t.now;
    if (s < 3600) return fill(t.minutes, s / 60);
    if (s < 2 * 86400) return fill(t.hours, s / 3600);
    if (s < 60 * 86400) return fill(t.days, s / 86400);
    return fill(t.months, s / (30 * 86400));
}

}  // namespace uagfx::chardiary
