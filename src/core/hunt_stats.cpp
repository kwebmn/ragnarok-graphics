#include "core/hunt_stats.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace uagfx::hunt {
namespace {

constexpr uint64_t kMinuteMs = 60000, kTenMs = 10000;

std::string fill(std::string f, const std::string& a, const std::string& b = "") {
    size_t p = f.find("{1}");
    if (p != std::string::npos) f.replace(p, 3, a);
    p = f.find("{2}");
    if (p != std::string::npos) f.replace(p, 3, b);
    return f;
}

double pctOf(long long exp, long long next) { return next > 0 ? double(exp) * 100.0 / double(next) : 0.0; }

}  // namespace

void Stats::reset(uint64_t now) {
    began_ = now ? now : 1;
    dealt_.clear();
    gained_ = 0;
    known_ = false;
    killed_.clear();
    kills_ = 0;
}

void Stats::hit(int damage, int skill, uint64_t now) {
    if (!began_) reset(now);
    if (damage <= 0) return;
    dealt_.push_back(Dealt{now, damage, skill});
    while (!dealt_.empty() && dealt_.front().at + kMinuteMs < now) dealt_.pop_front();
}

void Stats::status(int level, long long exp, long long next, long long zeny, uint64_t now) {
    if (!began_) reset(now);
    if (level <= 0) return;
    if (!known_) {
        known_ = true;
        zenyFrom_ = zeny;
    } else if (level > level_) {
        gained_ += (100.0 - pctOf(exp_, next_)) + double(level - level_ - 1) * 100.0 + pctOf(exp, next);
    } else if (level == level_) {
        gained_ += std::max(0.0, pctOf(exp, next) - pctOf(exp_, next_));
    }
    level_ = level;
    exp_ = exp;
    next_ = next;
    zeny_ = zeny;
}

void Stats::kill(const std::string& name) {
    ++kills_;
    ++killed_[name];
}

View Stats::view(uint64_t now) const {
    View v;
    if (!began_) return v;
    v.seconds = now > began_ ? static_cast<long long>((now - began_) / 1000) : 0;
    long long ten = 0, minute = 0;
    std::map<int, long long> bySkill;
    for (const Dealt& d : dealt_) {
        if (d.at + kMinuteMs < now) continue;
        minute += d.damage;
        bySkill[d.skill] += d.damage;
        if (d.at + kTenMs >= now) ten += d.damage;
    }
    const double span = std::max(1.0, std::min(60.0, double(v.seconds)));
    v.dps10 = double(ten) / std::max(1.0, std::min(10.0, double(v.seconds)));
    v.dps60 = double(minute) / span;
    for (const auto& [skill, dmg] : bySkill)
        if (minute > 0) v.skills.emplace_back(skill, int(std::lround(double(dmg) * 100.0 / double(minute))));
    std::sort(v.skills.begin(), v.skills.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.skills.size() > 3) v.skills.resize(3);
    const double hours = double(v.seconds) / 3600.0;
    if (hours > 0.01) {
        v.levelPerHour = gained_ / hours;
        v.zenyPerHour = static_cast<long long>(double(zeny_ - zenyFrom_) / hours);
        if (v.levelPerHour > 0.01 && next_ > 0)
            v.secondsToLevel = static_cast<long long>((100.0 - pctOf(exp_, next_)) / v.levelPerHour * 3600.0);
    }
    v.kills = kills_;
    for (const auto& [name, n] : killed_) v.killed.emplace_back(name, n);
    std::sort(v.killed.begin(), v.killed.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.killed.size() > 3) v.killed.resize(3);
    return v;
}

std::string duration(long long s) {
    char buf[32];
    if (s >= 3600)
        std::snprintf(buf, sizeof buf, "%lld:%02lld:%02lld", s / 3600, (s % 3600) / 60, s % 60);
    else
        std::snprintf(buf, sizeof buf, "%lld:%02lld", s / 60, s % 60);
    return buf;
}

std::string grouped(long long v) {
    const std::string d = std::to_string(v < 0 ? -v : v);
    std::string out = v < 0 ? "\xE2\x88\x92" : "";
    for (size_t i = 0; i < d.size(); ++i) {
        if (i && (d.size() - i) % 3 == 0) out += ' ';
        out += d[i];
    }
    return out;
}

std::vector<std::string> lines(const View& v, const Texts& t, const std::vector<std::string>& skillNames) {
    std::vector<std::string> out;
    out.push_back(fill(t.title, duration(v.seconds)));
    out.push_back(fill(t.dps, grouped(std::llround(v.dps10)), grouped(std::llround(v.dps60))));
    std::string shares;
    for (size_t i = 0; i < v.skills.size(); ++i) {
        const std::string name = v.skills[i].first == 0 ? t.attack
                                 : i < skillNames.size() && !skillNames[i].empty()
                                     ? skillNames[i]
                                     : "#" + std::to_string(v.skills[i].first);
        shares += (shares.empty() ? "" : " \xC2\xB7 ") + name + " " + std::to_string(v.skills[i].second) + " %";
    }
    out.push_back(shares.empty() ? t.none : shares);
    char pct[16];
    std::snprintf(pct, sizeof pct, "%.0f", v.levelPerHour);
    std::string exp = fill(t.exp, pct);
    if (v.secondsToLevel >= 0) exp += " \xC2\xB7 " + fill(t.toLevel, duration(v.secondsToLevel));
    out.push_back(exp);
    out.push_back(fill(t.zeny, (v.zenyPerHour >= 0 ? "+" : "") + grouped(v.zenyPerHour)));
    std::string who;
    for (const auto& [name, n] : v.killed) who += (who.empty() ? "" : " \xC2\xB7 ") + name + " " + std::to_string(n);
    out.push_back(fill(t.kills, std::to_string(v.kills), who.empty() ? t.none : who));
    return out;
}

}  // namespace uagfx::hunt
