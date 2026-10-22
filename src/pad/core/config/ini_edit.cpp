#include "pad/core/config/ini_edit.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace uapad {
namespace {

const char kBom[] = "\xEF\xBB\xBF";

struct Line {
    std::string body;  // without the line end
    std::string eol;   // "\r\n", "\n" or "" for a last line without one
};

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<Line> splitLines(const std::string& text) {
    std::vector<Line> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) {
            lines.push_back(Line{text.substr(pos), ""});
            break;
        }
        const bool crlf = nl > pos && text[nl - 1] == '\r';
        lines.push_back(Line{text.substr(pos, nl - pos - (crlf ? 1 : 0)), crlf ? "\r\n" : "\n"});
        pos = nl + 1;
    }
    return lines;
}

// What parseConfig sees in a line: the part before ';', trimmed (the first line without a BOM).
std::string meaning(const std::string& body, bool first) {
    std::string s = first && body.rfind(kBom, 0) == 0 ? body.substr(3) : body;
    const size_t comment = s.find(';');
    if (comment != std::string::npos) s = s.substr(0, comment);
    return trim(s);
}

bool sectionName(const std::string& meant, std::string& name) {
    if (meant.size() < 2 || meant.front() != '[' || meant.back() != ']') return false;
    name = trim(meant.substr(1, meant.size() - 2));
    return true;
}

bool keyName(const std::string& meant, std::string& key) {
    const size_t eq = meant.find('=');
    if (eq == std::string::npos) return false;
    key = trim(meant.substr(0, eq));
    return true;
}

// The line with a new value: what comes before the old value and what follows it (spaces, comment) stays.
std::string withValue(const std::string& body, const std::string& value) {
    const size_t eq = body.find('=');
    size_t start = eq + 1;
    while (start < body.size() && (body[start] == ' ' || body[start] == '\t')) ++start;
    size_t end = body.find(';', start);
    if (end == std::string::npos) end = body.size();
    while (end > start && (body[end - 1] == ' ' || body[end - 1] == '\t')) --end;
    std::string out = body.substr(0, start);
    if (start == body.size() && !out.empty() && out.back() == '=') out += ' ';  // "key =" with nothing after it
    return out + value + body.substr(end);
}

}  // namespace

std::string applyIniEdits(const std::string& text, const std::vector<IniEdit>& edits) {
    std::vector<Line> lines = splitLines(text);
    const std::string eol = text.find("\r\n") != std::string::npos ? "\r\n"
                            : text.find('\n') != std::string::npos ? "\n"
                                                                   : "\r\n";
    std::vector<std::string> sectionOf(lines.size());
    std::string current;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string name;
        if (sectionName(meaning(lines[i].body, i == 0), name)) current = name;
        sectionOf[i] = current;
    }

    std::vector<std::pair<size_t, std::string>> inserts;  // a new line after line i
    std::vector<IniEdit> newSections;
    for (const IniEdit& e : edits) {
        bool replaced = false;
        size_t last = lines.size();  // the section's last line with something on it
        for (size_t i = 0; i < lines.size(); ++i) {
            if (sectionOf[i] != e.section) continue;
            const std::string meant = meaning(lines[i].body, i == 0);
            std::string name;
            std::string key;
            if (!meant.empty()) last = i;
            if (sectionName(meant, name) || !keyName(meant, key) || key != e.key) continue;
            lines[i].body = withValue(lines[i].body, e.value);
            replaced = true;
        }
        if (replaced) continue;
        if (last < lines.size())
            inserts.emplace_back(last, e.key + " = " + e.value);
        else
            newSections.push_back(e);
    }
    std::stable_sort(inserts.begin(), inserts.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    std::string out;
    size_t next = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i].body;
        const bool more = next < inserts.size() && inserts[next].first == i;
        out += lines[i].eol.empty() && more ? eol : lines[i].eol;
        for (; next < inserts.size() && inserts[next].first == i; ++next) out += inserts[next].second + eol;
    }
    std::vector<std::string> added;  // section names, in the order the edits name them
    for (const IniEdit& e : newSections)
        if (std::find(added.begin(), added.end(), e.section) == added.end()) added.push_back(e.section);
    for (const std::string& section : added) {
        if (!out.empty() && out.back() != '\n') out += eol;
        if (!out.empty()) out += eol;
        out += "[" + section + "]" + eol;
        for (const IniEdit& e : newSections)
            if (e.section == section) out += e.key + " = " + e.value + eol;
    }
    return out;
}

}  // namespace uapad
