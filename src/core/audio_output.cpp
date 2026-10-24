#include "core/audio_output.hpp"

#include <algorithm>

namespace uagfx::audio_output {

bool sameId(const std::string& a, const std::string& b) {
    const auto low = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + 32) : c; };
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [&](char x, char y) {
               return low(x) == low(y);
           });
}

Target pick(const std::string& wanted, const std::vector<std::string>& active) {
    Target t;
    if (wanted.empty()) return t;
    const bool there = std::any_of(active.begin(), active.end(), [&](const std::string& id) {
        return sameId(id, wanted);
    });
    if (there) t.id = wanted;
    t.away = !there;
    return t;
}

bool reopen(const std::string& openedId, const Target& target, bool lost) {
    return lost || !sameId(openedId, target.id);
}

}  // namespace uagfx::audio_output
