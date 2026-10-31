#include "core/npc_talk.hpp"

#include <algorithm>
#include <cmath>

#include "core/pad_glyphs.hpp"
#include "core/ui_style.hpp"

namespace uagfx::npctalk {
namespace {

// The concept's sizes at scale 1: 860 wide, the title 40, a line of the words 22 at 14 px, an option 34 high
constexpr float kW = 860, kTitleH = 40, kSide = 18, kLineH = 22, kTextPx = 14, kOptionH = 34, kOptionGap = 8;
constexpr float kFooterH = 34, kButtonW = 120, kButtonH = 30, kGlyph = 16;
constexpr size_t kMostLines = 12;
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

std::vector<PadItem> padItems(const Talk& t, const Texts& tx) {
    if (!t.options.empty())
        return {{padglyph::Button::Down, tx.choose}, {padglyph::Button::A, tx.ok}, {padglyph::Button::B, tx.close}};
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
    if (!t.lines.empty() && t.lines[0].size() > 2 && t.lines[0].front() == '[' && t.lines[0].back() == ']') {
        t.name = t.lines[0].substr(1, t.lines[0].size() - 2);
        t.lines.erase(t.lines.begin());
    }
    return t;
}

Layout layout(int centreX, int bottom, float scale, const Talk& t, const Show& sh, const Texts& tx, const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s, px = kTextPx * s;
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

void paint(panel::Picture& out, const Layout& L, const Talk& t, const Show& sh) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 12 * s, kCard, uistyle::window(), kWhite, 0.09f, 1.0f * s);
    if (sh.hoverClose) panel::roundRect(out, L.close, 7 * s, kWhite, 0.12f);
    panel::roundRect(out, panel::Box{kSide * s, L.title.h, L.glass.w - 2 * kSide * s, 1 * s}, 0, kWhite, 0.07f);
    for (size_t i = 0; i < L.options.size(); ++i) {
        const panel::Box& b = L.options[i];
        const bool on = int(i) == t.selected, hover = int(i) == sh.hoverOption;
        panel::roundRect(out, b, 8 * s, on ? kGold2 : kWhite, on ? 0.16f : hover ? 0.10f : 0.05f, on ? kGold : kWhite,
                         on ? 0.85f : 0.06f, 1.0f * s);
        const panel::Box chip{b.x + 6 * s, b.y + (b.h - 22 * s) / 2, 22 * s, 22 * s};
        panel::roundRect(out, chip, 6 * s, kTile, 0.95f, kWhite, 0.10f, 1.0f * s);
    }
    if (L.button.w > 0)
        panel::roundRect(out, L.button, 8 * s, kGold, sh.hoverButton ? 0.32f : 0.20f, kGold, 0.8f, 1.0f * s);
    panel::roundRect(out, panel::Box{0, L.footer.y, L.glass.w, 1 * s}, 0, kWhite, 0.07f);
    if (sh.pad) {
        const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
        const std::vector<PadItem> items = padItems(t, Texts{});
        for (size_t i = 0; i < items.size() && i < L.padGlyphs.size(); ++i)
            padglyph::paint(out, items[i].button, f, L.padGlyphs[i].x, L.padGlyphs[i].y, kGlyph * s);
    }
}

std::vector<Words> words(const Layout& L, const Talk& t, const Show& sh, const Texts& tx, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s, side = kSide * s;
    const float nameW = t.name.empty() ? 0 : m(t.name, 15 * s, true) + 4;
    if (nameW > 0)
        out.push_back(Words{panel::Box{side, 0, nameW, L.title.h}, t.name, kText, 15 * s, true, Align::Left});
    if (!t.place.empty())
        out.push_back(Words{panel::Box{side + nameW + (nameW > 0 ? 10 * s : 0), 0, m(t.place, 11.5f * s, false) + 4,
                                       L.title.h},
                            t.place, kFaint, 11.5f * s, false, Align::Left});
    out.push_back(Words{L.close, "\xC3\x97", sh.hoverClose ? kText : kMuted, 16 * s, false, Align::Center});
    for (const Placed& p : L.lines) out.push_back(Words{p.box, p.text, p.rgb, kTextPx * s, false, Align::Left});
    for (size_t i = 0; i < L.options.size() && i < t.options.size(); ++i) {
        const panel::Box& b = L.options[i];
        out.push_back(Words{panel::Box{b.x + 6 * s, b.y, 22 * s, b.h}, std::to_string(i + 1),
                            int(i) == t.selected ? kGold : kMuted, 12 * s, true, Align::Center});
        const std::vector<Run> runs = colours(t.options[i], kText);
        std::string plain;
        for (const Run& r : runs) plain += r.text;
        out.push_back(Words{panel::Box{b.x + 36 * s, b.y, b.w - 42 * s, b.h}, plain, runs.front().rgb, 13 * s,
                            int(i) == t.selected, Align::Left});
    }
    if (L.button.w > 0)
        out.push_back(Words{L.button, t.button == Button::Next ? tx.next : tx.close, kText, 13 * s, true,
                            Align::Center});
    const float serverW = m(tx.server, 11 * s, false) + 4;
    out.push_back(Words{panel::Box{L.footer.w - side - serverW, L.footer.y, serverW, L.footer.h}, tx.server, kFaint,
                        11 * s, false, Align::Right});
    if (!sh.pad) {
        out.push_back(Words{panel::Box{side, L.footer.y, L.footer.w - 2 * side - serverW, L.footer.h}, tx.keys, kFaint,
                            11.5f * s, false, Align::Left});
        return out;
    }
    const padglyph::Family f = sh.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
    const std::vector<PadItem> items = padItems(t, tx);
    for (size_t i = 0; i < items.size() && i < L.padGlyphs.size(); ++i) {
        const padglyph::Letter l = padglyph::letter(items[i].button, f, L.padGlyphs[i].x, L.padGlyphs[i].y,
                                                    kGlyph * s);
        if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        out.push_back(Words{L.padWords[i], items[i].words, kText, 11.5f * s, false, Align::Left});
    }
    return out;
}

}  // namespace uagfx::npctalk
