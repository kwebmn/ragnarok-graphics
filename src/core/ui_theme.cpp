#include "core/ui_theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace uagfx::uitheme {
namespace {

struct Rgb {
    float r, g, b;
};

Rgb of(uint32_t c) { return {float((c >> 16) & 0xFF) / 255, float((c >> 8) & 0xFF) / 255, float(c & 0xFF) / 255}; }
uint32_t to(Rgb c) {
    const auto ch = [](float v) { return uint32_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255)); };
    return ch(c.r) << 16 | ch(c.g) << 8 | ch(c.b);
}
float lum(Rgb c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }
float chroma(Rgb c) { return std::max({c.r, c.g, c.b}) - std::min({c.r, c.g, c.b}); }

std::string trimmed(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

bool hex(const std::string& s, uint32_t& out) {
    if (s.size() != 7 || s[0] != '#') return false;
    char* end = nullptr;
    out = uint32_t(std::strtoul(s.c_str() + 1, &end, 16));
    return end && *end == 0;
}

}  // namespace

Theme named(const std::string& name) {
    Theme t;
    if (name == "classic") {  // the old game's look: warm browns and cream, square-ish corners
        t.name = name;
        t.greys = Greys::Warm;
        t.radius = 0.3f;
    } else if (name == "contrast") {  // darker grounds, brighter words, bolder edges
        t.name = name;
        t.greys = Greys::Spread;
        t.edge = 1.6f;
    }
    return t;
}

uint32_t colour(const Theme& t, uint32_t rgb) {
    rgb &= 0xFFFFFF;
    const auto it = t.exact.find(rgb);
    if (it != t.exact.end()) return it->second;
    if (t.greys == Greys::Keep) return rgb;
    const Rgb c = of(rgb);
    if (chroma(c) > 0.18f) return rgb;  // a colour that means something (HP, SP, gold, warnings) stays
    const float l = lum(c);
    if (t.greys == Greys::Warm) {  // a warm tint of the same brightness
        const Rgb tint = l < 0.5f ? Rgb{1.30f, 1.0f, 0.72f} : Rgb{1.04f, 1.0f, 0.90f};
        const float k = l / lum(tint);
        return to({tint.r * k, tint.g * k, tint.b * k});
    }
    const float m = l < 0.5f ? l * 0.55f : 1 - (1 - l) * 0.45f;  // Spread: the darks darker, the lights lighter
    const float k = l > 0 ? m / l : 0;
    return to({c.r * k, c.g * k, c.b * k});
}

void read(const std::string& text, Theme& t, std::string& bad) {
    bad.clear();
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trimmed(line);
        if (line.empty() || (line[0] == '#' && line.find('=') == std::string::npos)) continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            if (bad.empty()) bad = line;
            continue;
        }
        const std::string key = trimmed(line.substr(0, eq)), value = trimmed(line.substr(eq + 1));
        uint32_t from = 0, to = 0;
        if (hex(key, from) && hex(value, to)) {
            t.exact[from] = to;
        } else if (key == "radius" || key == "edge") {
            char* end = nullptr;
            const float v = std::strtof(value.c_str(), &end);
            if (end && *end == 0 && v >= 0 && v <= 4) (key == "radius" ? t.radius : t.edge) = v;
            else if (bad.empty()) bad = line;
        } else if (bad.empty()) {
            bad = line;
        }
    }
}

}  // namespace uagfx::uitheme
