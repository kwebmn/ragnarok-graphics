#include "core/window_title.hpp"

#include <cstdint>
#include <vector>

namespace uagfx::window_title {
namespace {

constexpr int kMostLevel = 999;
constexpr int kHeavy = 50;  // %: the game heals no more by itself

// 0-100 %, a little left never shown as none
int share(int part, int most) {
    const int p = int(int64_t(part) * 100 / most);
    return p > 100 ? 100 : p == 0 && part > 0 ? 1 : p;
}

// "45,2" (in English "45.2"): down to a tenth, never a whole 100 before the level comes
std::string tenths(int part, int most, settings::Lang lang) {
    int64_t t = int64_t(part) * 1000 / most;
    if (t > 999) t = 999;
    return std::to_string(t / 10) + (lang == settings::Lang::En ? "." : ",") + std::to_string(t % 10);
}

}  // namespace

std::string title(const std::string& name, const charstatus::Status* status, const std::string& map,
                  const std::string& client, settings::Lang lang) {
    if (name.empty()) return client;
    std::vector<std::string> parts;
    if (status) {
        const charstatus::Status& s = *status;
        if (s.baseLevel > 0 && s.baseLevel <= kMostLevel)
            parts.push_back(std::to_string(s.baseLevel) +
                            (s.jobLevel > 0 && s.jobLevel <= kMostLevel ? "/" + std::to_string(s.jobLevel) : ""));
        if (s.baseNext > 0 && s.baseExp >= 0)
            parts.push_back(tenths(s.baseExp, s.baseNext, lang) + " %");
        else if (s.baseNext == 0 && s.jobNext > 0 && s.jobExp >= 0)
            parts.push_back(tenths(s.jobExp, s.jobNext, lang) + " %");
        if (s.hpMost > 0 && s.hp >= 0) parts.push_back("HP " + std::to_string(share(s.hp, s.hpMost)) + " %");
        if (s.spMost > 0 && s.sp >= 0) parts.push_back("SP " + std::to_string(share(s.sp, s.spMost)) + " %");
        if (s.weightMost > 0 && s.weight >= 0 && share(s.weight, s.weightMost) >= kHeavy)
            parts.push_back(settings::fmt(settings::tr(lang, settings::Text::TitleWeight),
                                          {std::to_string(share(s.weight, s.weightMost))}));
    }
    if (!map.empty()) parts.push_back(map);
    std::string text = name;
    for (const std::string& p : parts) text += " · " + p;
    return text + " - " + client;
}

}  // namespace uagfx::window_title
