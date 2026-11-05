#include "core/mvp_timers.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace uagfx::mvptimers {

bool Timers::died(int mob, const std::string& map, long long now, int respawnMs) {
    if (mob <= 0 || map.empty() || respawnMs <= 0) return false;
    for (Timer& t : list_)
        if (t.mob == mob && t.map == map) {
            if (now - t.died < 5) return false;  // the same death told twice
            t.died = now;
            t.respawnMs = respawnMs;
            return true;
        }
    list_.push_back(Timer{mob, map, now, respawnMs});
    return true;
}

void Timers::remove(int mob, const std::string& map) {
    list_.erase(std::remove_if(list_.begin(), list_.end(),
                               [&](const Timer& t) { return t.mob == mob && t.map == map; }),
                list_.end());
}

void Timers::prune(long long now) {
    list_.erase(std::remove_if(list_.begin(), list_.end(),
                               [&](const Timer& t) { return now - t.back() > kKeepAfter; }),
                list_.end());
}

std::vector<Timer> Timers::shown(long long now) const {
    std::vector<Timer> out;
    for (const Timer& t : list_)
        if (now - t.back() <= kKeepAfter) out.push_back(t);
    std::sort(out.begin(), out.end(), [](const Timer& a, const Timer& b) { return a.back() < b.back(); });
    return out;
}

std::string Timers::text() const {
    std::string out;
    for (const Timer& t : list_)
        out += std::to_string(t.mob) + ' ' + t.map + ' ' + std::to_string(t.died) + ' ' +
               std::to_string(t.respawnMs) + '\n';
    return out;
}

Timers Timers::parse(const std::string& text) {
    Timers out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        Timer t;
        if (l >> t.mob >> t.map >> t.died >> t.respawnMs && t.mob > 0 && t.respawnMs > 0) out.list_.push_back(t);
    }
    return out;
}

std::string left(long long back, long long now) {
    long long s = back - now;
    const bool past = s < 0;
    if (past) s = -s;
    char buf[32];
    if (s >= 3600)
        std::snprintf(buf, sizeof buf, "%s%lld:%02lld:%02lld", past ? "+" : "", s / 3600, s / 60 % 60, s % 60);
    else
        std::snprintf(buf, sizeof buf, "%s%lld:%02lld", past ? "+" : "", s / 60, s % 60);
    return buf;
}

}  // namespace uagfx::mvptimers
