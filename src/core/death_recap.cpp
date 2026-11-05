#include "core/death_recap.hpp"

#include <cstdio>

namespace uagfx::deathrecap {

void Log::hit(const Hit& h) {
    hits_.push_back(h);
    while (hits_.size() > kMost) hits_.pop_front();
    while (!hits_.empty() && h.at > hits_.front().at + kKeepMs) hits_.pop_front();
}

std::vector<Hit> Log::before(uint64_t deathAt) const {
    std::vector<Hit> out;
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it)
        if (it->at <= deathAt + 500 && it->at + kKeepMs >= deathAt) out.push_back(*it);
    return out;
}

std::string line(const Hit& h, uint64_t deathAt, const std::string& skillName, const Texts& t, char decimal) {
    std::string who = h.who.empty() ? "#" + std::to_string(h.source) : h.who;
    const std::string what = h.skill ? (skillName.empty() ? "#" + std::to_string(h.skill) : skillName) : t.attack;
    char ago[16];
    const double sec = deathAt > h.at ? double(deathAt - h.at) / 1000.0 : 0.0;
    std::snprintf(ago, sizeof ago, "%.1f", sec);
    for (char* c = ago; *c; ++c)
        if (*c == '.') *c = decimal;
    std::string when = t.seconds;
    const size_t at = when.find("{1}");
    if (at != std::string::npos) when.replace(at, 3, ago);
    return who + " \xC2\xB7 " + what + " \xC2\xB7 " + std::to_string(h.damage) + " \xC2\xB7 " + when;
}

long long total(const std::vector<Hit>& hits) {
    long long sum = 0;
    for (const Hit& h : hits) sum += h.damage;
    return sum;
}

}  // namespace uagfx::deathrecap
