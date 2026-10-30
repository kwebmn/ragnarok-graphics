#include "core/chat_panel.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::chatpanel {
namespace {

// The concept's sizes at scale 1 (v23 chat): 520 wide, the tabs' row 30, a line's row 18 (its words 12.5), the
// input's row 28; the glass 86 % (the concept: chat and windows not under 80 %), idle 55 %
constexpr float kW = 520, kTabsH = 30, kRowH = 18, kInputH = 28, kPad = 12;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kGold = 0xF3C653, kAccent = 0x5DB2FF;
constexpr uint32_t kOnBadge = 0x08111F, kHoverTab = 0xC9D1DE;

size_t utf8Next(const std::string& s, size_t i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    return std::min(s.size(), i + n);
}

// Small letters of the Latin and Cyrillic ones (UTF-8): what tradeLike looks for
std::string lowerUtf8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out += char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            ++i;
            continue;
        }
        const size_t n = utf8Next(s, i) - i;
        if (n == 2) {
            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            if (c == 0xD0 && d >= 0x90 && d <= 0x9F) {  // А-П -> а-п
                out += char(0xD0);
                out += char(d + 0x20);
            } else if (c == 0xD0 && d >= 0xA0 && d <= 0xAF) {  // Р-Я -> р-я
                out += char(0xD1);
                out += char(d - 0x20);
            } else if (c == 0xD0 && d >= 0x80 && d <= 0x8F) {  // Ѐ-Џ (Є, І, Ї) -> ѐ-џ
                out += char(0xD1);
                out += char(d + 0x10);
            } else if (c == 0xD2 && d == 0x90) {  // Ґ
                out += char(0xD2);
                out += char(0x91);
            } else {
                out.append(s, i, n);
            }
        } else {
            out.append(s, i, n);
        }
        i += n;
    }
    return out;
}

std::string named(const std::string& form, const std::string& name, std::string& before, std::string& after) {
    const size_t at = form.find("{1}");
    if (at == std::string::npos) {
        before = form + " ";
        after.clear();
    } else {
        before = form.substr(0, at);
        after = form.substr(at + 3);
    }
    return before + name + after;
}

// «Name : words», the server's way: its name and words; false - not such
bool said(const std::string& text, std::string& who, std::string& rest) {
    const size_t at = text.find(" : ");
    if (at == std::string::npos || at == 0 || at > 48) return false;
    who = text.substr(0, at);
    rest = text.substr(at + 3);
    return true;
}

// A whisper's «( From NAME ) : words» or «( To NAME ) : words»
bool whispered(const std::string& text, const char* head, std::string& who, std::string& rest) {
    const std::string h = head;
    if (text.compare(0, h.size(), h) != 0) return false;
    const size_t end = text.find(" ) : ", h.size());
    if (end == std::string::npos) return false;
    who = text.substr(h.size(), end - h.size());
    rest = text.substr(end + 5);
    return !who.empty();
}

uint32_t chipRgb(const Input& in) {
    if (!in.whisper.empty() || in.whisperFocus) return kWhisper;
    return in.mode == 1 ? kParty : in.mode == 2 ? kGuild : kText;
}

std::string chipText(const Input& in, const Texts& t) {
    if (!in.whisper.empty() || in.whisperFocus) return "\xE2\x86\x92 " + in.whisper;  // → Name
    return t.tabs[in.mode == 1 ? 1 : in.mode == 2 ? 2 : 0];
}

// A tab's count of new lines: its pill right of its name; w 0 none
panel::Box badgeBox(const Layout& L, int i, const Show& sh, const Texts& t, const Measure& m) {
    if (L.quiet || i < 0 || i >= int(L.tabs.size()) || sh.unread[i] <= 0 || i == int(sh.tab)) return panel::Box{};
    const float s = L.s;
    const panel::Box& b = L.tabs[size_t(i)];
    const float lw = std::ceil(m(t.tabs[i], 12 * s, true)) + 2;
    const float bw = m(std::to_string(std::min(sh.unread[i], 99)), 10 * s, true) + 10 * s;
    return panel::Box{b.x + 10 * s + lw + 4 * s, b.y + (b.h - 3 * s - 15 * s) / 2, bw, 15 * s};
}

}  // namespace

uint32_t gameRgb(uint32_t c) { return (c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF); }

bool tradeLike(const std::string& utf8) {
    const std::string s = lowerUtf8(utf8);
    static const char* const kWords[] = {
        "s>", "b>", "wts", "wtb", "wtt", "sell", "buy",
        "\xD0\xBF\xD1\x80\xD0\xBE\xD0\xB4\xD0\xB0\xD0\xBC",              // продам
        "\xD0\xBF\xD1\x80\xD0\xBE\xD0\xB4\xD0\xB0\xD1\x8E",              // продаю
        "\xD0\xBF\xD1\x80\xD0\xBE\xD0\xB4\xD0\xB0\xD0\xB6",              // продаж
        "\xD0\xBA\xD1\x83\xD0\xBF\xD0\xBB\xD1\x8E",                      // куплю
        "\xD0\xBA\xD1\x83\xD0\xBF\xD1\x83\xD1\x8E",                      // купую
        "\xD0\xBE\xD0\xB1\xD0\xBC\xD1\x96\xD0\xBD",                      // обмін
        "\xD0\xBE\xD0\xB1\xD0\xBC\xD0\xB5\xD0\xBD",                      // обмен
        "\xD0\xBC\xD1\x96\xD0\xBD\xD1\x8F\xD1\x8E",                      // міняю
        "\xD0\xBC\xD0\xB5\xD0\xBD\xD1\x8F\xD1\x8E"};                     // меняю
    for (const char* w : kWords)
        if (s.find(w) != std::string::npos) return true;
    return false;
}

Line fromGame(Source source, const std::string& text, uint32_t colorRef, const Texts& t, uint64_t ms) {
    Line l;
    l.ms = ms;
    const uint32_t own = colorRef == 0 || colorRef == 0xFFFFFF ? kText : gameRgb(colorRef);
    std::string who, rest, before, after;
    switch (source) {
        case Source::Public:
        case Source::PublicOwn:
            l.kind = Kind::Public;
            if (said(text, who, rest)) {  // a GM's in the game's colour, the rest as the words
                l.who = who;
                l.trade = tradeLike(rest);
                l.spans = {Span{who, own, true, true}, Span{": " + rest, own, false, false}};
                return l;
            }
            break;
        case Source::Party:
        case Source::Guild: {
            const bool party = source == Source::Party;
            const uint32_t c = party ? kParty : kGuild;
            l.kind = party ? Kind::Party : Kind::Guild;
            const std::string tag = "[" + t.tabs[party ? 1 : 2] + "] ";
            if (said(text, who, rest)) {
                l.who = who;
                l.spans = {Span{tag, c, true, false}, Span{who, c, true, true}, Span{": " + rest, kText, false, false}};
            } else {
                l.spans = {Span{tag, c, true, false}, Span{text, kText, false, false}};
            }
            return l;
        }
        case Source::Whisper:
        case Source::WhisperResult: {
            l.kind = Kind::Whisper;
            const bool in = whispered(text, "( From ", who, rest);
            if (in || whispered(text, "( To ", who, rest)) {
                l.who = who;
                named(in ? t.from : t.to, who, before, after);
                l.spans = {Span{"[" + before, kWhisper, true, false}, Span{who, kWhisper, true, true},
                           Span{after + "] ", kWhisper, true, false}, Span{rest, kWhisper, false, false}};
                return l;
            }
            l.spans = {Span{text, own == kText ? kWhisper : own, false, false}};
            return l;
        }
        case Source::Announce:
            l.kind = Kind::Announce;
            l.spans = {Span{text, own == kText ? kSystem : own, true, false}};
            return l;
        case Source::Other:
            break;
    }
    if (l.spans.empty()) {
        if (source != Source::Public && source != Source::PublicOwn) l.kind = Kind::System;
        l.spans = {Span{text, own, false, false}};
    }
    return l;
}

bool shows(Tab tab, const Line& line) {
    switch (tab) {
        case Tab::All: return line.kind != Kind::Battle;
        case Tab::Party: return line.kind == Kind::Party;
        case Tab::Guild: return line.kind == Kind::Guild;
        case Tab::Whisper: return line.kind == Kind::Whisper;
        case Tab::Trade: return line.kind == Kind::Public && line.trade;
        case Tab::Battle: return line.kind == Kind::Battle || line.kind == Kind::Loot;
    }
    return false;
}

int unreadOn(const Line& line) {
    return line.kind == Kind::Party ? 1 : line.kind == Kind::Guild ? 2 : line.kind == Kind::Whisper ? 3 : -1;
}

float estimate(const std::string& utf8, float px, bool bold) { return panel::wordsWidth(utf8, px, bold); }

std::vector<Row> wrap(const std::vector<const Line*>& lines, float width, float px, const Measure& m) {
    std::vector<Row> rows;
    const auto box = [&](const std::string& s, bool bold) { return std::ceil(m(s, px, bold)) + 1; };
    for (size_t li = 0; li < lines.size(); ++li) {
        Row row;
        float x = 0;
        const auto put = [&](const std::string& unit, const Span& sp) {
            if (!row.empty() && row.back().rgb == sp.rgb && row.back().bold == sp.bold && row.back().name == sp.name)
                row.back().text += unit;
            else
                row.push_back(Piece{unit, sp.rgb, sp.bold, sp.name, 0, 0, int(li)});
        };
        const auto next = [&]() {
            rows.push_back(row);
            row.clear();
            x = 0;
        };
        for (const Span& sp : lines[li]->spans) {
            size_t i = 0;
            while (i < sp.text.size()) {  // a word with its space after it
                const size_t sp1 = sp.text.find(' ', i);
                const size_t j = sp1 == std::string::npos ? sp.text.size() : sp1 + 1;
                std::string unit = sp.text.substr(i, j - i);
                i = j;
                const float uw = m(unit, px, sp.bold);
                std::string bare = unit;
                while (!bare.empty() && bare.back() == ' ') bare.pop_back();
                if (x > 0 && x + m(bare, px, sp.bold) > width) {
                    next();
                    if (unit == " ") continue;
                }
                if (uw > width) {  // longer than a row: by its letters
                    for (size_t k = 0; k < unit.size();) {
                        const size_t e = utf8Next(unit, k);
                        const std::string ch = unit.substr(k, e - k);
                        const float cw = m(ch, px, sp.bold);
                        if (x > 0 && x + cw > width) next();
                        put(ch, sp);
                        x += cw;
                        k = e;
                    }
                    continue;
                }
                put(unit, sp);
                x += uw;
            }
        }
        next();
    }
    for (Row& r : rows) {  // their places from their own words' widths: the boxes meet, never overlap
        float x = 0;
        for (Piece& p : r) {
            p.x = x;
            p.w = box(p.text, p.bold);
            x += p.w;
        }
    }
    return rows;
}

Layout layout(int left, int bottom, float scale, int rows, bool quiet, const Show& sh, const Input& in, const Texts& t,
              const Measure& m) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.quiet = quiet;
    L.px = 12.5f * s;
    L.rowH = kRowH * s;
    L.rows = quiet ? std::min(std::max(1, rows), 3) : std::max(1, rows);
    const float w = kW * s, pad = kPad * s;
    const float tabsH = quiet ? 0 : kTabsH * s, inputH = quiet ? 0 : kInputH * s;
    const float top = quiet ? 8 * s : tabsH + 4 * s;
    const float linesH = float(L.rows) * L.rowH;
    const float h = top + linesH + (quiet ? 8 * s : 6 * s + inputH + 8 * s);
    L.glass = panel::Box{0, 0, w, h};
    L.lines = panel::Box{pad, top, w - 2 * pad, linesH};
    if (!quiet) {
        L.tabsRow = panel::Box{0, 0, w, tabsH};
        float x = 8 * s;
        for (int i = 0; i < kTabs; ++i) {
            float tw = m(t.tabs[i], 12 * s, true) + 20 * s;
            if (sh.unread[i] > 0 && i != int(sh.tab))
                tw += m(std::to_string(std::min(sh.unread[i], 99)), 10 * s, true) + 14 * s;
            L.tabs.push_back(panel::Box{x, 4 * s, tw, tabsH - 4 * s});
            x += tw + 2 * s;
        }
        L.input = panel::Box{8 * s, h - 8 * s - inputH, w - 16 * s, inputH};
        const bool whisper = !in.whisper.empty() || in.whisperFocus;
        const float chipW = m(chipText(in, t), 12 * s, true) + (whisper ? 18 : 30) * s;
        L.mode = panel::Box{L.input.x + 4 * s, L.input.y + 4 * s, chipW, inputH - 8 * s};
        float fieldX = L.mode.x + L.mode.w + 8 * s;
        if (!in.whisper.empty()) {
            L.clear = panel::Box{L.mode.x + L.mode.w + 2 * s, L.mode.y, L.mode.h, L.mode.h};
            fieldX = L.clear.x + L.clear.w + 6 * s;
        }
        L.field = panel::Box{fieldX, L.input.y, L.input.x + L.input.w - 8 * s - fieldX, inputH};
        if (sh.scroll > 0)
            L.newer = panel::Box{L.lines.x + L.lines.w - 30 * s, L.lines.y + L.lines.h - 19 * s, 30 * s, 18 * s};
    }
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = left;
    L.y = bottom - L.h;
    return L;
}

void shown(const Layout& L, const std::vector<Row>& rows, int scroll, size_t& first, size_t& count) {
    const size_t n = rows.size();
    const size_t up = std::min(n, size_t(std::max(0, scroll)));
    const size_t end = n - up;
    count = std::min(end, size_t(std::max(0, L.rows)));
    first = end - count;
}

Hit hitTest(const Layout& L, const std::vector<Row>& rows, const Show& sh, float x, float y) {
    if (!L.glass.contains(x, y)) return Hit{};
    if (!L.quiet) {
        for (size_t i = 0; i < L.tabs.size(); ++i)
            if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, int(i)};
        if (L.newer.w > 0 && L.newer.contains(x, y)) return Hit{Part::Newer, -1};
        if (L.clear.w > 0 && L.clear.contains(x, y)) return Hit{Part::Clear, -1};
        if (L.mode.contains(x, y)) return Hit{Part::Mode, -1};
        if (L.input.contains(x, y)) return Hit{Part::Field, -1};
    }
    if (L.lines.contains(x, y)) {
        size_t first = 0, count = 0;
        shown(L, rows, sh.scroll, first, count);
        const float top = L.lines.y + L.lines.h - float(count) * L.rowH;
        if (y >= top) {
            const size_t r = first + size_t((y - top) / L.rowH);
            if (r < first + count)
                for (const Piece& p : rows[r])
                    if (p.name && x >= L.lines.x + p.x && x < L.lines.x + p.x + p.w) return Hit{Part::Name, p.line};
        }
        return Hit{Part::Lines, -1};
    }
    return Hit{Part::Body, -1};
}

std::string fieldText(const Layout& L, const Input& in, const Measure& m, float& caretX) {
    const float px = 12 * L.s, room = L.field.w - 4 * L.s;
    const size_t caret = std::min(in.caret, in.text.size());
    std::string shown = in.text;
    size_t start = 0;
    const std::string dots = "\xE2\x80\xA6";
    while (start < shown.size() && m((start ? dots : "") + shown.substr(start), px, false) > room) {
        if (start >= caret) break;  // the caret stays in sight
        start = utf8Next(shown, start);
    }
    const std::string prefix = start ? dots : "";
    caretX = m(prefix + shown.substr(start, caret - start), px, false);
    return prefix + shown.substr(start);
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Row>& rows, const Input& in, const Show& sh,
           const Texts& t, const Measure& m) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 10 * s, kCard, L.quiet ? 0.55f : uistyle::chat(), kWhite, 0.09f, 1.0f * s);
    if (!L.quiet) {
        for (size_t i = 0; i < L.tabs.size(); ++i) {
            const panel::Box& b = L.tabs[i];
            const bool on = int(i) == int(sh.tab);
            if (on || int(i) == sh.hoverTab) panel::roundRect(out, b, 7 * s, kWhite, on ? 0.07f : 0.04f);
            if (on) panel::roundRect(out, panel::Box{b.x + 9 * s, b.y + b.h - 3 * s, b.w - 18 * s, 2 * s}, 1 * s, kGold,
                                     0.95f);
            const panel::Box badge = badgeBox(L, int(i), sh, t, m);
            if (badge.w > 0) panel::roundRect(out, badge, badge.h / 2, kAccent, 0.95f);
        }
        panel::roundRect(out, panel::Box{0, L.tabsRow.h, L.glass.w, 1 * s}, 0, kWhite, 0.06f);
        panel::roundRect(out, L.input, 8 * s, kBlack, 0.38f, kWhite, 0.10f, 1.0f * s);
        panel::roundRect(out, L.mode, 6 * s, chipRgb(in), sh.hoverMode ? 0.28f : 0.16f);
        if (in.whisper.empty() && !in.whisperFocus) {  // its chevron: the channel goes round on a click
            const float icon = 11 * s;
            uiicons::draw(out, uiicons::Icon::Down, L.mode.x + L.mode.w - icon - 6 * s,
                          L.mode.y + (L.mode.h - icon) / 2, icon, chipRgb(in), 0.9f, 2.4f);
        }
        if (L.clear.w > 0 && sh.hoverClear) panel::roundRect(out, L.clear, 5 * s, kWhite, 0.14f);
        if (in.caretOn && in.whisperFocus) {  // the name's caret at its end
            const float x = L.mode.x + 8 * s + m(chipText(in, t), 12 * s, true) + 1 * s;
            panel::roundRect(out, panel::Box{x, L.mode.y + 4 * s, 1.5f * s, L.mode.h - 8 * s}, 0, kWhisper, 0.95f);
        } else if (in.caretOn && in.typing && !in.text.empty()) {  // the words' caret
            float caretX = 0;
            fieldText(L, in, m, caretX);
            panel::roundRect(out, panel::Box{L.field.x + caretX, L.field.y + 6 * s, 1.5f * s, L.field.h - 12 * s}, 0,
                             kText, 0.95f);
        }
        if (L.newer.w > 0) panel::roundRect(out, L.newer, 6 * s, kAccent, sh.hoverNewer ? 0.45f : 0.28f);
    }
    if (sh.hoverLine >= 0) {  // the name pointed at: underlined
        size_t first = 0, count = 0;
        shown(L, rows, sh.scroll, first, count);
        const float top = L.lines.y + L.lines.h - float(count) * L.rowH;
        for (size_t r = first; r < first + count; ++r)
            for (const Piece& p : rows[r])
                if (p.name && p.line == sh.hoverLine)
                    panel::roundRect(out,
                                     panel::Box{L.lines.x + p.x, top + float(r - first + 1) * L.rowH - 3 * s,
                                                p.w - 1, 1 * s},
                                     0, p.rgb, 0.8f);
    }
}

std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const Input& in, const Show& sh,
                         const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s;
    if (!L.quiet) {
        for (size_t i = 0; i < L.tabs.size(); ++i) {
            const panel::Box& b = L.tabs[i];
            const bool on = int(i) == int(sh.tab);
            const float lw = std::ceil(m(t.tabs[i], 12 * s, true)) + 2;
            out.push_back(Words{panel::Box{b.x + 10 * s, b.y, lw, b.h - 3 * s}, t.tabs[i],
                                on ? kText : int(i) == sh.hoverTab ? kHoverTab : kFaint, 12 * s, true, Align::Left});
            const panel::Box badge = badgeBox(L, int(i), sh, t, m);
            if (badge.w > 0)
                out.push_back(Words{badge, std::to_string(std::min(sh.unread[i], 99)), kOnBadge, 10 * s, true,
                                    Align::Center});
        }
    }
    size_t first = 0, count = 0;
    shown(L, rows, sh.scroll, first, count);
    const float top = L.lines.y + L.lines.h - float(count) * L.rowH;
    for (size_t r = first; r < first + count; ++r)
        for (const Piece& p : rows[r]) {
            std::string text = p.text;
            while (!text.empty() && text.back() == ' ') text.pop_back();
            if (text.empty()) continue;
            out.push_back(Words{panel::Box{L.lines.x + p.x, top + float(r - first) * L.rowH, p.w, L.rowH}, text,
                                p.rgb, L.px, p.bold, Align::Left});
        }
    if (L.quiet) return out;
    const std::string chip = chipText(in, t);
    out.push_back(Words{panel::Box{L.mode.x + 8 * s, L.mode.y, std::ceil(m(chip, 12 * s, true)) + 2, L.mode.h}, chip,
                        chipRgb(in), 12 * s, true, Align::Left});
    if (L.clear.w > 0)
        out.push_back(Words{L.clear, "\xC3\x97", sh.hoverClear ? kText : kMuted, 13 * s, true, Align::Center});  // ×
    if (L.newer.w > 0) out.push_back(Words{L.newer, t.newer, kText, 11 * s, true, Align::Center});
    if (in.text.empty()) {
        const std::string& hint = in.battle && !in.typing ? t.type : t.send;
        out.push_back(Words{panel::Box{L.field.x, L.field.y, L.field.w, L.field.h}, hint, kFaint, 11.5f * s, false,
                            Align::Left});
    } else {
        float caretX = 0;
        const std::string shownText = fieldText(L, in, m, caretX);
        out.push_back(Words{panel::Box{L.field.x, L.field.y, L.field.w, L.field.h}, shownText, kText, 12 * s, false,
                            Align::Left});
    }
    return out;
}

std::string grouped(long long n) {
    std::string digits = std::to_string(n < 0 ? -n : n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += ' ';
        out += digits[i];
    }
    return n < 0 ? "-" + out : out;
}

}  // namespace uagfx::chatpanel
