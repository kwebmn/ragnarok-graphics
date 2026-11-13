#include "core/chat_panel.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "core/settings/search.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::chatpanel {
namespace {

// The concept's sizes at scale 1 (v23 chat): 520 wide, the tabs' row 30, a line's row 18 (its words 12.5), the
// input's row 28; the glass 86 % (the concept: chat and windows not under 80 %), idle 55 %
constexpr float kTabsH = 30, kRowH = 18, kInputH = 28, kPad = 12;
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
    if (in.searching) return kSystem;  // 3.0.11-beta.284
    if (!in.whisper.empty() || in.whisperFocus) return kWhisper;
    return in.mode == 1 ? kParty : in.mode == 2 ? kGuild : kText;
}

std::string chipText(const Input& in, const Texts& t) {
    if (in.searching) return t.search;  // 3.0.11-beta.284
    if (!in.whisper.empty() || in.whisperFocus) return "\xE2\x86\x92 " + in.whisper;  // → Name
    return t.tabs[in.mode == 1 ? 1 : in.mode == 2 ? 2 : 0];
}

// A tab's count of new lines: its pill right of its name; w 0 none
panel::Box badgeBox(const Layout& L, int i, const Show& sh, const Texts& t, const Measure& m) {
    if (i < 0 || i >= int(L.tabs.size()) || sh.unread[i] <= 0 || i == int(sh.tab)) return panel::Box{};
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

namespace {
// 3.0.12-beta.3 (players' screenshots): the server's MVP lines glued - «are29587acquired», «isBlue Potion» - spaced;
// the client's «Not Enough SP» in the player's language
std::string tidied(const std::string& text, const Texts& t) {
    if (text == "Not Enough SP" && !t.noSp.empty()) return t.noSp;
    std::string out = text;
    const auto spaceAfter = [&](const std::string& lead) {
        const size_t at = out.find(lead);
        if (at == std::string::npos) return;
        const size_t end = at + lead.size();
        if (end < out.size() && out[end] != ' ') out.insert(end, " ");
    };
    spaceAfter("Your reward EXP Points are");
    spaceAfter("Your reward item is");
    const size_t are = out.find("Your reward EXP Points are ");
    if (are != std::string::npos) {
        size_t d = are + 27;
        while (d < out.size() && out[d] >= '0' && out[d] <= '9') ++d;
        if (d > are + 27 && d < out.size() && out[d] != ' ' && out[d] != '!') out.insert(d, " ");
    }
    return out;
}
}  // namespace

std::string logLine(const Line& l, int hour, int minute, int second) {
    if (l.kind == Kind::Battle) return "";
    std::string words;
    for (const Span& s : l.spans) words += s.text;
    if (words.empty()) return "";
    for (char& c : words)
        if (c == '\r' || c == '\n') c = ' ';
    char at[16];
    std::snprintf(at, sizeof at, "[%02d:%02d:%02d] ", hour, minute, second);
    return at + words + "\r\n";
}

std::vector<std::string> pieces(const std::string& text, size_t most) {
    std::vector<std::string> out;
    if (most == 0) return out;
    size_t at = 0;
    while (at < text.size()) {
        while (at < text.size() && text[at] == ' ') ++at;  // no piece starts with a space
        if (at >= text.size()) break;
        if (text.size() - at <= most) {
            std::string last = text.substr(at);
            while (!last.empty() && last.back() == ' ') last.pop_back();
            out.push_back(last);
            break;
        }
        size_t cut = text.rfind(' ', at + most);
        if (cut == std::string::npos || cut <= at) cut = at + most;  // one long word: cut where it must
        std::string piece = text.substr(at, cut - at);
        while (!piece.empty() && piece.back() == ' ') piece.pop_back();
        out.push_back(piece);
        at = cut;
    }
    return out;
}

uint32_t chatColour(const std::string& token, uint32_t fallback) {
    if (token == "blue") return 0x7FD3FF;
    if (token == "green") return 0x9FE07C;
    if (token == "yellow") return 0xF3E27A;
    if (token == "orange") return 0xFFB067;
    if (token == "pink") return 0xFFAEE0;
    if (token == "violet") return 0xC3A6FF;
    if (token == "white") return 0xFFFFFF;
    return fallback;
}

Line fromGame(Source source, const std::string& gameText, uint32_t colorRef, const Texts& t, uint64_t ms) {
    const std::string text = source == Source::Public || source == Source::Party || source == Source::Guild ||
                                     source == Source::Whisper
                                 ? gameText
                                 : tidied(gameText, t);  // 3.0.12-beta.3: never a player's words
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
                uint32_t c = own;  // 3.0.13-beta.3: one's own or the others' colour - a GM's kept
                if (own == kText && (source == Source::PublicOwn ? t.own : t.others))
                    c = source == Source::PublicOwn ? t.own : t.others;
                l.spans = {Span{who, c, true, true}, Span{": " + rest, c, false, false}};
                return l;
            }
            // 3.0.11-beta.245 (R-YCYYXU: «Autolooting items…» as it came): the server's own lines come as the
            // character's (0x008e, no «name :») - short where known, the system's (its filter) either way
            if (source == Source::PublicOwn) {
                if (shortSystem(text, t, l)) return l;
                l.kind = Kind::System;
                l.spans = {Span{text, own, false, false}};
                return l;
            }
            break;
        case Source::Party:
        case Source::Guild: {
            const bool party = source == Source::Party;
            const uint32_t c = party ? t.party : t.guild;  // 3.0.13-beta.3: the player's colours
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
            if (shortSystem(text, t, l)) return l;  // 3.0.11-beta.243
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

bool showsWith(Tab tab, const std::string& person, const Line& line, int hide) {
    if (!shows(tab, line)) return false;
    if (tab == Tab::All && hide) {  // 3.0.11-beta.243: «Усі»'s filters
        if ((hide & kHideSystem) && line.kind == Kind::System) return false;
        if ((hide & kHideLoot) && line.kind == Kind::Loot) return false;
        if ((hide & kHideAnnounce) && line.kind == Kind::Announce) return false;
        if ((hide & kHideTrade) && line.kind == Kind::Public && line.trade) return false;
    }
    return tab != Tab::Whisper || person.empty() || line.who == person;
}

bool matches(const Line& line, const std::string& foldedWords) {
    if (foldedWords.empty()) return true;
    std::string all = line.who;
    for (const Span& sp : line.spans) all += ' ' + sp.text;
    return settings::foldCase(all).find(foldedWords) != std::string::npos;
}

int unreadOn(const Line& line) {
    return line.kind == Kind::Party ? 1 : line.kind == Kind::Guild ? 2 : line.kind == Kind::Whisper ? 3 : -1;
}

std::vector<Row> wrap(const std::vector<const Line*>& lines, float width, float px, const Measure& m) {
    std::vector<Row> rows;
    const auto box = [&](const std::string& s, bool bold) { return std::ceil(m(s, px, bold)) + 1; };
    for (size_t li = 0; li < lines.size(); ++li) {
        Row row;
        float x = 0;
        const auto put = [&](const std::string& unit, const Span& sp) {
            if (!row.empty() && row.back().rgb == sp.rgb && row.back().bold == sp.bold && row.back().name == sp.name &&
                row.back().item == sp.item && row.back().mob == sp.mob)
                row.back().text += unit;
            else
                row.push_back(Piece{unit, sp.rgb, sp.bold, sp.name, 0, 0, int(li), sp.item, sp.mob});
        };
        const auto next = [&]() {
            rows.push_back(row);
            row.clear();
            x = 0;
        };
        std::vector<Span> spans = lines[li]->spans;  // 3.0.11-beta.243: «×N» after a line said again
        if (lines[li]->repeat > 1)
            spans.push_back(Span{" \xC3\x97" + std::to_string(lines[li]->repeat), kMuted, true, false});
        for (const Span& sp : spans) {
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

std::vector<Row> WrapCache::wrap(const std::vector<const Line*>& lines, float width, float px, const Measure& m,
                                 int generation) {
    if (width != width_ || px != px_ || generation != generation_) {
        kept_.clear();
        width_ = width;
        px_ = px;
        generation_ = generation;
    }
    std::vector<Row> rows;
    std::unordered_map<uint32_t, Kept> next;
    next.reserve(lines.size());
    for (size_t li = 0; li < lines.size(); ++li) {
        const Line& l = *lines[li];
        Kept own;
        const auto it = l.serial ? kept_.find(l.serial) : kept_.end();
        if (it != kept_.end() && it->second.spans == l.spans.size() && it->second.repeat == l.repeat)
            own = std::move(it->second);
        else
            own = Kept{l.spans.size(), l.repeat, chatpanel::wrap(std::vector<const Line*>{&l}, width, px, m)};
        for (const Row& r : own.rows) {  // its pieces on its place in this list
            rows.push_back(r);
            for (Piece& p : rows.back()) p.line = int(li);
        }
        if (l.serial) next[l.serial] = std::move(own);
    }
    kept_ = std::move(next);
    return rows;
}

Layout layout(int left, int bottom, float scale, int rows, bool quiet, const Show& sh, const Input& in, const Texts& t,
              const Measure& m, int width) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    // 3.0.11-beta.240 (the chat concept, stage 1: «затухание без исчезновения»): idle, only its ground fades - its
    // tabs, its rows and its input stay where they were (the chat no longer jumps under the pointer)
    L.quiet = quiet;
    L.px = 12.5f * s;
    L.rowH = kRowH * s;
    L.rows = std::max(1, rows);
    const float w = float(std::clamp(width > 0 ? width : kWidth, kLeastWidth, kMostWidth)) * s, pad = kPad * s;
    const float tabsH = kTabsH * s, inputH = kInputH * s;
    // 3.0.11-beta.242: on «Шепіт» the row of the ones whispered with under the tabs
    const bool peopleShown = sh.tab == Tab::Whisper && !sh.people.empty();
    const float peopleH = peopleShown ? 26 * s : 0;
    const float top = tabsH + 4 * s + peopleH;
    const float linesH = float(L.rows) * L.rowH;
    const float h = top + linesH + 6 * s + inputH + 8 * s;
    L.glass = panel::Box{0, 0, w, h};
    L.lines = panel::Box{pad, top, w - 2 * pad - 10 * s, linesH};  // 3.0.11-beta.245: room for the scroll bar
    L.bar = panel::Box{w - pad - 5 * s, top + 2 * s, 5 * s, linesH - 4 * s};
    if (peopleShown) {
        L.peopleRow = panel::Box{pad, tabsH + 4 * s, w - 2 * pad, 22 * s};
        float x = L.peopleRow.x;
        const auto chip = [&](const std::string& name, int unread) {
            float cw = m(name, 11.5f * s, true) + 16 * s;
            if (unread > 0) cw += m(std::to_string(std::min(unread, 99)), 10 * s, true) + 10 * s;
            if (x + cw > L.peopleRow.x + L.peopleRow.w) return false;
            L.people.push_back(panel::Box{x, L.peopleRow.y, cw, L.peopleRow.h});
            x += cw + 4 * s;
            return true;
        };
        chip(t.all, 0);
        for (size_t i = 0; i < sh.people.size(); ++i)
            if (!chip(sh.people[i], i < sh.peopleUnread.size() ? sh.peopleUnread[i] : 0)) break;
    }
    if (sh.menuLine >= 0) {  // a name's menu where it was asked for, inside the chat (3.0.11-beta.244: a line's too)
        const int n = sh.menuNamed ? 4 : 1;
        float mw = 0;
        for (int i = 0; i < n; ++i) mw = std::max(mw, m(t.menu[menuAction(sh, i)], 12 * s, false));
        mw += 24 * s;
        const float mh = (float(n) * 22 + 8) * s;
        const float mx = std::clamp(sh.menuX, 4 * s, w - mw - 4 * s);
        const float my = std::clamp(sh.menuY - mh, 4 * s, h - mh - 4 * s);
        L.menu = panel::Box{mx, my, mw, mh};
        for (int i = 0; i < n; ++i)
            L.items.push_back(panel::Box{mx + 4 * s, my + (4 + 22 * float(i)) * s, mw - 8 * s, 22 * s});
    }
    L.grip = panel::Box{w - 18 * s, 0, 18 * s, 18 * s};  // 3.0.11-beta.241
    // 3.0.18-beta.105: the search's magnifier, left of «Усі»'s gear (where the gear stands on the other tabs)
    L.searchButton = panel::Box{w - (sh.tab == Tab::All ? 72 : 46) * s, 5 * s, 22 * s, 22 * s};
    if (sh.tab == Tab::All) {  // 3.0.11-beta.243: «Усі»'s filters - a gear left of the grip, its menu under it
        L.filtersButton = panel::Box{w - 46 * s, 5 * s, 22 * s, 22 * s};
        if (sh.filtersOpen) {
            float fw = 0;
            for (const std::string& f : t.filters) fw = std::max(fw, m(f, 12 * s, false));
            fw += 40 * s;
            const float fh = (4 * 22 + 8) * s;
            L.filtersMenu = panel::Box{w - fw - 8 * s, 30 * s, fw, fh};
            for (int i = 0; i < 4; ++i)
                L.filters.push_back(panel::Box{L.filtersMenu.x + 4 * s, L.filtersMenu.y + (4 + 22 * float(i)) * s,
                                               fw - 8 * s, 22 * s});
        }
    }
    {
        L.tabsRow = panel::Box{0, 0, w, tabsH};
        float x = 8 * s;
        for (int i = 0; i < kTabs; ++i) {
            float tw = m(t.tabs[i], 12 * s, true) + 20 * s;
            if (sh.unread[i] > 0 && i != int(sh.tab))
                tw += m(std::to_string(std::min(sh.unread[i], 99)), 10 * s, true) + 14 * s;
            L.tabs.push_back(panel::Box{x, 4 * s, tw, tabsH - 4 * s});
            x += tw + 2 * s;
        }
        // 3.0.18-beta.105: a chat too narrow for the tabs and the magnifier - none (Ctrl+F in the field still opens it)
        if (x > L.searchButton.x) L.searchButton = panel::Box{};
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
        if (commandHints(in.text)) {  // 3.0.11-beta.240: over the rows' bottom, under the newest
            float hw = 0;
            for (const std::string& c : t.commands) hw = std::max(hw, m(c, 12 * s, false));
            const float hh = (4 * 18 + 8) * s;
            L.hints = panel::Box{L.lines.x, L.lines.y + L.lines.h - hh, std::min(L.lines.w, hw + 20 * s), hh};
        }
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
    for (size_t i = 0; i < L.items.size(); ++i)  // 3.0.11-beta.242: a name's menu over all
        if (L.items[i].contains(x, y)) return Hit{Part::Item, int(i)};
    for (size_t i = 0; i < L.filters.size(); ++i)  // 3.0.11-beta.243: the filters' menu
        if (L.filters[i].contains(x, y)) return Hit{Part::Filter, int(i)};
    if (L.filtersMenu.w > 0 && L.filtersMenu.contains(x, y)) return Hit{Part::Body, -1};
    if (L.filtersButton.w > 0 && L.filtersButton.contains(x, y)) return Hit{Part::Filters, -1};
    if (L.searchButton.w > 0 && L.searchButton.contains(x, y)) return Hit{Part::Search, -1};  // 3.0.18-beta.105
    if (L.menu.w > 0 && L.menu.contains(x, y)) return Hit{Part::Body, -1};
    if (L.grip.w > 0 && L.grip.contains(x, y)) return Hit{Part::Grip, -1};  // 3.0.11-beta.241
    for (size_t i = 0; i < L.people.size(); ++i)  // 3.0.11-beta.242: index 0 «Усі», then the ones whispered with
        if (L.people[i].contains(x, y)) return Hit{Part::Person, int(i)};
    {
        for (size_t i = 0; i < L.tabs.size(); ++i)
            if (L.tabs[i].contains(x, y)) return Hit{Part::Tab, int(i)};
        if (L.newer.w > 0 && L.newer.contains(x, y)) return Hit{Part::Newer, -1};
        if (L.clear.w > 0 && L.clear.contains(x, y)) return Hit{Part::Clear, -1};
        if (L.mode.contains(x, y)) return Hit{Part::Mode, -1};
        if (L.input.contains(x, y)) return Hit{Part::Field, -1};
    }
    if (rows.size() > size_t(std::max(0, L.rows)) &&  // 3.0.11-beta.245: the scroll bar (a little wider to take)
        x >= L.bar.x - 4 * L.s && x <= L.bar.x + L.bar.w + 4 * L.s && y >= L.bar.y && y <= L.bar.y + L.bar.h)
        return Hit{Part::Bar, -1};
    if (L.lines.contains(x, y)) {
        size_t first = 0, count = 0;
        shown(L, rows, sh.scroll, first, count);
        const float top = L.lines.y + L.lines.h - float(count) * L.rowH;
        if (y >= top) {
            const size_t r = first + size_t((y - top) / L.rowH);
            if (r < first + count)
                for (const Piece& p : rows[r])
                    if (x >= L.lines.x + p.x && x < L.lines.x + p.x + p.w) {
                        if (p.name) return Hit{Part::Name, p.line};
                        if (p.item) return Hit{Part::Link, p.item};  // 3.0.11-beta.244
                        if (p.mob) return Hit{Part::Link, -p.mob};
                    }
            if (r < first + count && !rows[r].empty()) return Hit{Part::Lines, rows[r].front().line};  // .243
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

std::vector<Words> words(const Layout& L, const std::vector<Row>& rows, const Input& in, const Show& sh,
                         const Texts& t, const Measure& m) {
    std::vector<Words> out;
    const float s = L.s;
    {
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
    const std::string chip = chipText(in, t);
    out.push_back(Words{panel::Box{L.mode.x + 8 * s, L.mode.y, std::ceil(m(chip, 12 * s, true)) + 2, L.mode.h}, chip,
                        chipRgb(in), 12 * s, true, Align::Left});
    if (L.clear.w > 0)
        out.push_back(Words{L.clear, "\xC3\x97", sh.hoverClear ? kText : kMuted, 13 * s, true, Align::Center});  // ×
    if (L.newer.w > 0) out.push_back(Words{L.newer, t.newer, kText, 11 * s, true, Align::Center});
    for (size_t i = 0; i < L.people.size(); ++i) {  // 3.0.11-beta.242
        const std::string& name = i == 0 ? t.all : sh.people[i - 1];
        const bool on = i == 0 ? sh.person.empty() : sh.person == name;
        const int unread = i == 0 || i - 1 >= sh.peopleUnread.size() ? 0 : sh.peopleUnread[i - 1];
        const panel::Box& b = L.people[i];
        const float nw = std::ceil(m(name, 11.5f * s, true)) + 2;
        out.push_back(Words{panel::Box{b.x + 8 * s, b.y, nw, b.h}, name, on ? kText : kMuted, 11.5f * s, true,
                            Align::Left});
        if (unread > 0)
            out.push_back(Words{panel::Box{b.x + 8 * s + nw + 2 * s, b.y, b.w - nw - 14 * s, b.h},
                                std::to_string(std::min(unread, 99)), kWhisper, 10 * s, true, Align::Left});
    }
    for (size_t i = 0; i < L.items.size(); ++i)
        out.push_back(Words{panel::Box{L.items[i].x + 8 * s, L.items[i].y, L.items[i].w - 12 * s, L.items[i].h},
                            t.menu[menuAction(sh, int(i))], int(i) == sh.hoverItem ? kText : kMuted, 12 * s, false,
                            Align::Left});
    for (size_t i = 0; i < L.filters.size(); ++i)  // 3.0.11-beta.243
        out.push_back(Words{panel::Box{L.filters[i].x + 26 * s, L.filters[i].y, L.filters[i].w - 30 * s,
                                       L.filters[i].h},
                            t.filters[i], int(i) == sh.hoverFilter ? kText : kMuted, 12 * s, false, Align::Left});
    if (sh.hoverRowLine >= 0 && sh.clockSec >= 0) {  // 3.0.11-beta.243: the line pointed at - its time at its right
        size_t first = 0, count = 0;
        shown(L, rows, sh.scroll, first, count);
        const float top = L.lines.y + L.lines.h - float(count) * L.rowH;
        for (size_t r = first; r < first + count; ++r) {
            if (rows[r].empty() || rows[r].front().line != sh.hoverRowLine) continue;
            const uint64_t at =
                size_t(sh.hoverRowLine) < sh.lineTimes.size() ? sh.lineTimes[size_t(sh.hoverRowLine)] : 0;
            if (!at || at > sh.nowMs) break;
            const std::string when = clock(sh.nowMs - at, sh.clockSec);
            const float tw = std::ceil(m(when, 10.5f * s, false)) + 6;
            out.push_back(Words{panel::Box{L.lines.x + L.lines.w - tw, top + float(r - first) * L.rowH, tw, L.rowH},
                                when, kFaint, 10.5f * s, false, Align::Right});
            break;
        }
    }
    if (L.hints.w > 0) {  // 3.0.11-beta.240: the commands, the one being typed lit
        for (int i = 0; i < 4; ++i) {
            std::string line = t.commands[i];
            if (i == 3) {
                const size_t at = line.find("{1}");
                if (at != std::string::npos) line.replace(at, 3, in.replyTo.empty() ? "\xE2\x80\x94" : in.replyTo);
            }
            const bool typed = in.text.size() > 1 && line.rfind(in.text, 0) == 0;
            out.push_back(Words{panel::Box{L.hints.x + 10 * s, L.hints.y + (4 + 18 * float(i)) * s, L.hints.w - 14 * s,
                                           18 * s},
                                line, typed ? kText : kMuted, 12 * s, typed, Align::Left});
        }
    }
    if (in.text.empty()) {
        const std::string& hint = in.searching             ? t.searchHint
                                  : in.walk && !in.typing    ? t.walk  // 3.0.18-beta.208
                                  : in.battle && !in.typing ? t.type
                                                            : t.send;
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

bool commandHints(const std::string& utf8) {
    if (utf8.empty() || utf8[0] != '/' || utf8.find(' ') != std::string::npos || utf8.size() > 2) return false;
    return utf8.size() == 1 || std::string("pgwr").find(utf8[1]) != std::string::npos;
}

bool command(const std::string& utf8, const std::string& replyTo, Command& out) {
    if (utf8.size() < 3 || utf8[0] != '/' || utf8[2] != ' ') return false;
    const char c = utf8[1];
    std::string rest = utf8.substr(3);
    while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
    out = Command{};
    if (c == 'p' || c == 'g') {
        if (rest.empty()) return false;
        out.mode = c == 'p' ? 1 : 2;
        out.words = rest;
        return true;
    }
    if (c == 'r') {
        if (rest.empty() || replyTo.empty()) return false;
        out.whisper = replyTo;
        out.words = rest;
        return true;
    }
    if (c != 'w') return false;
    size_t end = 0;
    if (!rest.empty() && rest[0] == '"') {  // a name with spaces: in quotes
        end = rest.find('"', 1);
        if (end == std::string::npos) return false;
        out.whisper = rest.substr(1, end - 1);
        ++end;
    } else {
        end = rest.find(' ');
        if (end == std::string::npos) return false;
        out.whisper = rest.substr(0, end);
    }
    out.words = rest.substr(std::min(rest.size(), end));
    while (!out.words.empty() && out.words.front() == ' ') out.words.erase(out.words.begin());
    return !out.whisper.empty() && !out.words.empty();
}

void sizeTo(const Layout& L, float x, float y, int& width, int& rows) {
    const float s = L.s > 0 ? L.s : 1.0f;
    width = std::clamp(int(std::lround((x - float(L.x)) / s)), kLeastWidth, kMostWidth);
    const float fixed = float(L.h) - float(L.rows) * L.rowH;  // its tabs, input and edges
    const float linesH = float(L.y + L.h) - y - fixed;
    rows = std::clamp(int(std::lround(linesH / L.rowH)), kLeastRows, kMostRows);
}

namespace {
// «… of 12.50% and below» -> «12.50%»; a number after `key`, as text
bool after(const std::string& text, const std::string& key, std::string& value) {
    const size_t at = text.find(key);
    if (at == std::string::npos) return false;
    size_t i = at + key.size(), j = i;
    while (j < text.size() && (std::isdigit(static_cast<unsigned char>(text[j])) || text[j] == '.' || text[j] == '%'))
        ++j;
    value = text.substr(i, j - i);
    return !value.empty();
}

std::string put2(std::string form, const std::string& a, const std::string& b) {
    for (const auto& [k, v] : {std::pair<std::string, std::string>{"{1}", a}, {"{2}", b}})
        if (const size_t at = form.find(k); at != std::string::npos) form.replace(at, 2 + 1, v);
    return form;
}
}  // namespace

bool shortSystem(const std::string& text, const Texts& t, Line& out) {
    std::string v, w;
    std::string said;
    if (after(text, "Autolooting items with drop rates of ", v) && !t.autoloot.empty()) {
        said = put2(t.autoloot, v, "");
    } else if (text.rfind("Autoloot is now off", 0) == 0 && !t.autolootOff.empty()) {
        said = t.autolootOff;
    } else if (text.rfind("Gained exp is now shown", 0) == 0 && !t.expOn.empty()) {
        said = t.expOn;
    } else if (text.rfind("Gained exp will not be shown", 0) == 0 && !t.expOff.empty()) {
        said = t.expOff;
    } else if (text.rfind("Gained zeny is now shown", 0) == 0 && !t.zenyOn.empty()) {
        said = t.zenyOn;
    } else if (text.rfind("Gained zeny will not be shown", 0) == 0 && !t.zenyOff.empty()) {
        said = t.zenyOff;
    } else if (after(text, "Experience Gained Base:", v) && after(text, "Job:", w) && !t.expGained.empty()) {
        out.kind = Kind::Loot;  // a kill's experience: the loot's filter, «Бій»
        out.spans = {Span{put2(t.expGained, grouped(std::atoll(v.c_str())), grouped(std::atoll(w.c_str()))),
                          kBaseExp, false, false}};
        return true;
    }
    if (said.empty()) return false;
    out.kind = Kind::System;
    out.spans = {Span{said, kSystem, false, false}};
    return true;
}

std::string clock(uint64_t ago, int nowSec) {
    long long sec = (long long)nowSec - (long long)(ago / 1000);
    sec %= 86400;
    if (sec < 0) sec += 86400;
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02lld:%02lld", sec / 3600, (sec / 60) % 60);
    return buf;
}

int menuAction(const Show& sh, int index) { return sh.menuNamed ? std::clamp(index, 0, 3) : 3; }

panel::Box barThumb(const Layout& L, size_t total, int scroll) {
    const size_t shownRows = size_t(std::max(1, L.rows));
    if (total <= shownRows || L.bar.h <= 0) return panel::Box{};
    const float h = std::max(L.bar.h * float(shownRows) / float(total), 14 * L.s);
    const float most = float(total - shownRows);
    const float up = std::clamp(float(scroll), 0.0f, most) / most;  // 0 the newest (the bottom), 1 the oldest
    return panel::Box{L.bar.x, L.bar.y + (L.bar.h - h) * (1 - up), L.bar.w, h};
}

int scrollAt(const Layout& L, size_t total, float y) {
    const size_t shownRows = size_t(std::max(1, L.rows));
    if (total <= shownRows || L.bar.h <= 0) return 0;
    const float h = std::max(L.bar.h * float(shownRows) / float(total), 14 * L.s);
    const float room = std::max(1.0f, L.bar.h - h);
    const float top = std::clamp(y - h / 2 - L.bar.y, 0.0f, room);
    return int(std::lround((1 - top / room) * float(total - shownRows)));
}

View view(const Layout& L, const std::vector<Row>& rows, const Input& in, const Show& sh, const Texts& t,
          const Measure& m) {
    View v;
    const float s = L.s;
    for (size_t i = 0; i < L.tabs.size(); ++i) v.badges.push_back(badgeBox(L, int(i), sh, t, m));
    v.chipRgb = chipRgb(in);
    if (in.whisper.empty() && !in.whisperFocus) {
        const float icon = 11 * s;
        v.chevron = panel::Box{L.mode.x + L.mode.w - icon - 6 * s, L.mode.y + (L.mode.h - icon) / 2, icon, icon};
    }
    if (in.caretOn && in.whisperFocus) {
        const float x = L.mode.x + 8 * s + m(chipText(in, t), 12 * s, true) + 1 * s;
        v.caret = panel::Box{x, L.mode.y + 4 * s, 1.5f * s, L.mode.h - 8 * s};
        v.caretRgb = kWhisper;
    } else if (in.caretOn && in.typing && !in.text.empty()) {
        float caretX = 0;
        fieldText(L, in, m, caretX);
        v.caret = panel::Box{L.field.x + caretX, L.field.y + 6 * s, 1.5f * s, L.field.h - 12 * s};
        v.caretRgb = kText;
    }
    v.thumb = barThumb(L, rows.size(), sh.scroll);
    if (sh.hoverLine >= 0 || sh.hoverLink != 0) {
        size_t first = 0, count = 0;
        shown(L, rows, sh.scroll, first, count);
        const float top = L.lines.y + L.lines.h - float(count) * L.rowH;
        for (size_t r = first; r < first + count; ++r)
            for (const Piece& p : rows[r])
                if ((p.name && p.line == sh.hoverLine) ||
                    (sh.hoverLink != 0 && (p.item == sh.hoverLink || (p.mob && -p.mob == sh.hoverLink))))
                    v.underlines.push_back(Underline{
                        panel::Box{L.lines.x + p.x, top + float(r - first + 1) * L.rowH - 3 * s, p.w - 1, 1 * s},
                        p.rgb});
    }
    return v;
}

std::vector<Span> bracketLinks(const std::vector<Span>& in,
                               const std::function<std::pair<int, int>(const std::string& name)>& resolve) {
    std::vector<Span> out;
    for (const Span& sp : in) {
        if (sp.name || sp.item || sp.mob || sp.text.find('[') == std::string::npos || !resolve) {
            out.push_back(sp);
            continue;
        }
        const std::string& s = sp.text;
        std::string plain;
        size_t i = 0;
        const auto flush = [&] {
            if (plain.empty()) return;
            Span p = sp;
            p.text = plain;
            out.push_back(std::move(p));
            plain.clear();
        };
        while (i < s.size()) {
            const size_t open = s.find('[', i);
            if (open == std::string::npos) break;
            const size_t close = s.find(']', open + 1);
            if (close == std::string::npos) break;
            const std::string name = s.substr(open + 1, close - open - 1);
            const std::pair<int, int> to =
                name.empty() || name.size() > 64 ? std::pair<int, int>{0, 0} : resolve(name);
            if (to.first > 0 || to.second > 0) {
                plain += s.substr(i, open - i);
                flush();
                Span link = sp;
                link.text = s.substr(open, close - open + 1);
                link.item = to.first;
                link.mob = to.second;
                out.push_back(std::move(link));
            } else {
                plain += s.substr(i, close + 1 - i);
            }
            i = close + 1;
        }
        plain += s.substr(i);
        flush();
    }
    return out;
}

}  // namespace uagfx::chatpanel
