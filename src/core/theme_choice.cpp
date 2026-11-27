#include "core/theme_choice.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "core/ui_theme.hpp"

namespace uagfx::themechoice {
namespace {

std::string hexOf(uint32_t rgb, bool upper) {
    char b[8];
    std::snprintf(b, sizeof b, upper ? "#%06X" : "#%06x", unsigned(rgb & 0xFFFFFF));
    return b;
}

}  // namespace

bool swatchRow(const std::string& key) { return key == "interface.theme"; }

std::vector<uint32_t> palette() {
    std::vector<uint32_t> out;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 10; ++col) out.push_back(uitheme::paletteColour(row, col));
    return out;
}

std::pair<std::string, std::string> pick(int row, int col) {
    return {"own", hexOf(uitheme::paletteColour(row, col), true)};
}

std::string typed(const std::string& text) {
    std::string s;
    for (char c : text)
        if (!std::isspace(static_cast<unsigned char>(c))) s += c;
    if (!s.empty() && s[0] != '#') s = "#" + s;
    if (s.size() != 7) return "";
    for (size_t i = 1; i < 7; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return "";
        s[i] = char(std::toupper(static_cast<unsigned char>(s[i])));
    }
    return s;
}

std::string dot(const std::string& word, const std::string& ownColour) {
    if (word == "own") {
        const std::string own = typed(ownColour);
        return own.empty() ? "" : hexOf(uitheme::accentOf("own", own), false);
    }
    const uint32_t c = uitheme::accentOf(word, "");
    return c ? hexOf(c, false) : "";
}

std::vector<settings::IniEdit> ownEdits(const std::string& hex) {
    return {{"interface", "accent", "own"}, {"interface", "accent_colour", hex}};
}

uint32_t bgr(const std::string& hex) {
    const std::string c = typed(hex);
    if (c.empty()) return 0;
    const uint32_t rgb = uint32_t(std::strtoul(c.c_str() + 1, nullptr, 16));
    return (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF);
}

std::string fromBgr(uint32_t colorRef) {
    return hexOf((colorRef & 0xFF) << 16 | (colorRef & 0xFF00) | (colorRef >> 16 & 0xFF), true);
}

}  // namespace uagfx::themechoice
