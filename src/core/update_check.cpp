#include "core/update_check.hpp"

#include <cctype>
#include <cstdlib>
#include <map>
#include <vector>

#include "core/settings/ini_edit.hpp"

namespace uagfx::update {
namespace {

std::vector<long> numbers(const std::string& v) {
    std::vector<long> out;
    size_t at = 0;
    while (at < v.size()) {
        size_t dot = v.find('.', at);
        if (dot == std::string::npos) dot = v.size();
        out.push_back(std::strtol(v.substr(at, dot - at).c_str(), nullptr, 10));
        at = dot + 1;
    }
    return out;
}

// The parts after '-' ("beta.2"): dot by dot, numbers by their value, words as text (semver's order).
bool preReleaseLater(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i <= a.size() && j <= b.size()) {
        const size_t ei = a.find('.', i), ej = b.find('.', j);
        const std::string p = a.substr(i, ei == std::string::npos ? std::string::npos : ei - i);
        const std::string q = b.substr(j, ej == std::string::npos ? std::string::npos : ej - j);
        const bool pn = !p.empty() && p.find_first_not_of("0123456789") == std::string::npos;
        const bool qn = !q.empty() && q.find_first_not_of("0123456789") == std::string::npos;
        if (pn && qn && std::strtol(p.c_str(), nullptr, 10) != std::strtol(q.c_str(), nullptr, 10))
            return std::strtol(p.c_str(), nullptr, 10) > std::strtol(q.c_str(), nullptr, 10);
        if (!(pn && qn) && p != q) return pn != qn ? !pn : p > q;
        if (ei == std::string::npos || ej == std::string::npos) return ei != std::string::npos;
        i = ei + 1;
        j = ej + 1;
    }
    return false;
}

bool isVersion(const std::string& v) {
    if (v.empty() || !std::isdigit(static_cast<unsigned char>(v[0]))) return false;
    for (char c : v)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') return false;
    return true;
}

// One archive's lines: a plain file name (no folder), a size, a SHA-256.
bool archiveOf(const std::string& file, const std::string& size, const std::string& sha, Archive& a,
               const std::string& section, std::string& error) {
    bool plain = !file.empty() && file.size() <= 80;
    for (char c : file)
        plain = plain && (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-');
    if (!plain || file.find("..") != std::string::npos) {
        error = "update.ini: [" + section + "] file is not a plain file name";
        return false;
    }
    char* end = nullptr;
    const unsigned long long n = size.empty() || size[0] == '-' ? 0 : std::strtoull(size.c_str(), &end, 10);
    if (!n || !end || *end) {
        error = "update.ini: [" + section + "] size is not a number of bytes";
        return false;
    }
    std::string hex;
    for (char c : sha) hex += char(std::tolower(static_cast<unsigned char>(c)));
    if (hex.size() != 64 || hex.find_first_not_of("0123456789abcdef") != std::string::npos) {
        error = "update.ini: [" + section + "] sha256 is not 64 hex digits";
        return false;
    }
    a.file = file;
    a.size = n;
    a.sha256 = hex;
    return true;
}

}  // namespace

bool newer(const std::string& a, const std::string& b) {
    if (a.empty()) return false;
    const size_t da = a.find('-'), db = b.find('-');
    const std::vector<long> x = numbers(a.substr(0, da)), y = numbers(b.substr(0, db));
    for (size_t i = 0; i < x.size() || i < y.size(); ++i) {
        const long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p > q;
    }
    if (da == std::string::npos || db == std::string::npos) return da == std::string::npos && db != std::string::npos;
    return preReleaseLater(a.substr(da + 1), b.substr(db + 1));
}

std::string bare(const std::string& version) { return version.substr(0, version.find(' ')); }

bool parseManifest(const std::string& text, Manifest& out, std::string& error) {
    const std::map<std::string, std::string> ini = settings::readIni(text);
    const auto value = [&](const std::string& key) {
        const auto it = ini.find(key);
        return it == ini.end() ? std::string() : it->second;
    };
    const std::string format = value("release.format");
    if (format != "1") {
        error = "update.ini is not format 1 (format " + (format.empty() ? std::string("?") : format) + ")";
        return false;
    }
    Manifest m;
    m.package.version = value("release.version");
    if (!isVersion(m.package.version)) {
        error = "update.ini has no [release] version";
        return false;
    }
    for (const char* section : {"package", "dlss", "models"}) {
        const std::string s = section;
        Archive& a = s == "package" ? m.package : s == "dlss" ? m.dlss : m.models;
        if (s != "package") {
            bool any = false;
            for (const char* k : {".version", ".file", ".size", ".sha256"}) any = any || ini.count(s + k);
            if (!any) continue;  // the site has none of them
            a.version = value(s + ".version");
            if (a.version.empty()) {
                error = "update.ini has no [" + s + "] version";
                return false;
            }
        }
        if (!archiveOf(value(s + ".file"), value(s + ".size"), value(s + ".sha256"), a, s, error)) return false;
    }
    std::string list = value("release.withdrawn");
    for (size_t at = 0; at < list.size();) {
        const size_t space = list.find(' ', at);
        const std::string v = list.substr(at, space == std::string::npos ? std::string::npos : space - at);
        if (!v.empty()) m.withdrawn.push_back(v);
        if (space == std::string::npos) break;
        at = space + 1;
    }
    out = m;
    return true;
}

std::string offer(const Manifest& m, const std::string& installed, const std::string& skip) {
    const std::string site = m.package.version, mine = bare(installed);
    if (site.empty() || site == mine || site == skip) return "";
    for (const std::string& w : m.withdrawn)
        if (w == mine) return site;
    return newer(site, mine) ? site : "";
}

bool payloadAt(const uint8_t* tail16, uint64_t fileSize, uint64_t& offset, uint64_t& size) {
    if (fileSize < 16 || std::string(reinterpret_cast<const char*>(tail16) + 8, 8) != "UAROSETP") return false;
    uint64_t n = 0;
    for (int i = 7; i >= 0; --i) n = n << 8 | tail16[i];
    if (!n || n > fileSize - 16) return false;
    offset = fileSize - 16 - n;
    size = n;
    return true;
}

bool askAtStart(const std::string& updateInfo, const std::string& installed, std::string& version) {
    Manifest m;
    std::string error;
    if (!parseManifest(updateInfo, m, error)) return false;
    const std::map<std::string, std::string> ini = settings::readIni(updateInfo);
    const auto skip = ini.find("local.skip");
    version = offer(m, installed, skip == ini.end() ? "" : skip->second);
    return !version.empty();
}

}  // namespace uagfx::update
