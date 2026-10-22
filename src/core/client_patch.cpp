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

}  // namespace

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
    if (all == "data.ini") return false;
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

std::vector<File> needed(const Manifest& m, const std::function<Have(const File&)>& have) {
    std::vector<File> out;
    for (const File& f : m.files) {
        const Have h = have(f);
        if (!h.exists || h.size != f.size || (!h.sha256.empty() && h.sha256 != f.sha256)) out.push_back(f);
    }
    return out;
}

std::string record(const Manifest& m) {
    std::string out = "version " + m.version + "\n";
    for (const File& f : m.files) out += std::to_string(f.size) + " " + f.sha256 + " " + f.path + "\n";
    return out;
}

bool upToDate(const Manifest& m, const std::string& recordText,
              const std::function<long long(const std::string&)>& sizeOf) {
    return !recordText.empty() && recordText == record(m) && [&] {
        for (const File& f : m.files)
            if (sizeOf(f.path) != (long long)f.size) return false;
        return true;
    }();
}

}  // namespace uagfx::client_patch
