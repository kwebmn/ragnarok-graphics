// 3.0.24-beta.26 (docs/superpowers/plans/2026-11-27-other-windows-old-layout.md, task 1; the spec: «каждый символ
// каждой строки окон есть в шрифте; стрелки, звёзды и треугольники — рисованные значки, не буквы»; R-WP985Q
// «непонятные символы в карточке»): every window's markup, every text of strings.cpp and every sign the windows write
// from their own code is a letter IBM Plex Sans has (core/item_ui fontHas) - read from the sources themselves
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/item_ui.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::string readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<std::string> sourcesIn(const std::vector<std::string>& dirs) {
    std::vector<std::string> out;
    for (const std::string& d : dirs)
        for (const auto& e : std::filesystem::directory_iterator(std::string(UAGFX_SOURCE_DIR) + "/" + d))
            if (e.path().extension() == ".cpp") out.push_back(e.path().string());
    return out;
}

void putUtf8(std::string& out, uint32_t c) {
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xC0 | c >> 6), out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += char(0xE0 | c >> 12), out += char(0x80 | (c >> 6 & 0x3F)), out += char(0x80 | (c & 0x3F));
    } else {
        out += char(0xF0 | c >> 18), out += char(0x80 | (c >> 12 & 0x3F)), out += char(0x80 | (c >> 6 & 0x3F));
        out += char(0x80 | (c & 0x3F));
    }
}

// The markup with its entities as UTF-8 letters
std::string decoded(const std::string& rml) {
    static const std::vector<std::pair<std::string, uint32_t>> named = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}, {"&nbsp;", 0xA0}};
    std::string out;
    for (size_t i = 0; i < rml.size(); ++i) {
        if (rml.compare(i, 2, "&#") == 0) {
            const size_t end = rml.find(';', i);
            if (end != std::string::npos && end - i < 12) {
                const bool hex = rml[i + 2] == 'x' || rml[i + 2] == 'X';
                const std::string n = rml.substr(i + (hex ? 3 : 2), end - i - (hex ? 3 : 2));
                putUtf8(out, uint32_t(std::strtoul(n.c_str(), nullptr, hex ? 16 : 10)));
                i = end;
                continue;
            }
        }
        bool done = false;
        for (const auto& [word, c] : named)
            if (rml.compare(i, word.size(), word) == 0) {
                putUtf8(out, c);
                i += word.size() - 1;
                done = true;
                break;
            }
        if (!done) out += rml[i];
    }
    return out;
}

// UTF-8 at all (the game's own bytes - a Korean file name, a code page's words - are not the windows' letters)
bool utf8(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        const auto b = static_cast<unsigned char>(s[i]);
        const size_t n = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
        if (n == 0 || i + n > s.size()) return false;
        for (size_t k = 1; k < n; ++k)
            if ((static_cast<unsigned char>(s[i + k]) >> 6) != 2) return false;
        i += n;
    }
    return true;
}

// The first letter the font has not, as itself ("" none); the code's own marks no window shows - the control
// letters (a separator's \x01), the byte order mark, the Korean of the game's file names - are not counted
std::string missing(const std::string& utf8) {
    for (size_t i = 0; i < utf8.size();) {
        const auto b = static_cast<unsigned char>(utf8[i]);
        const size_t n = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 1;
        const std::string letter = utf8.substr(i, n);
        uint32_t c = n == 1 ? b : n == 2 ? b & 0x1F : n == 3 ? b & 0x0F : b & 0x07;
        for (size_t k = 1; k < n && i + k < utf8.size(); ++k)
            c = c << 6 | (static_cast<unsigned char>(utf8[i + k]) & 0x3F);
        const bool unseen = c < 0x20 || c == 0xFEFF || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x3130 && c <= 0x318F);
        if (!unseen && !itemui::fontHas(letter)) return letter;
        i += n;
    }
    return "";
}

// The string literals of C++ source text, their escapes undone; raw strings, comments and the log's lines left out
std::vector<std::string> literals(const std::string& t) {
    std::vector<std::string> out;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t.compare(i, 2, "//") == 0) {
            i = t.find('\n', i);
            if (i == std::string::npos) break;
            continue;
        }
        if (t.compare(i, 2, "/*") == 0) {
            i = t.find("*/", i);
            if (i == std::string::npos) break;
            ++i;
            continue;
        }
        if (t[i] == '\'') {  // a character: '"' must not open a string
            const size_t end = t.find('\'', i + (t[i + 1] == '\\' ? 3 : 2));
            if (end != std::string::npos && end - i <= 12) i = end;
            continue;
        }
        if (t[i] == 'R' && i + 1 < t.size() && t[i + 1] == '"') {  // R"tag( ... )tag"
            const size_t open = t.find('(', i);
            if (open == std::string::npos) break;
            const std::string close = ")" + t.substr(i + 2, open - i - 2) + "\"";
            i = t.find(close, open);
            if (i == std::string::npos) break;
            i += close.size() - 1;
            continue;
        }
        if (t[i] != '"') continue;
        // the statement it is in, from the last ';', '{' or '}' before it: a log's words are not shown
        const size_t from = t.find_last_of(";{}", i);
        const size_t at = from == std::string::npos ? 0 : from;
        const std::string line = t.substr(at, i - at);
        std::string s;
        size_t j = i + 1;
        for (; j < t.size() && t[j] != '"' && t[j] != '\n'; ++j) {
            if (t[j] != '\\' || j + 1 >= t.size()) {
                s += t[j];
                continue;
            }
            const char e = t[++j];
            if (e == 'x') {
                size_t k = j + 1;
                while (k < t.size() && k < j + 3 && std::isxdigit(static_cast<unsigned char>(t[k]))) ++k;
                s += char(std::strtoul(t.substr(j + 1, k - j - 1).c_str(), nullptr, 16));
                j = k - 1;
            } else if (e == 'u' || e == 'U') {  // a universal name: its letter as UTF-8 (L"\u2605")
                const size_t n = e == 'u' ? 4 : 8;
                putUtf8(s, uint32_t(std::strtoul(t.substr(j + 1, n).c_str(), nullptr, 16)));
                j += n;
            } else {
                s += e == 'n' ? '\n' : e == 't' ? '\t' : e;
            }
        }
        if (line.find("LOG_") == std::string::npos) out.push_back(s);
        i = j;
    }
    return out;
}

}  // namespace

TEST(markup_letters_are_the_fonts) {
    std::string bad;
    for (const std::string& f : sourcesIn({"src/win", "src/uikit"})) {
        const std::string t = readAll(f);
        for (size_t at = t.find("R\"RML("); at != std::string::npos; at = t.find("R\"RML(", at + 1)) {
            const size_t end = t.find(")RML\"", at);
            if (end == std::string::npos) break;
            const std::string m = missing(decoded(t.substr(at + 6, end - at - 6)));
            if (!m.empty()) bad += f.substr(f.rfind('/') + 1) + ": " + m + "\n";
        }
    }
    if (!bad.empty()) std::fprintf(stderr, "markup letters the font has not:\n%s", bad.c_str());
    CHECK(bad.empty());
}

TEST(texts_letters_are_the_fonts) {
    std::string bad;
    for (const std::string& s : literals(readAll(std::string(UAGFX_SOURCE_DIR) + "/src/core/settings/strings.cpp"))) {
        const std::string m = missing(s);
        if (!m.empty()) bad += m + " in «" + s.substr(0, 70) + "»\n";
    }
    if (!bad.empty()) std::fprintf(stderr, "texts' letters the font has not:\n%s", bad.c_str());
    CHECK(bad.empty());
}

TEST(code_words_letters_are_the_fonts) {
    std::string bad;
    for (const std::string& f : sourcesIn({"src/win", "src/uikit", "src/core"}))
        for (const std::string& s : literals(readAll(f))) {
            if (!utf8(s)) continue;
            const std::string m = missing(s);
            if (!m.empty()) bad += f.substr(f.rfind('/') + 1) + ": " + m + " in «" + s.substr(0, 50) + "»\n";
        }
    if (!bad.empty()) std::fprintf(stderr, "the windows' own signs the font has not:\n%s", bad.c_str());
    CHECK(bad.empty());
}
