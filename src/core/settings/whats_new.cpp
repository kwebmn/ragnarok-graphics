#include "core/settings/whats_new.hpp"

#include <vector>

namespace uagfx::settings {
namespace {

std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

// "# uaRO Graphics 2.5.27 — ..." for 2.5.27, not for 2.5.2
bool heads(const std::string& line, const std::string& version) {
    const std::string mark = "uaRO Graphics " + version;
    const size_t at = line.find(mark);
    if (line.compare(0, 2, "# ") != 0 || at == std::string::npos) return false;
    const size_t after = at + mark.size();
    return after == line.size() || (line[after] != '.' && (line[after] < '0' || line[after] > '9'));
}

// "  1. ..." - a numbered step under a point
bool step(const std::string& line, size_t text) {
    size_t i = text;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
    return text > 0 && i > text && line.compare(i, 2, ". ") == 0;
}

std::string plain(std::string s) {
    for (const std::string mark : {"**", "`"})
        for (size_t at = s.find(mark); at != std::string::npos; at = s.find(mark, at)) s.erase(at, mark.size());
    return s;
}

}  // namespace

std::string notesOf(const std::string& markdown, const std::string& version) {
    const std::vector<std::string> lines = linesOf(markdown);
    size_t i = 0;
    while (i < lines.size() && !heads(lines[i], version)) ++i;
    if (i == lines.size()) return "";
    std::vector<std::string> points;
    for (++i; i < lines.size() && lines[i].compare(0, 2, "# ") != 0; ++i) {
        const std::string& line = lines[i];
        const size_t text = line.find_first_not_of(' ');
        if (text == std::string::npos) continue;
        if (line.compare(text, 2, "- ") == 0 && text == 0)
            points.push_back("• " + line.substr(2));
        else if (step(line, text))  // its own line, under its point
            points.push_back("   " + line.substr(text));
        else if (!points.empty() && text > 0)  // a point's (or a step's) next line
            points.back() += " " + line.substr(text);
        else
            points.push_back(line.substr(text));
    }
    std::string out;
    for (const std::string& p : points) out += (out.empty() ? "" : "\n") + plain(p);
    return out;
}

}  // namespace uagfx::settings
