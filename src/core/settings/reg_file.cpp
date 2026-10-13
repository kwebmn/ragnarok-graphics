#include "core/settings/reg_file.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace uagfx::settings {
namespace {

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        if (ch == '\\' || ch == '"') out += '\\';
        out += ch;
    }
    return out + "\"";
}

std::string hexBytes(const std::vector<uint8_t>& b) {
    std::string out;
    char buf[4];
    for (size_t i = 0; i < b.size(); ++i) {
        std::snprintf(buf, sizeof buf, "%02x", b[i]);
        if (i) out += ',';
        out += buf;
    }
    return out;
}

std::string valueText(const RegValue& v) {
    if (v.isSz()) return quoted(v.text);
    if (const auto d = v.asDword()) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "dword:%08x", unsigned(*d));
        return buf;
    }
    if (v.type == uint32_t(RegType::Binary)) return "hex:" + hexBytes(v.bytes);
    char head[16];
    std::snprintf(head, sizeof head, "hex(%x):", unsigned(v.type));
    return head + hexBytes(v.bytes);
}

bool sameText(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

std::string trimRight(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    return s;
}

std::string trimLeft(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a;
    return s.substr(a);
}

// The logical lines: '\' at a line's end joins the next line (regedit wraps long hex values so).
std::vector<std::string> logicalLines(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    bool joining = false;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = trimRight(text.substr(pos, nl - pos));
        if (joining) line = trimLeft(line);
        const bool more = !line.empty() && line.back() == '\\';
        if (more) line.pop_back();
        current += line;
        joining = more;
        if (!more) {
            out.push_back(current);
            current.clear();
        }
        pos = nl + 1;
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

// A quoted string from s[i] (at the opening quote); i ends after the closing quote.
bool readQuoted(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    out.clear();
    for (++i; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            out += s[++i];
        } else if (s[i] == '"') {
            ++i;
            return true;
        } else {
            out += s[i];
        }
    }
    return false;
}

bool readHex(const std::string& s, std::vector<uint8_t>& out) {
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ',' || s[i] == ' ')) ++i;
        if (i >= s.size()) break;
        char* end = nullptr;
        const long b = std::strtol(s.c_str() + i, &end, 16);
        const size_t used = size_t(end - (s.c_str() + i));
        if (used == 0 || used > 2 || b < 0 || b > 255) return false;
        out.push_back(uint8_t(b));
        i += used;
    }
    return true;
}

// UTF-16LE bytes as UTF-8 text, up to the first NUL: regedit exports a REG_SZ with NULs after its text - the game
// writes SITEM, SKINNAME and ID with a fixed size (registry.md) - as hex(1).
std::string utf16leText(const std::vector<uint8_t>& b) {
    std::string out;
    for (size_t i = 0; i + 1 < b.size(); i += 2) {
        uint32_t c = uint32_t(b[i]) | uint32_t(b[i + 1]) << 8;
        if (c == 0) break;
        if (c >= 0xD800 && c < 0xDC00 && i + 3 < b.size()) {  // a surrogate pair
            const uint32_t low = uint32_t(b[i + 2]) | uint32_t(b[i + 3]) << 8;
            if (low >= 0xDC00 && low < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            }
        }
        if (c < 0x80) {
            out += char(c);
        } else if (c < 0x800) {
            out += char(0xC0 | c >> 6);
            out += char(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += char(0xE0 | c >> 12);
            out += char(0x80 | (c >> 6 & 0x3F));
            out += char(0x80 | (c & 0x3F));
        } else {
            out += char(0xF0 | c >> 18);
            out += char(0x80 | (c >> 12 & 0x3F));
            out += char(0x80 | (c >> 6 & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
    }
    return out;
}

bool readValue(const std::string& s, RegValue& v) {
    if (!s.empty() && s[0] == '"') {
        size_t i = 0;
        std::string text;
        if (!readQuoted(s, i, text)) return false;
        v = RegValue::sz(text);
        return true;
    }
    if (s.rfind("dword:", 0) == 0) {
        char* end = nullptr;
        const unsigned long d = std::strtoul(s.c_str() + 6, &end, 16);
        if (end == s.c_str() + 6) return false;
        v = RegValue::dword(uint32_t(d));
        return true;
    }
    if (s.rfind("hex:", 0) == 0) {
        std::vector<uint8_t> b;
        if (!readHex(s.substr(4), b)) return false;
        v = RegValue::binary(b);
        return true;
    }
    if (s.rfind("hex(", 0) == 0) {
        const size_t close = s.find("):");
        if (close == std::string::npos) return false;
        v = RegValue{};
        v.type = uint32_t(std::strtoul(s.substr(4, close - 4).c_str(), nullptr, 16));
        if (!readHex(s.substr(close + 2), v.bytes)) return false;
        if (v.type == uint32_t(RegType::Sz)) v = RegValue::sz(utf16leText(v.bytes));  // regedit: NULs after the text
        return true;
    }
    return false;
}

}  // namespace

std::string toRegFile(const RegCopy& copy, const std::string& root) {
    std::string out = "Windows Registry Editor Version 5.00\r\n\r\n";
    if (!copy.exists) return out;
    auto section = [&](const std::string& subkey, const RegValues* values) {
        out += "[" + root + (subkey.empty() ? "" : "\\" + subkey) + "]\r\n";
        if (values)
            for (const auto& v : *values)
                out += (v.first.empty() ? std::string("@") : quoted(v.first)) + "=" + valueText(v.second) + "\r\n";
        out += "\r\n";
    };
    const auto self = copy.keys.find("");
    section("", self == copy.keys.end() ? nullptr : &self->second);  // the key itself first, even without values
    for (const auto& key : copy.keys)
        if (!key.first.empty()) section(key.first, &key.second);
    return out;
}

bool fromRegFile(const std::string& text, const std::string& root, RegCopy& out) {
    std::string body = text.rfind("\xEF\xBB\xBF", 0) == 0 ? text.substr(3) : text;
    const std::vector<std::string> lines = logicalLines(body);
    size_t first = 0;
    while (first < lines.size() && lines[first].empty()) ++first;
    if (first == lines.size() ||
        (lines[first] != "Windows Registry Editor Version 5.00" && lines[first] != "REGEDIT4"))
        return false;
    out = RegCopy{};
    bool inside = false;
    std::string subkey;
    for (size_t n = first + 1; n < lines.size(); ++n) {
        const std::string& line = lines[n];
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[') {
            inside = false;
            if (line.size() < 2 || line.back() != ']' || line[1] == '-') continue;
            const std::string path = line.substr(1, line.size() - 2);
            if (sameText(path, root)) {
                subkey.clear();
            } else if (path.size() > root.size() + 1 && sameText(path.substr(0, root.size()), root) &&
                       path[root.size()] == '\\') {
                subkey = path.substr(root.size() + 1);
            } else {
                continue;
            }
            inside = true;
            out.exists = true;
            out.keys[subkey];
            continue;
        }
        if (!inside) continue;
        std::string name;
        size_t i = 0;
        if (line[0] == '@') {
            i = 1;
        } else if (!readQuoted(line, i, name)) {
            continue;
        }
        if (i >= line.size() || line[i] != '=') continue;
        const std::string rest = line.substr(i + 1);
        if (rest == "-") continue;
        RegValue v;
        if (readValue(rest, v)) out.keys[subkey][name] = v;
    }
    return true;
}

const std::vector<std::string>& gameKeyRoots() {
    static const std::vector<std::string> roots{
        "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok",
        "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Gravity Soft\\Ragnarok",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Gravity Soft\\Ragnarok"};
    return roots;
}

bool fromRegFileAny(const std::string& text, const std::vector<std::string>& roots, RegCopy& out) {
    for (const std::string& root : roots) {
        RegCopy c;
        if (fromRegFile(text, root, c) && c.exists) {
            out = c;
            return true;
        }
    }
    return false;
}

}  // namespace uagfx::settings
