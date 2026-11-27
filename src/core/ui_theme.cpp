#include "core/ui_theme.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <regex>
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

bool hex(const std::string& s, uint32_t& out) {  // "#RRGGBB" exactly
    if (s.size() != 7 || s[0] != '#') return false;
    for (size_t i = 1; i < 7; ++i)
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    out = uint32_t(std::strtoul(s.c_str() + 1, nullptr, 16));
    return true;
}

constexpr uint32_t kGold = 0xE6CB8A, kBlue = 0x2F5FD0, kInk = 0x181C26;
constexpr uint32_t kNoWord = 0xFFFFFFFFu;
// the light ground's colours: no lighter than this, to be read on it (3.0.24-beta.33, R-8BCJAH: 0.38, was 0.45 - the
// chat's yellow and green faint on the light glass)
constexpr float kReadable = 0.38f;

float hue(Rgb c) {  // degrees
    const float mx = std::max({c.r, c.g, c.b}), d = mx - std::min({c.r, c.g, c.b});
    if (d <= 0) return 0;
    float h = mx == c.r ? (c.g - c.b) / d : mx == c.g ? 2 + (c.b - c.r) / d : 4 + (c.r - c.g) / d;
    h *= 60;
    return h < 0 ? h + 360 : h;
}

// The gold the windows are drawn with (#e6cb8a, #c9a55b, #f3c653...): not the oranges (warnings), not the browns
bool gold(Rgb c) {
    const float mx = std::max({c.r, c.g, c.b});
    if (mx < 0.3f || chroma(c) / mx <= 0.25f) return false;
    const float h = hue(c);
    return h >= 36 && h <= 50;
}

// A gold in the accent: as much lighter or darker than the accent as it is than the windows' gold
uint32_t accented(Rgb c, uint32_t accent) {
    const Rgb a = of(accent);
    const float la = lum(a), k = lum(c) / lum(of(kGold)), want = la * k;
    if (want <= la || la >= 1) return to({a.r * k, a.g * k, a.b * k});
    const float w = std::clamp((want - la) / (1 - la), 0.0f, 1.0f);  // lighter: toward white
    return to({a.r + (1 - a.r) * w, a.g + (1 - a.g) * w, a.b + (1 - a.b) * w});
}

// The spec's light: the windows' and cards' ground #f6f8fc, the cells white (and the dark insets), the words #181c26
const std::unordered_map<uint32_t, uint32_t>& lightAnchors() {
    static const std::unordered_map<uint32_t, uint32_t> m = {
        {0x10141C, 0xF6F8FC}, {0x161B22, 0xF6F8FC}, {0x1B1D24, 0xFFFFFF}, {0x000000, 0xFFFFFF},
        {0xFFFFFF, kInk},     {0xEEF2F8, kInk},     {0xECEFF5, kInk}};
    return m;
}

// The text's colours through the theme; `word` (not kNoWord): the grey words' («color:») colour
std::string recoloured(const Theme& t, const std::string& text, uint32_t word) {
    static const std::regex hexes("#([0-9a-fA-F]{6})([0-9a-fA-F]{2})?\\b");
    std::string out;
    out.reserve(text.size());
    auto at = text.cbegin();
    for (std::sregex_iterator it(text.begin(), text.end(), hexes), end; it != end; ++it) {
        const std::smatch& m = *it;
        out.append(at, m[0].first);
        // 3.0.24-beta.30: a shadow's colour stays (ua-shadow(...)'s black would come out white in the light theme)
        const size_t pos = size_t(m.position(0)), shadow = text.rfind("ua-shadow(", pos);
        if (shadow != std::string::npos && text.find(')', shadow) > pos) {
            out += m[0].str();
            at = m[0].second;
            continue;
        }
        const uint32_t rgb = uint32_t(std::strtoul(m[1].str().c_str(), nullptr, 16));
        uint32_t c = colour(t, rgb);
        if (word != kNoWord && chroma(of(rgb)) <= 0.18f) {
            size_t k = size_t(m.position(0));  // the property before: «color:» alone (not «background-color:»)
            while (k > 0 && (text[k - 1] == ' ' || text[k - 1] == '\t')) --k;
            if (k > 0 && text[k - 1] == ':') {
                size_t e = k - 1;
                while (e > 0 && text[e - 1] == ' ') --e;
                size_t s = e;
                while (s > 0 && (std::isalnum(static_cast<unsigned char>(text[s - 1])) || text[s - 1] == '-')) --s;
                if (text.compare(s, e - s, "color") == 0) c = word;
            }
        }
        char b[16];
        std::snprintf(b, sizeof b, "#%06x", unsigned(c));
        out += b;
        std::string alpha = m[2].str();  // 3.0.24-beta.33: a light ground less see-through (groundAlpha)
        if (!alpha.empty()) {
            const float a = float(std::strtoul(alpha.c_str(), nullptr, 16)) / 255.0f, lifted = groundAlpha(t, rgb, a);
            if (lifted != a) {
                char ab[4];
                std::snprintf(ab, sizeof ab, "%02x", int(std::lround(lifted * 255)) & 0xFF);
                alpha = ab;
            }
        }
        out += alpha;
        at = m[0].second;
    }
    out.append(at, text.cend());
    return out;
}

// A rule's body on a solid ground (its «background-color:» 60 % or more; a faint one is a tint of the window's
// ground): the accent's gold, or (3.0.24-beta.30) in the light theme a colour that means something - a red or green
// button: its grey words light or dark by that ground as the theme draws it; otherwise kNoWord
uint32_t wordOnGround(const Theme& t, const std::string& body) {
    if (t.greys == Greys::Keep && !t.accent) return kNoWord;
    static const std::regex ground("(^|[^-a-z])background-color:\\s*#([0-9a-fA-F]{6})([0-9a-fA-F]{2})?\\b");
    std::smatch m;
    if (!std::regex_search(body, m, ground)) return kNoWord;
    if (m[3].matched && std::strtoul(m[3].str().c_str(), nullptr, 16) < 0x99) return kNoWord;
    const uint32_t rgb = uint32_t(std::strtoul(m[2].str().c_str(), nullptr, 16));
    const Rgb c = of(rgb);
    if (t.accent && gold(c)) return lum(of(accented(c, t.accent))) > 0.55f ? kInk : 0xFFFFFF;
    if (t.greys == Greys::Keep || chroma(c) <= 0.18f) return kNoWord;  // dark: as drawn; a grey ground: the greys'
    return lum(of(colour(t, rgb))) > 0.55f ? kInk : 0xFFFFFF;
}

}  // namespace

Theme named(const std::string& name) {
    if (name != "light") return Theme{};
    Look look;
    look.tone = Tone::Light;
    return themeFor(look, Group::Windows);
}

Theme themeFor(const Look& look, Group g) {
    const GroupTone own = look.groups[size_t(g)];
    const Tone tone = own == GroupTone::Same ? look.tone : own == GroupTone::Light ? Tone::Light : Tone::Dark;
    Theme t;
    if (tone == Tone::Light) {
        t.name = "light";
        t.greys = Greys::Light;
        t.exact = lightAnchors();
    }
    const uint32_t accent = look.accent ? look.accent & 0xFFFFFF : tone == Tone::Light ? kBlue : 0;
    t.accent = accent == kGold ? 0 : accent;
    return t;
}

uint32_t colour(const Theme& t, uint32_t rgb) {
    rgb &= 0xFFFFFF;
    const auto it = t.exact.find(rgb);
    if (it != t.exact.end()) return it->second;
    const Rgb c = of(rgb);
    if (t.accent && gold(c)) return accented(c, t.accent);
    if (t.greys == Greys::Keep) return rgb;
    const float l = lum(c);
    if (chroma(c) > 0.18f) {  // a colour that means something (HP, SP, warnings) stays, dark enough to read
        if (l <= kReadable) return rgb;
        const float k = kReadable / l;
        return to({c.r * k, c.g * k, c.b * k});
    }
    // Light: the greys turned over (the dark grounds light, the light words dark), their tint kept
    const float m = std::clamp(0.976f - 1.097f * (l - 0.13f), 0.06f, 0.985f);
    return to({m + (c.r - l) * 0.8f, m + (c.g - l) * 0.8f, m + (c.b - l) * 0.8f});
}

float groundAlpha(const Theme& t, uint32_t rgb, float a) {
    if (t.greys == Greys::Keep || a <= 0 || a >= 1) return a;
    const Rgb c = of(rgb & 0xFFFFFF);
    if (chroma(c) > 0.18f || lum(c) > 0.3f) return a;
    return 1 - (1 - a) / 3;
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

std::string themedCss(const Theme& t, const std::string& css) {
    if (t.greys == Greys::Keep && t.exact.empty() && !t.accent && t.radius == 1 && t.edge == 1) return css;
    std::string out;
    out.reserve(css.size());
    size_t i = 0;  // each innermost rule's body on its own (a word on the accent's ground), the rest as it comes
    while (i < css.size()) {
        const size_t open = css.find('{', i);
        const size_t close = open == std::string::npos ? std::string::npos : css.find('}', open);
        if (close == std::string::npos) {
            out += recoloured(t, css.substr(i), kNoWord);
            break;
        }
        const size_t inner = css.find('{', open + 1);
        if (inner != std::string::npos && inner < close) {  // @media, @keyframes: their rules inside
            out += recoloured(t, css.substr(i, inner - i), kNoWord);
            i = inner;
            continue;
        }
        const std::string body = css.substr(open + 1, close - open - 1);
        out += recoloured(t, css.substr(i, open + 1 - i), kNoWord);
        out += recoloured(t, body, wordOnGround(t, body));
        out += '}';
        i = close + 1;
    }
    const auto scaled = [](const std::string& text, const std::regex& re, float k) {
        if (k == 1) return text;
        std::string r;
        auto from = text.cbegin();
        for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
            const std::smatch& m = *it;
            r.append(from, m[2].first);
            char b[24];
            std::snprintf(b, sizeof b, "%g", double(std::strtof(m[2].str().c_str(), nullptr) * k));
            r += b;
            from = m[2].second;
        }
        r.append(from, text.cend());
        return r;
    };
    out = scaled(out, std::regex("(border-radius:\\s*)([0-9.]+)"), t.radius);
    out = scaled(out, std::regex("(border(?:-width)?:\\s*)([0-9.]+)"), t.edge);
    return out;
}

Group groupOf(const std::string& docName) {
    // the names uaUI's documents are made with (uaui::Doc::make, uisheet::Sheet::frame, itemgridui::Ui::frame)
    static const std::unordered_map<std::string, Group> groups = [] {
        std::unordered_map<std::string, Group> m;
        for (const char* n : {"buffs", "charblock", "exp bars", "exp gain", "hotbar", "hunt", "item toast",
                              "low hp edge", "minimap", "mvp", "recap", "target", "threat", "tracker", "cursor",
                              "big map", "big map cross", "skill wheel", "pad panel ring", "window size grip", "cast",
                              "ground items", "hud edit", "key hint", "loading bar", "mob card", "mob hp", "npc dots",
                              "npc tag", "pad carry", "pad hint", "pad tiles", "signs", "merc block", "enter veil",
                              "showcase", "party"})
            m[n] = Group::Hud;
        for (const char* n : {"chat", "chat wheel", "emote", "emotes", "commands", "whisper setup", "chat room setup",
                              "room", "answer", "macro", "whisper"})
            m[n] = Group::Chat;
        for (const char* n : {"items", "equip", "storage", "cart", "ask", "pick"}) m[n] = Group::Items;
        for (const char* n : {"shop", "vend shop", "vend setup", "my shop", "trade"}) m[n] = Group::Trade;
        m["options"] = Group::Settings;
        return m;
    }();
    const auto it = groups.find(docName);
    return it == groups.end() ? Group::Windows : it->second;
}

const char* groupWord(Group g) {
    switch (g) {
        case Group::Hud: return "hud";
        case Group::Chat: return "chat";
        case Group::Items: return "items";
        case Group::Trade: return "trade";
        case Group::Settings: return "settings";
        case Group::Windows: break;
    }
    return "windows";
}

uint32_t accentOf(const std::string& word, const std::string& colour) {
    if (word == "gold") return kGold;
    if (word == "blue") return kBlue;
    if (word == "green") return 0x2E8B57;
    if (word == "red") return 0xC0392B;
    uint32_t c = 0;
    return word == "own" && hex(trimmed(colour), c) ? c : 0;
}

uint32_t paletteColour(int row, int col) {  // HSL: the hue by the column, the lightness by the row
    static constexpr float kLight[3] = {0.35f, 0.55f, 0.75f};
    const float h = float(std::clamp(col, 0, 9)) * 36, l = kLight[std::clamp(row, 0, 2)], s = 0.75f;
    const float c = (1 - std::fabs(2 * l - 1)) * s, x = c * (1 - std::fabs(std::fmod(h / 60, 2.0f) - 1)), m = l - c / 2;
    Rgb v{0, 0, 0};
    if (h < 60) v = {c, x, 0};
    else if (h < 120) v = {x, c, 0};
    else if (h < 180) v = {0, c, x};
    else if (h < 240) v = {0, x, c};
    else if (h < 300) v = {x, 0, c};
    else v = {c, 0, x};
    return to({v.r + m, v.g + m, v.b + m});
}

std::string themedLink(const std::string& rml, Group g) {
    static const std::string from = "href=\"uaui:theme.rcss\"";
    const std::string to = std::string("href=\"uaui:theme-") + groupWord(g) + ".rcss\"";
    std::string out = rml;
    for (size_t at = out.find(from); at != std::string::npos; at = out.find(from, at + to.size()))
        out.replace(at, from.size(), to);
    return out;
}

bool styleGroup(const std::string& path, Group& g) {
    if (path == "uaui:theme.rcss") {
        g = Group::Windows;
        return true;
    }
    static const std::string head = "uaui:theme-", foot = ".rcss";
    if (path.size() <= head.size() + foot.size() || path.compare(0, head.size(), head) != 0 ||
        path.compare(path.size() - foot.size(), foot.size(), foot) != 0)
        return false;
    const std::string word = path.substr(head.size(), path.size() - head.size() - foot.size());
    for (int i = 0; i < kGroups; ++i)
        if (word == groupWord(Group(i))) {
            g = Group(i);
            return true;
        }
    return false;
}

namespace {
std::array<Theme, kGroups>& activeThemes() {
    static std::array<Theme, kGroups> themes;
    return themes;
}
}  // namespace

void setActive(const std::array<Theme, kGroups>& themes) { activeThemes() = themes; }
const Theme& active(Group g) { return activeThemes()[size_t(g)]; }
uint32_t paint(uint32_t rgb, Group g) { return colour(active(g), rgb); }

}  // namespace uagfx::uitheme
