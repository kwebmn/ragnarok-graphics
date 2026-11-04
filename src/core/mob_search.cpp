#include "core/mob_search.hpp"

#include <algorithm>
#include <cctype>

namespace uagfx::mobsearch {
namespace {

const std::string kHeader = "Mob Search... ";

std::string lowered(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string plainMap(const std::string& map) {
    std::string m = lowered(map);
    for (const char* ext : {".rsw", ".gat", ".gnd"})
        if (m.size() > 4 && m.compare(m.size() - 4, 4, ext) == 0) m.resize(m.size() - 4);
    return m;
}

// Digits after spaces from `at`; the number in `v`, `at` past them
bool number(const std::string& s, size_t& at, int& v) {
    while (at < s.size() && s[at] == ' ') ++at;
    const size_t from = at;
    v = 0;
    while (at < s.size() && std::isdigit(static_cast<unsigned char>(s[at])) && at - from < 6)
        v = v * 10 + (s[at++] - '0');
    return at > from;
}

}  // namespace

std::optional<std::pair<std::string, std::string>> header(const std::string& line) {
    if (line.compare(0, kHeader.size(), kHeader) != 0) return std::nullopt;
    const std::string rest = line.substr(kHeader.size());
    const size_t last = rest.find_last_of(' ');
    if (last == std::string::npos || last == 0 || last + 1 >= rest.size()) return std::nullopt;
    return std::make_pair(rest.substr(0, last), rest.substr(last + 1));
}

std::optional<Point> point(const std::string& line) {
    size_t at = 0;
    int n = 0;
    Point p;
    if (!number(line, at, n) || at >= line.size() || line[at] != '[') return std::nullopt;
    ++at;
    if (!number(line, at, p.x) || at >= line.size() || line[at] != ':') return std::nullopt;
    ++at;
    if (!number(line, at, p.y) || at + 1 >= line.size() || line[at] != ']' || line[at + 1] != ' ') return std::nullopt;
    p.name = line.substr(at + 2);
    if (p.name.empty()) return std::nullopt;
    return p;
}

bool sameMap(const std::string& searched, const std::string& shown) {
    return !searched.empty() && plainMap(searched) == plainMap(shown);
}

bool Reader::line(const std::string& text, uint64_t nowMs) {
    if (const auto h = header(text)) {
        result_ = Result{h->first, h->second, {}};
        active_ = collecting_ = true;
        lastAt_ = nowMs;
        return true;
    }
    if (collecting_ && nowMs - lastAt_ <= kLineGapMs) {
        if (const auto p = point(text)) {
            result_.points.push_back(*p);
            lastAt_ = nowMs;
            return true;
        }
    }
    collecting_ = false;
    return false;
}

void Reader::clear() {
    result_ = Result{};
    active_ = collecting_ = false;
}

}  // namespace uagfx::mobsearch
