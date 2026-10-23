#include "core/client_patch.hpp"

#include <sstream>

#include "core/json.hpp"

namespace uagfx::client_patch {
namespace {

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

bool endsWith(const std::string& s, const std::string& end) {
    return s.size() >= end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0;
}

bool hexSha(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}

bool letterOrDigit(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

// The file of this path in `m`, in any case: Windows' names
const File* find(const Manifest& m, const std::string& path) {
    const std::string want = lower(path);
    for (const File& f : m.files)
        if (lower(f.path) == want) return &f;
    return nullptr;
}

// The file on the disk is `f` - its sha256 known and the same
bool same(const Have& h, const File& f) {
    return h.exists && h.size == f.size && !h.sha256.empty() && h.sha256 == f.sha256;
}

}  // namespace

std::string clientName(const std::string& name) {
    bool fits = !name.empty() && name.size() <= 40 && letterOrDigit(name.front()) && name.back() != ' ' &&
                name.back() != '.';
    for (char c : name)
        fits = fits && (letterOrDigit(c) || c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == ')');
    const std::string low = lower(name);
    const bool device = low == "con" || low == "prn" || low == "aux" || low == "nul" ||
                        (low.size() == 4 && (low.rfind("com", 0) == 0 || low.rfind("lpt", 0) == 0) &&
                         low[3] >= '1' && low[3] <= '9');
    return fits && !device ? name : kOldClient;
}

bool sameClient(const std::string& a, const std::string& b) {
    return lower(a.empty() ? kOldClient : a) == lower(b.empty() ? kOldClient : b);
}

bool playersOwn(const std::string& path) {
    const std::string p = lower(path);
    return p == "palettes.grf" || p == "dinput8.ini";
}

bool safePath(const std::string& path) {
    if (path.empty() || path.size() > 200) return false;
    for (char c : path) {  // ASCII letters, digits, ". _ - ( )", space and '/': the game's names in its code page
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '_' || c == '-' || c == '(' || c == ')' || c == ' ' || c == '/';
        if (!ok) return false;
    }
    std::vector<std::string> parts;
    std::stringstream in(path);
    std::string part;
    while (std::getline(in, part, '/')) parts.push_back(part);
    if (path.back() == '/') parts.push_back("");
    for (const std::string& p : parts)  // inside the game's folder, no name Windows would cut
        if (p.empty() || p == "." || p == ".." || p.back() == '.' || p.back() == ' ' || p.front() == ' ') return false;
    const std::string all = lower(path), first = lower(parts.front());
    const bool root = parts.size() == 1;
    if (all == "data.ini" || all == kClientFile) return false;  // 3.0.1: the client build's own manifest neither
    if (first == "uaro-graphics" || first == "dgvoodoo" || all == "ddraw.dll" || all == "uaro-graphics.exe" ||
        all == "uaro-graphics.ini" || all == "dinput.dll")
        return false;  // uaRO Graphics' own
    if (root && ((all.rfind("test.", 0) == 0 && endsWith(all, ".md")) ||
                 (all.rfind("uaro-", 0) == 0 && endsWith(all, ".log")) ||
                 (all.rfind("uaro-graphics-", 0) == 0 && endsWith(all, ".txt"))))
        return false;
    if (first == "screenshot" || first == "emblem" || first == "_tmpemblem") return false;  // the player's
    for (const char* program : {".exe", ".dll", ".bat", ".cmd", ".com", ".scr", ".ps1", ".vbs", ".js", ".msi", ".lnk",
                                ".asi"})
        if (endsWith(all, program)) return false;
    return true;
}

std::string localPath(const std::string& path) {
    std::string out = path;
    for (char& c : out)
        if (c == '/') c = '\\';
    return out;
}

bool parse(const std::string& text, Manifest& out, std::string& error) {
    json::Value v;
    if (!json::parse(text, v, error)) return false;
    if (v.type() != json::Value::Object || v["format"].asInt() != 1 || v["files"].type() != json::Value::Array) {
        error = "not a client manifest of format 1";
        return false;
    }
    Manifest m;
    m.client = v["client"].asString();
    m.version = v["version"].asString();
    for (const json::Value& f : v["files"].items()) {
        File file;
        file.path = f["path"].asString();
        file.sha256 = f["sha256"].asString();
        const int64_t size = f["size"].asInt(-1);
        if (!safePath(file.path) || !hexSha(file.sha256) || size < 0) continue;
        file.size = uint64_t(size);
        m.files.push_back(file);
    }
    out = m;
    return true;
}

Plan plan(const Manifest& next, const Manifest& last, const Manifest& build, const Look& look) {
    Plan p;
    for (const File& f : next.files) {
        if (!playersOwn(f.path)) {
            const Have h = look(f.path, {f.size});
            if (!h.exists || h.size != f.size || (!h.sha256.empty() && h.sha256 != f.sha256)) p.put.push_back(f);
            continue;
        }
        const File* before[] = {find(last, f.path), find(build, f.path)};  // what the patch and the build put there
        std::vector<uint64_t> sizes{f.size};
        for (const File* b : before)
            if (b) sizes.push_back(b->size);
        const Have h = look(f.path, sizes);
        if (!h.exists || (!same(h, f) && ((before[0] && same(h, *before[0])) || (before[1] && same(h, *before[1])))))
            p.put.push_back(f);
        else if (!same(h, f))
            p.kept.push_back(f);
    }
    for (const File& f : last.files) {
        const bool withdrawn = f.made && !find(next, f.path) && !find(build, f.path) && safePath(f.path);
        if (withdrawn && same(look(f.path, {f.size}), f)) p.remove.push_back(f);
    }
    return p;
}

void carryMade(Manifest& next, const Manifest& last) {
    for (File& f : next.files) {
        const File* before = find(last, f.path);
        f.made = before && before->made;
    }
}

std::string record(const Manifest& m) {
    std::string out = "version " + m.version + "\nclient " + m.client + "\n";
    for (const File& f : m.files)
        out += std::to_string(f.size) + " " + f.sha256 + (f.made ? " made " : " over ") + f.path + "\n";
    return out;
}

bool readRecord(const std::string& text, Manifest& out) {
    Manifest m;
    std::stringstream in(text);
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {
            if (line.rfind("version ", 0) != 0) return false;
            m.version = line.substr(8);
            first = false;
            continue;
        }
        if (line.empty()) continue;
        if (line.rfind("client ", 0) == 0) {
            m.client = line.substr(7);
            continue;
        }
        const size_t a = line.find(' '), b = a == std::string::npos ? a : line.find(' ', a + 1);
        if (b == std::string::npos || a == 0 || a > 19 || line.find_first_not_of("0123456789") != a) return false;
        File f;
        f.size = std::stoull(line.substr(0, a));
        f.sha256 = line.substr(a + 1, b - a - 1);
        f.path = line.substr(b + 1);
        if (f.path.rfind("made ", 0) == 0 || f.path.rfind("over ", 0) == 0) {  // 3.0.0's lines have no mark
            f.made = f.path[0] == 'm';
            f.path = f.path.substr(5);
        }
        if (!hexSha(f.sha256) || f.path.empty()) return false;
        m.files.push_back(f);
    }
    if (first) return false;
    out = m;
    return true;
}

bool upToDate(const Manifest& m, const std::string& recordText,
              const std::function<long long(const std::string&)>& sizeOf) {
    Manifest last;
    if (!readRecord(recordText, last) || last.version != m.version || !sameClient(last.client, m.client) ||
        last.files.size() != m.files.size())
        return false;
    for (size_t i = 0; i < m.files.size(); ++i) {
        const File &f = m.files[i], &was = last.files[i];
        if (f.path != was.path || f.size != was.size || f.sha256 != was.sha256) return false;
        const long long size = sizeOf(f.path);
        if (playersOwn(f.path) ? size < 0 : size != (long long)f.size) return false;
    }
    return true;
}

}  // namespace uagfx::client_patch
