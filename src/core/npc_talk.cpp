#include "core/npc_talk.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/pad_glyphs.hpp"
#include "core/ui_style.hpp"

namespace uagfx::npctalk {
namespace {

// The concept's sizes at scale 1: 860 wide, the title 40, a line of the words 22 at 14 px, an option 34 high
constexpr float kW = 860, kTitleH = 40, kSide = 18, kLineH = 22, kTextPx = 14, kOptionH = 34, kOptionGap = 8;
constexpr float kFooterH = 34, kButtonW = 120, kButtonH = 30, kGlyph = 16;
constexpr size_t kMostLines = 12;
// 3.0.12-beta.2 (a player's wish: «зробити трохи менші ... вікна чатів з НПЦ»): the window as wide as its words, no
// less than kMinW, no more than kW; an option's room past its words (its number and edges)
constexpr float kMinW = 460, kOptionRoom = 56;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xF3C653, kGold2 = 0xC9A55B, kTile = 0x1B1D24;
constexpr uint32_t kText = 0xEEF2F8, kMuted = 0x9AA3B5, kFaint = 0x7E889A;

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

uint32_t readable(uint32_t rgb, uint32_t base) {
    const int r = int(rgb >> 16 & 0xFF), g = int(rgb >> 8 & 0xFF), b = int(rgb & 0xFF);
    const int luma = (r * 299 + g * 587 + b * 114) / 1000;
    if (std::max({r, g, b}) < 0x30) return base;  // black: the game's window was white
    if (luma >= 0x40) return rgb;
    const auto lift = [](int v) { return uint32_t(v + (255 - v) * 45 / 100); };  // dark: lit toward white
    return lift(r) << 16 | lift(g) << 8 | lift(b);
}

struct PadItem {
    padglyph::Button button;
    std::string words;
};

std::vector<PadItem> padItems(const Talk& t, const Texts& tx) {  // 3.0.11-beta.255: A first, its wait drawn on it
    if (!t.options.empty())
        return {{padglyph::Button::A, tx.ok}, {padglyph::Button::Down, tx.choose}, {padglyph::Button::B, tx.close}};
    if (t.button == Button::Next) return {{padglyph::Button::A, tx.next}, {padglyph::Button::B, tx.close}};
    if (t.button == Button::Close) return {{padglyph::Button::A, tx.close}};
    return {{padglyph::Button::B, tx.close}};
}

}  // namespace

std::vector<Run> colours(const std::string& utf8, uint32_t base) {
    std::vector<Run> out;
    uint32_t now = base;
    std::string run;
    for (size_t i = 0; i < utf8.size();) {
        if (utf8[i] == '^' && i + 6 < utf8.size() + 0 && i + 7 <= utf8.size()) {
            uint32_t v = 0;
            bool ok = true;
            for (size_t k = 1; k <= 6 && ok; ++k) {
                const int h = hex(utf8[i + k]);
                ok = h >= 0;
                v = v << 4 | uint32_t(h < 0 ? 0 : h);
            }
            if (ok) {
                if (!run.empty()) out.push_back(Run{run, now});
                run.clear();
                now = readable(v, base);
                i += 7;
                continue;
            }
        }
        run += utf8[i++];
    }
    if (!run.empty() || out.empty()) out.push_back(Run{run, now});
    return out;
}

Talk named(Talk t) {
    if (t.lines.empty()) return t;
    std::string plain;  // 3.0.11-beta.117 (R-2YRWU2): the line's words without its ^RRGGBB colours
    for (const Run& r : colours(t.lines[0], 0)) plain += r.text;
    const size_t from = plain.find_first_not_of(' '), to = plain.find_last_not_of(' ');
    plain = from == std::string::npos ? std::string() : plain.substr(from, to - from + 1);
    if (plain.size() > 2 && plain.front() == '[' && plain.back() == ']') {
        t.name = plain.substr(1, plain.size() - 2);
        t.lines.erase(t.lines.begin());
    }
    return t;
}

Layout layout(int centreX, int bottom, float scale, const Talk& t, const Show& sh, const Texts& tx, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float side = kSide * s, px = kTextPx * s;
    float need = 0;  // 3.0.12-beta.2: the widest line, the options' two columns
    for (const std::string& line : t.lines) {
        float lw = 0;
        for (const Run& r : colours(line, kText)) lw += m(r.text, px, false);
        need = std::max(need, lw + 2 * side + 4 * s);
    }
    float option = 0;
    for (const std::string& o : t.options) option = std::max(option, m(o, px, false) + kOptionRoom * s);
    if (!t.options.empty())
        need = std::max(need, 2 * side + (t.options.size() > 1 ? 2 * option + kOptionGap * s : option));
    const float w = std::clamp(std::ceil(need), kMinW * s, kW * s);
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.close = panel::Box{w - side - 24 * s, (kTitleH * s - 24 * s) / 2, 24 * s, 24 * s};
    // the words wrapped at the window's width, word by word, each colour's run apart
    const float textW = w - 2 * side;
    std::vector<std::vector<Placed>> rows;
    for (const std::string& line : t.lines) {
        rows.emplace_back();
        float x = 0;
        for (const Run& r : colours(line, kText)) {
            size_t from = 0;
            while (from < r.text.size()) {
                size_t end = r.text.find(' ', from);
                end = end == std::string::npos ? r.text.size() : end + 1;
                const std::string word = r.text.substr(from, end - from);
                if (x == 0 && word.find_first_not_of(' ') == std::string::npos) {  // 3.0.12-beta.3: no lead spaces
                    from = end;
                    continue;
                }
                const float ww = m(word, px, false);
                if (x > 0 && x + ww > textW) {
                    rows.emplace_back();
                    x = 0;
                }
                std::vector<Placed>& row = rows.back();
                if (!row.empty() && row.back().rgb == r.rgb && row.back().box.x + row.back().box.w >= x - 0.5f) {
                    row.back().text += word;  // the same colour's words one run
                    row.back().box.w = x + ww - row.back().box.x;
                } else {
                    row.push_back(Placed{panel::Box{x, 0, ww, kLineH * s}, word, r.rgb});
                }
                x += ww;
                from = end;
            }
        }
    }
    if (rows.size() > kMostLines) rows.erase(rows.begin(), rows.end() - std::ptrdiff_t(kMostLines));  // the last ones
    const float bodyY = kTitleH * s + 4 * s;
    for (size_t i = 0; i < rows.size(); ++i)
        for (Placed p : rows[i]) {
            p.box.x += side;
            p.box.y = bodyY + float(i) * kLineH * s;
            L.lines.push_back(p);
        }
    float y = bodyY + float(rows.size()) * kLineH * s + 10 * s;
    L.body = panel::Box{side, bodyY, textW, y - bodyY};
    if (!t.options.empty()) {  // two columns, row by row: 1 2 / 3 4
        const float ow = (textW - kOptionGap * s) / 2;
        for (size_t i = 0; i < t.options.size(); ++i) {
            const float ox = side + float(i % 2) * (ow + kOptionGap * s);
            const float oy = y + float(i / 2) * (kOptionH + 6) * s;
            L.options.push_back(panel::Box{ox, oy, ow, kOptionH * s});
        }
        y += float((t.options.size() + 1) / 2) * (kOptionH + 6) * s + 4 * s;
    } else if (t.button != Button::None) {
        L.button = panel::Box{w - side - kButtonW * s, y, kButtonW * s, kButtonH * s};
        y += kButtonH * s + 8 * s;
    }
    L.footer = panel::Box{0, y, w, kFooterH * s};
    if (sh.pad) {  // the controller's buttons at the footer's left
        float x = side;
        for (const PadItem& it : padItems(t, tx)) {
            const float gw = padglyph::widthOf(it.button, kGlyph * s);
            L.padGlyphs.push_back(panel::Box{x, L.footer.y + (L.footer.h - kGlyph * s) / 2, gw, kGlyph * s});
            x += gw + 6 * s;
            const float ww = m(it.words, 11.5f * s, false) + 2;
            L.padWords.push_back(panel::Box{x, L.footer.y, ww, L.footer.h});
            x += ww + 14 * s;
        }
    }
    L.glass = panel::Box{0, 0, w, L.footer.y + L.footer.h};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(L.glass.h));
    L.x = centreX - L.w / 2;
    L.y = bottom - L.h;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (L.close.contains(x, y)) return Hit{Part::Close, -1};
    if (L.button.w > 0 && L.button.contains(x, y)) return Hit{Part::Button, -1};
    for (size_t i = 0; i < L.options.size(); ++i)
        if (L.options[i].contains(x, y)) return Hit{Part::Option, int(i)};
    if (y < L.title.y + L.title.h) return Hit{Part::Title, -1};
    return Hit{Part::Body, -1};
}

int moved(int selected, int count, Move m) {
    if (count <= 0) return -1;
    const int at = std::clamp(selected, 0, count - 1);
    int to = at;
    switch (m) {
        case Move::Up: to = at - 2; break;
        case Move::Down: to = at + 2 < count ? at + 2 : (at % 2 == 1 && at + 1 < count ? at + 1 : at); break;
        case Move::Left: to = at % 2 == 1 ? at - 1 : at; break;
        case Move::Right: to = at % 2 == 0 && at + 1 < count ? at + 1 : at; break;
        case Move::PageUp: to = at - kPage; break;
        case Move::PageDown: to = at + kPage; break;
    }
    if (m == Move::Up && to < 0) return at;
    return std::clamp(to, 0, count - 1);
}

float waiting(uint64_t changedMs, uint64_t nowMs) {
    if (!changedMs || nowMs >= changedMs + kGuardMs) return 0;
    if (nowMs <= changedMs) return 1;
    return 1.0f - float(nowMs - changedMs) / float(kGuardMs);
}

std::string menuKey(const Talk& t) {
    std::string key = t.name + '\x1F' + t.place;
    for (const std::string& o : t.options) key += '\x1F' + o;
    for (char& c : key)
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    return key;
}

std::map<std::string, int> readChoices(const std::string& text) {
    std::map<std::string, int> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        const size_t tab = line.find('\t');
        if (tab != std::string::npos && tab > 0) {
            int index = -1;
            if (std::sscanf(line.c_str(), "%d", &index) == 1 && index >= 0 && index < 1000)
                out[line.substr(tab + 1)] = index;
        }
        at = end + 1;
    }
    return out;
}

std::string writeChoices(const std::map<std::string, int>& choices) {
    std::string out;
    for (const auto& [key, index] : choices) out += std::to_string(index) + '\t' + key + '\n';
    return out;
}

}  // namespace uagfx::npctalk
