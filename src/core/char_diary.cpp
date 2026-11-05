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
        if (f.size() >= 12) {  // 3.0.11-beta.274: the last session
            Entry& x = d[f[0]];
            x.sessionSeconds = std::atoll(f[7].c_str());
            x.baseGain = std::atoi(f[8].c_str());
            x.jobGain = std::atoi(f[9].c_str());
            x.zenyGain = std::atoll(f[10].c_str());
            x.sessionEnd = std::atoll(f[11].c_str());
        }
        Entry& e = d[f[0]];
        e.lastSeen = std::atoll(f[1].c_str());
        e.map = f[2];
        e.level = std::atoi(f[3].c_str());
        e.jobLevel = std::atoi(f[4].c_str());
        e.job = std::atoi(f[5].c_str());
        e.played = std::atoll(f[6].c_str());
    }
    return d;
}

std::string text(const Diary& d) {
    std::string out;
    for (const auto& [name, e] : d)
        if (!name.empty() && name.find('\t') == std::string::npos && name.find('\n') == std::string::npos)
            out += name + '\t' + std::to_string(e.lastSeen) + '\t' + e.map + '\t' + std::to_string(e.level) + '\t' +
                   std::to_string(e.jobLevel) + '\t' + std::to_string(e.job) + '\t' + std::to_string(e.played) + '\t' +
                   std::to_string(e.sessionSeconds) + '\t' + std::to_string(e.baseGain) + '\t' +
                   std::to_string(e.jobGain) + '\t' + std::to_string(e.zenyGain) + '\t' + std::to_string(e.sessionEnd) +
                   '\n';
    return out;
}

int gained(int levelFrom, long long expFrom, long long nextFrom, int levelTo, long long expTo, long long nextTo) {
    const auto at = [](int level, long long exp, long long next) {
        return double(level) * 100.0 + (next > 0 ? double(exp) * 100.0 / double(next) : 0.0);
    };
    return int(at(levelTo, expTo, nextTo) - at(levelFrom, expFrom, nextFrom) + (levelTo >= levelFrom ? 0.5 : -0.5));
}

std::string summary(const std::string& name, const Entry& e, const SummaryTexts& t, char decimal) {
    (void)decimal;
    if (e.sessionSeconds < 60) return "";
    std::string out = t.head;
    const size_t at = out.find("{1}");
    if (at != std::string::npos) out.replace(at, 3, name);
    const long long h = e.sessionSeconds / 3600, m = (e.sessionSeconds % 3600) / 60;
    std::string time = h > 0 ? t.hours : t.minutes;
    const auto put = [](std::string& f, const char* key, long long v) {
        const size_t p = f.find(key);
        if (p != std::string::npos) f.replace(p, 3, std::to_string(v));
    };
    if (h > 0) {
        put(time, "{1}", h);
        put(time, "{2}", m);
    } else {
        put(time, "{1}", m);
    }
    out += " " + time;
    std::string base = t.base, job = t.job;
    put(base, "{1}", e.baseGain);
    put(job, "{1}", e.jobGain);
    if (e.baseGain > 0) out += " \xC2\xB7 " + base;
    if (e.jobGain > 0) out += " \xC2\xB7 " + job;
    if (e.zenyGain != 0) {
        std::string digits = std::to_string(e.zenyGain < 0 ? -e.zenyGain : e.zenyGain), grouped;
        for (size_t i = 0; i < digits.size(); ++i) {
            if (i && (digits.size() - i) % 3 == 0) grouped += ' ';
            grouped += digits[i];
        }
        std::string zeny = t.zeny;
        const size_t p = zeny.find("{1}");
        if (p != std::string::npos) zeny.replace(p, 3, (e.zenyGain < 0 ? "\xE2\x88\x92" : "+") + grouped);
        out += " \xC2\xB7 " + zeny;
    }
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
