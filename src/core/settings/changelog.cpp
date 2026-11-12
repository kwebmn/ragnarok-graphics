#include "core/settings/changelog.hpp"

#include <cctype>

#include "core/update_check.hpp"

namespace uagfx::settings::changelog {
namespace {

constexpr const char* kHead = "# uaRO Graphics ";

std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

std::string plain(std::string s) {
    for (const std::string mark : {"**", "`"})
        for (size_t at = s.find(mark); at != std::string::npos; at = s.find(mark, at)) s.erase(at, mark.size());
    return s;
}

std::string trimmed(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

bool dateAt(const std::string& s, size_t at) {  // YYYY-MM-DD at `at`
    if (at + 10 > s.size()) return false;
    for (size_t i = 0; i < 10; ++i) {
        const char c = s[at + i];
        if ((i == 4 || i == 7) ? c != '-' : !std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

bool versionOf(const std::string& line, Version& out) {
    if (line.compare(0, std::char_traits<char>::length(kHead), kHead) != 0) return false;
    const size_t from = std::char_traits<char>::length(kHead);
    const size_t end = line.find(' ', from);
    out = Version{};
    out.version = line.substr(from, end == std::string::npos ? std::string::npos : end - from);
    if (out.version.empty() || !std::isdigit(static_cast<unsigned char>(out.version[0]))) return false;
    out.beta = out.version.find("-beta") != std::string::npos;
    const size_t dot = line.rfind(" · ");
    if (dot != std::string::npos) {
        const size_t at = dot + std::char_traits<char>::length(" · ");
        if (dateAt(line, at) && at + 10 == line.size()) out.date = line.substr(at, 10);
    }
    return true;
}

}  // namespace

std::vector<Version> parse(const std::string& markdown) {
    std::vector<Version> out;
    bool in = false;  // under a version's heading
    for (const std::string& line : linesOf(markdown)) {
        if (line.compare(0, 2, "# ") == 0) {
            Version v;
            in = versionOf(line, v);
            if (in) out.push_back(v);
            continue;
        }
        if (!in) continue;
        const std::string text = trimmed(line);
        if (text.empty()) continue;
        std::vector<Point>& points = out.back().points;
        if (line.compare(0, 3, "## ") == 0) {
            points.push_back(Point{plain(trimmed(line.substr(3))), true});
        } else if (line[0] == ' ' && !points.empty() && !points.back().section) {
            points.back().text += "\n" + plain(text);  // a step under its point
        } else {
            points.push_back(Point{plain(text.compare(0, 2, "- ") == 0 ? text.substr(2) : text), false});
        }
    }
    return out;
}

size_t newerThan(const std::vector<Version>& v, const std::string& seen) {
    if (seen.empty()) return 0;
    size_t n = 0;
    while (n < v.size() && update::newer(v[n].version, seen)) ++n;
    return n;
}

std::vector<Version> forChannel(const std::vector<Version>& all, bool beta) {
    if (beta) return all;
    std::vector<Version> out;
    for (const Version& v : all) {
        if (!v.beta) {
            out.push_back(v);
            continue;
        }
        const std::string release = v.version.substr(0, v.version.find('-'));
        for (Version& r : out)
            if (r.version == release) r.points.insert(r.points.end(), v.points.begin(), v.points.end());
    }
    return out;
}

std::vector<std::string> quotedNames(const std::string& point) {
    std::vector<std::string> names;
    const std::string open = "«", close = "»";
    for (size_t at = point.find(open); at != std::string::npos; at = point.find(open, at)) {
        const size_t from = at + open.size(), to = point.find(close, from);
        if (to == std::string::npos) break;
        if (to > from) names.push_back(point.substr(from, to - from));
        at = to + close.size();
    }
    return names;
}

}  // namespace uagfx::settings::changelog
