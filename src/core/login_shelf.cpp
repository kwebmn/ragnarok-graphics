#include "core/login_shelf.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/class_art.hpp"
#include "core/settings/search.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::loginshelf {
namespace {

// At scale 1 (the concept's picture): a card 228 x 338, 20 apart, its picture 196 high; the title's pill 34 high, 18
// over the row; the buttons 42 high, 30 under it, 14 apart (210, 210, 150 wide); the arrows 36 x 56, 16 beside the row
constexpr float kCardW = 228, kCardH = 338, kGap = 20, kPicH = 196, kTitleH = 34, kTitleGap = 18, kRowGap = 30;
constexpr float kButtonH = 42, kButtonGap = 14, kOtherW = 210, kCreateW = 210, kExitW = 150;
constexpr float kArrowW = 36, kArrowH = 56, kArrowGap = 16, kPlayH = 40, kPlayIn = 14, kDotsW = 30, kDotsH = 26;
constexpr float kMenuRowH = 32, kMenuPad = 6, kLinkPad = 12;
constexpr float kSiteH = 40;  // 3.0.11-beta.181: the site's links' row under the buttons (.183: 12 + 26 high plates)
// 3.0.11-beta.189: the bar under the row (4 high, 10 under it; a press 9 either side of it counts), the search's field
// on the title row's left, the row's glide (a time constant: most of the way in a tenth of a second)
constexpr float kBarH = 4, kBarGap = 10, kBarReach = 9, kSearchW = 230, kClearW = 30, kGlideMs = 50;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kLogin = 0x7E879A, kRed = 0xFF5A4E, kAmber = 0xF2C46B, kPill = 0x0A0C12;
constexpr uint32_t kChosenWords = 0xF5E6BE, kKey = 0xB9A47A, kLink = 0x8CB8FF;

float rowWidth(int slots) { return slots * kCardW + std::max(0, slots - 1) * kGap; }

std::string capital(const std::string& s) {
    std::string out = s;
    if (!out.empty()) out[0] = char(std::toupper((unsigned char)out[0]));
    return out;
}

std::string fill(const std::string& pattern, const std::string& v) {
    const size_t at = pattern.find("{1}");
    return at == std::string::npos ? pattern + " " + v : pattern.substr(0, at) + v + pattern.substr(at + 3);
}

}  // namespace

Layout layout(int targetW, int targetH, float scale, size_t cards, int first, bool site, const Scroll& sc) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const int slots = int(cards) + (sc.add ? 1 : 0);  // the accounts and «Додати акаунт»
    const int most = mostFirst(cards, sc.add);
    L.target = std::clamp(first, 0, most);
    L.most = most;
    const float shown = sc.pos < 0 ? float(L.target) : std::clamp(sc.pos, 0.0f, float(most));
    const float w = (rowWidth(kInSight) + 2 * (kArrowGap + kArrowW)) * s;
    const float h = (kTitleH + kTitleGap + kCardH + kRowGap + kButtonH + (site ? kSiteH : 0.0f)) * s;
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    L.title = panel::Box{0, 0, w, kTitleH * s};
    const float top = (kTitleH + kTitleGap) * s, row = rowWidth(std::min(kInSight, std::max(1, slots))) * s;
    const float left = (w - row) / 2, step = (kCardW + kGap) * s;
    L.row = panel::Box{left, top, row, kCardH * s};
    // 3.0.11-beta.189: the cards from the one half in sight at the left to the one half in sight at the right
    L.first = std::max(0, int(std::floor(shown)));
    for (int i = L.first; i < slots; ++i) {
        const panel::Box b{left + (float(i) - shown) * step, top, kCardW * s, kCardH * s};
        if (b.x >= left + row - 0.5f) break;
        if (i < int(cards))
            L.cards.push_back(b);
        else
            L.add = b;
    }
    const float arrowY = top + (kCardH - kArrowH) * s / 2;
    if (L.target > 0) L.left = panel::Box{left - (kArrowGap + kArrowW) * s, arrowY, kArrowW * s, kArrowH * s};
    if (L.target < most) L.right = panel::Box{left + row + kArrowGap * s, arrowY, kArrowW * s, kArrowH * s};
    if (slots > kInSight) {  // 3.0.11-beta.189: the bar - the part in sight
        L.bar = panel::Box{left, top + (kCardH + kBarGap) * s, row, kBarH * s};
        const float tw = std::max(L.bar.w * float(kInSight) / float(slots), 24 * s);
        L.thumb = panel::Box{left + (row - tw) * shown / float(std::max(1, most)), L.bar.y, tw, L.bar.h};
    }
    if (sc.search) {  // 3.0.11-beta.189: the search's field
        L.search = panel::Box{0, 0, kSearchW * s, kTitleH * s};
        L.searchClear = panel::Box{L.search.w - kClearW * s, 0, kClearW * s, kTitleH * s};
    }
    const float by = top + (kCardH + kRowGap) * s;
    const float buttons = (kOtherW + kCreateW + kExitW + 2 * kButtonGap) * s;
    float bx = (w - buttons) / 2;
    L.other = panel::Box{bx, by, kOtherW * s, kButtonH * s};
    bx += (kOtherW + kButtonGap) * s;
    L.create = panel::Box{bx, by, kCreateW * s, kButtonH * s};
    bx += (kCreateW + kButtonGap) * s;
    L.exit = panel::Box{bx, by, kExitW * s, kButtonH * s};
    if (site) {  // 3.0.11-beta.181: the site account's parts
        L.chars = panel::Box{w - 150 * s, 0, 150 * s, kTitleH * s};
        const float ly = by + (kButtonH + 12) * s;
        L.newLogin = panel::Box{w / 2 - 230 * s, ly, 220 * s, 26 * s};
        L.linkLogin = panel::Box{w / 2 + 10 * s, ly, 220 * s, 26 * s};
    }
    return L;
}

int mostFirst(size_t cards, bool add) { return std::max(0, int(cards) + (add ? 1 : 0) - kInSight); }

float glide(float pos, int first, float ms) {
    if (pos < 0) return float(first);
    const float d = float(first) - pos;
    if (std::fabs(d) < 0.005f) return float(first);  // about a pixel
    return pos + d * (1.0f - std::exp(-std::max(0.0f, ms) / kGlideMs));
}

int barFirst(const Layout& L, float x) {
    if (L.bar.w <= 0 || L.most <= 0) return L.target;
    const float share = std::clamp((x - L.bar.x - L.thumb.w / 2) / std::max(1.0f, L.bar.w - L.thumb.w), 0.0f, 1.0f);
    return int(std::lround(share * float(L.most)));
}

bool matches(const std::string& search, const std::vector<std::string>& words) {
    const std::string want = settings::foldCase(search);
    std::vector<std::string> folded;
    for (const std::string& w : words) folded.push_back(settings::foldCase(w));
    size_t at = 0;
    while (at < want.size()) {
        const size_t space = want.find(' ', at);
        const std::string part = want.substr(at, space == std::string::npos ? std::string::npos : space - at);
        at = space == std::string::npos ? want.size() : space + 1;
        if (part.empty()) continue;
        if (std::none_of(folded.begin(), folded.end(),
                         [&](const std::string& f) { return f.find(part) != std::string::npos; }))
            return false;
    }
    return true;
}

panel::Box menuButton(const Layout& L, int visible) {
    if (visible < 0 || visible >= int(L.cards.size())) return {};
    const panel::Box& c = L.cards[size_t(visible)];
    return panel::Box{c.x + c.w - (10 + kDotsW) * L.s, c.y + 10 * L.s, kDotsW * L.s, kDotsH * L.s};
}

Hit hitTest(const Layout& L, float x, float y) {
    const bool inRow = L.row.w <= 0 || L.row.contains(x, y);  // 3.0.11-beta.189: a card half in sight - its half
    for (size_t i = 0; i < L.cards.size() && inRow; ++i) {
        if (menuButton(L, int(i)).contains(x, y)) return Hit{Part::Menu, L.first + int(i)};
        if (L.cards[i].contains(x, y)) return Hit{Part::Card, L.first + int(i)};
    }
    if (inRow && L.add.w > 0 && L.add.contains(x, y)) return Hit{Part::Add, -1};
    if (L.search.w > 0 && L.searchClear.contains(x, y)) return Hit{Part::SearchClear, -1};  // 3.0.11-beta.189
    if (L.search.w > 0 && L.search.contains(x, y)) return Hit{Part::Search, -1};
    if (L.bar.w > 0 && x >= L.bar.x && x < L.bar.x + L.bar.w && y >= L.bar.y - kBarReach * L.s &&
        y < L.bar.y + L.bar.h + kBarReach * L.s)
        return Hit{Part::Bar, -1};
    if (L.left.w > 0 && L.left.contains(x, y)) return Hit{Part::Left, -1};
    if (L.right.w > 0 && L.right.contains(x, y)) return Hit{Part::Right, -1};
    if (L.chars.w > 0 && L.chars.contains(x, y)) return Hit{Part::Chars, -1};  // 3.0.11-beta.181
    if (L.newLogin.w > 0 && L.newLogin.contains(x, y)) return Hit{Part::NewLogin, -1};
    if (L.linkLogin.w > 0 && L.linkLogin.contains(x, y)) return Hit{Part::LinkLogin, -1};
    if (L.other.contains(x, y)) return Hit{Part::Other, -1};
    if (L.create.contains(x, y)) return Hit{Part::Create, -1};
    if (L.exit.contains(x, y)) return Hit{Part::Exit, -1};
    return {};
}

bool classicInstead(size_t accounts) { return accounts == 0; }

int padMove(int index, int count, int dir) { return count <= 0 ? 0 : std::clamp(index + dir, 0, count - 1); }

std::string jobLine(const Card& c) {
    std::string line = c.job;
    if (c.level > 0) line += (line.empty() ? "" : " · ") + std::to_string(c.level);
    if (!c.map.empty()) line += (line.empty() ? "" : " · ") + capital(c.map);
    return line;
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Card>& cards, const Show& sh, const Texts& t) {
    const float s = L.s;
    const float tw = panel::wordsWidth(t.title, 17 * s, true) + 8 * s + panel::wordsWidth(t.titleHint, 14 * s, false);
    const float pw = tw + 36 * s;
    panel::roundRect(out, panel::Box{(float(L.w) - pw) / 2, L.title.y, pw, L.title.h}, L.title.h / 2, kPill, 0.72f,
                     kWhite, 0.10f, 1.0f * s);
    if (L.search.w > 0) {  // 3.0.11-beta.189: the search's field, gold edged with words in it
        const bool typed = !sh.search.empty(), hot = sh.hoverPart == Part::Search || sh.hoverPart == Part::SearchClear;
        panel::roundRect(out, L.search, L.search.h / 2, kPill, 0.80f, typed ? kGold : kWhite,
                         typed ? 0.75f : hot ? 0.30f : 0.14f, 1.0f * s);
        const float icon = 15 * s;
        uiicons::draw(out, uiicons::Icon::Search, L.search.x + 12 * s, L.search.y + (L.search.h - icon) / 2, icon,
                      typed ? kGold2 : kMuted, 0.95f);
    }
    // 3.0.11-beta.189: the cards within the row alone - one half in sight while it glides
    const int keepX0 = out.clipX0, keepX1 = out.clipX1;
    out.clipX0 = std::max(keepX0, int(std::floor(L.row.x)));
    out.clipX1 = std::min(keepX1, int(std::ceil(L.row.x + L.row.w)));
    for (size_t i = 0; i < L.cards.size(); ++i) {
        const int index = L.first + int(i);
        if (index >= int(cards.size())) break;
        const Card& c = cards[size_t(index)];
        const panel::Box& b = L.cards[i];
        const bool focus = sh.focus == index, hot = sh.hover == index;
        panel::roundRect(out, b, 14 * s, kCard, std::max(0.94f, uistyle::window()), focus ? kGold : kWhite,
                         focus ? 0.95f : hot ? 0.30f : 0.12f, (focus ? 2.0f : 1.0f) * s);
        // its picture: the class's portrait (3.0.11-beta.178), else the character's head on a gold-dark ground
        const panel::Box pic{b.x + 1 * s, b.y + 1 * s, b.w - 2 * s, kPicH * s};
        if (c.art) {
            classart::cover(out, pic, c.art->px, c.art->width, c.art->height, 13 * s, classart::kTopCorners);
        } else {
            panel::roundRect(out, pic, 13 * s, 0x2A2416, 0.9f);
            panel::roundRect(out, panel::Box{pic.x, pic.y + pic.h - 14 * s, pic.w, 14 * s}, 0, 0x2A2416, 0.9f);
            if (c.head) classart::centre(out, pic, *c.head);
        }
        if (!c.label.empty() || sh.editing == index) {
            const std::string& label = sh.editing == index ? sh.editText : c.label;
            const float lw = panel::wordsWidth(label, 12 * s, true) + 16 * s + (sh.editing == index ? 6 * s : 0);
            panel::roundRect(out, panel::Box{b.x + 12 * s, b.y + 12 * s, std::max(lw, 40 * s), 22 * s}, 6 * s, kPill,
                             0.78f, kGold, sh.editing == index ? 0.9f : 0.0f, 1.0f * s);
        }
        if (focus || hot) panel::roundRect(out, menuButton(L, int(i)), 6 * s, kPill, 0.78f);
        const panel::Box play{b.x + kPlayIn * s, b.y + b.h - (kPlayIn + kPlayH) * s, b.w - 2 * kPlayIn * s, kPlayH * s};
        if (focus || hot)
            panel::roundRect(out, play, 9 * s, kGold, hot ? 0.34f : 0.26f, kGold, 0.9f, 1.0f * s);
        else
            panel::roundRect(out, play, 9 * s, kWhite, 0.06f, kWhite, 0.16f, 1.0f * s);
    }
    if (L.add.w > 0) {
        const bool hot = sh.hoverPart == Part::Add;
        panel::roundRect(out, L.add, 14 * s, kCard, 0.70f, kWhite, hot ? 0.45f : 0.28f, 1.5f * s);
        const panel::Box plus{L.add.x + L.add.w / 2 - 28 * s, L.add.y + L.add.h / 2 - 62 * s, 56 * s, 56 * s};
        panel::roundRect(out, plus, 28 * s, kCard, 0.0f, kWhite, 0.35f, 1.5f * s);
    }
    out.clipX0 = keepX0;
    out.clipX1 = keepX1;
    if (L.bar.w > 0) {  // 3.0.11-beta.189: the bar - the part of the row in sight
        panel::roundRect(out, L.bar, L.bar.h / 2, kWhite, 0.10f);
        panel::roundRect(out, L.thumb, L.thumb.h / 2, sh.hoverPart == Part::Bar ? kGold : kWhite,
                         sh.hoverPart == Part::Bar ? 0.85f : 0.45f);
    }
    const auto arrow = [&](const panel::Box& a, Part p) {
        if (a.w > 0) panel::roundRect(out, a, 10 * s, kPill, sh.hoverPart == p ? 0.9f : 0.72f, kWhite, 0.14f, 1.0f * s);
    };
    arrow(L.left, Part::Left);
    arrow(L.right, Part::Right);
    // 3.0.11-beta.183 (R-BFQFDU): the buttons and the site's links on the dark plates of the title and the arrows - a
    // faint white ghost under white words was lost over a bright wallpaper
    const auto button = [&](const panel::Box& b, Part p) {
        const bool hot = sh.hoverPart == p;
        panel::roundRect(out, b, 9 * s, kPill, hot ? 0.92f : 0.78f, kWhite, hot ? 0.40f : 0.22f, 1.0f * s);
    };
    button(L.other, Part::Other);
    button(L.create, Part::Create);
    button(L.exit, Part::Exit);
    const auto link = [&](const panel::Box& b, const std::string& words, float size, bool right) {
        if (b.w <= 0 || words.empty()) return;
        const float pw = std::min(b.w, panel::wordsWidth(words, size * s, false) + 2 * kLinkPad * s);
        panel::roundRect(out, panel::Box{right ? b.x + b.w - pw : b.x, b.y, pw, b.h}, b.h / 2, kPill, 0.72f, kWhite,
                         0.10f, 1.0f * s);
    };
    link(L.chars, t.chars, 13.5f, true);
    link(L.newLogin, t.newLogin, 13, true);
    link(L.linkLogin, t.linkLogin, 13, false);
}

std::vector<Words> words(const Layout& L, const std::vector<Card>& cards, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    const float tw = panel::wordsWidth(t.title, 17 * s, true), hw = panel::wordsWidth(t.titleHint, 14 * s, false);
    const float tx = (float(L.w) - (tw + 8 * s + hw)) / 2;
    out.push_back(Words{panel::Box{tx, L.title.y, tw + 2, L.title.h}, t.title, kText, 17 * s, true, Align::Left});
    out.push_back(Words{panel::Box{tx + tw + 8 * s, L.title.y, hw + 2, L.title.h}, t.titleHint, kMuted, 14 * s, false,
                        Align::Left});
    const size_t rowFrom = out.size();  // 3.0.11-beta.189: the cards' words from here - drawn within the row
    for (size_t i = 0; i < L.cards.size(); ++i) {
        const int index = L.first + int(i);
        if (index >= int(cards.size())) break;
        const Card& c = cards[size_t(index)];
        const panel::Box& b = L.cards[i];
        const bool focus = sh.focus == index, hot = sh.hover == index;
        if (!c.label.empty() || sh.editing == index) {
            const std::string label = sh.editing == index ? sh.editText + (sh.caret ? "|" : "") : c.label;
            out.push_back(Words{panel::Box{b.x + 20 * s, b.y + 12 * s, b.w - 80 * s, 22 * s}, label, kGold2, 12 * s,
                                true, Align::Left});
        }
        if (focus || hot)
            out.push_back(Words{menuButton(L, int(i)), "•••", kText, 9 * s, true, Align::Center});
        const float bodyY = b.y + (kPicH + 12) * s, bodyX = b.x + 16 * s, bodyW = b.w - 32 * s;
        const bool hasChar = !c.name.empty();
        out.push_back(Words{panel::Box{bodyX, bodyY, bodyW, 26 * s}, hasChar ? c.name : c.login, kText, 19 * s, true,
                            Align::Left});
        const bool once = c.fromSite && !c.hasPassword;  // 3.0.11-beta.181: its password asked once
        const std::string line = c.busy      ? fill(t.busy, std::to_string(c.busyWindow))
                                 : c.refused ? t.refused
                                 : once      ? t.noPassword
                                 : hasChar   ? jobLine(c)
                                             : t.noChar;
        const uint32_t rgb = c.refused ? kRed : c.busy ? kAmber : kMuted;
        out.push_back(
            Words{panel::Box{bodyX, bodyY + 27 * s, bodyW, 20 * s}, line, rgb, 13.5f * s, false, Align::Left});
        if (hasChar)
            out.push_back(Words{panel::Box{bodyX, bodyY + 52 * s, bodyW, 18 * s}, fill(t.loginWord, c.login), kLogin,
                                12.5f * s, false, Align::Left});
        const panel::Box play{b.x + kPlayIn * s, b.y + b.h - (kPlayIn + kPlayH) * s, b.w - 2 * kPlayIn * s, kPlayH * s};
        if (focus && !t.enterKey.empty()) {
            const float pw = panel::wordsWidth(t.play, 15.5f * s, true);
            const float kw = panel::wordsWidth(t.enterKey, 11.5f * s, false);
            const float px = play.x + (play.w - (pw + 10 * s + kw)) / 2;
            out.push_back(Words{panel::Box{px, play.y, pw + 2, play.h}, t.play, kChosenWords, 15.5f * s, true,
                                Align::Left});
            out.push_back(Words{panel::Box{px + pw + 10 * s, play.y, kw + 2, play.h}, t.enterKey, kKey, 11.5f * s,
                                false, Align::Left});
        } else {
            out.push_back(Words{play, t.play, focus || hot ? kChosenWords : kText, 15.5f * s, true, Align::Center});
        }
    }
    if (L.add.w > 0) {
        const panel::Box& a = L.add;
        out.push_back(Words{panel::Box{a.x + a.w / 2 - 28 * L.s, a.y + a.h / 2 - 62 * s, 56 * s, 56 * s}, "+", kGold2,
                            30 * s, false, Align::Center});
        out.push_back(Words{panel::Box{a.x, a.y + a.h / 2 + 4 * s, a.w, 24 * s}, t.add, kText, 16 * s, true,
                            Align::Center});
        out.push_back(Words{panel::Box{a.x + 16 * s, a.y + a.h / 2 + 30 * s, a.w - 32 * s, 40 * s}, t.addHint, kMuted,
                            13 * s, false, Align::Center});
    }
    for (size_t i = rowFrom; i < out.size(); ++i) out[i].row = true;
    if (L.search.w > 0) {  // 3.0.11-beta.189: the search's words or its hint, its ×; nothing found - a line in the row
        const panel::Box field{L.search.x + 34 * s, L.search.y, L.search.w - 34 * s - L.searchClear.w, L.search.h};
        if (sh.search.empty())
            out.push_back(Words{field, t.searchHint, kMuted, 13.5f * s, false, Align::Left});
        else
            out.push_back(Words{field, sh.search + (sh.caret && sh.editing < 0 ? "|" : ""), kText, 13.5f * s, true,
                                Align::Left});
        if (!sh.search.empty())
            out.push_back(Words{L.searchClear, "×", sh.hoverPart == Part::SearchClear ? kGold2 : kMuted, 16 * s,
                                false, Align::Center});
        if (cards.empty() && !sh.search.empty())
            out.push_back(Words{L.row, t.noMatch, kMuted, 16 * s, false, Align::Center});
    }
    if (L.left.w > 0) out.push_back(Words{L.left, "‹", kText, 28 * s, false, Align::Center});
    if (L.right.w > 0) out.push_back(Words{L.right, "›", kText, 28 * s, false, Align::Center});
    // 3.0.11-beta.181; .183: inside their plates
    const auto inRight = [&](const panel::Box& b) { return panel::Box{b.x, b.y, b.w - kLinkPad * s, b.h}; };
    const auto inLeft = [&](const panel::Box& b) {
        return panel::Box{b.x + kLinkPad * s, b.y, b.w - kLinkPad * s, b.h};
    };
    if (L.chars.w > 0)
        out.push_back(Words{inRight(L.chars), t.chars, sh.hoverPart == Part::Chars ? kGold2 : kLink, 13.5f * s, false,
                            Align::Right});
    if (L.newLogin.w > 0) {
        out.push_back(Words{inRight(L.newLogin), t.newLogin, sh.hoverPart == Part::NewLogin ? kGold2 : kLink, 13 * s,
                            false, Align::Right});
        out.push_back(Words{inLeft(L.linkLogin), t.linkLogin, sh.hoverPart == Part::LinkLogin ? kGold2 : kLink,
                            13 * s, false, Align::Left});
    }
    out.push_back(Words{L.other, t.other, kText, 15 * s, true, Align::Center});
    out.push_back(Words{L.create, t.create, kText, 15 * s, true, Align::Center});
    out.push_back(Words{L.exit, t.exit, kText, 15 * s, true, Align::Center});
    return out;
}

CharLayout characterRows(int targetW, int targetH, float scale, const std::vector<int>& charsPerLogin, int first) {
    constexpr float kCardW2 = 200, kCardH2 = 120, kGap2 = 14, kLabelH = 22, kRowGap = 16, kArrow = 28;
    CharLayout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const int count = int(charsPerLogin.size());
    L.first = std::clamp(first, 0, std::max(0, count - kRowsInSight));
    const int inSight = std::min(kRowsInSight, count - L.first);
    const float w = ((kCharsInRow + 1) * kCardW2 + kCharsInRow * kGap2) * s;
    const float rowH = (kLabelH + kCardH2 + kRowGap) * s;
    const float h = (kTitleH + kTitleGap) * s + kArrow * s + std::max(1, inSight) * rowH + kArrow * s;
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    L.title = panel::Box{0, 0, w, kTitleH * s};
    L.back = panel::Box{0, 0, 120 * s, kTitleH * s};
    float y = (kTitleH + kTitleGap) * s;
    if (L.first > 0) L.up = panel::Box{w / 2 - 40 * s, y, 80 * s, kArrow * s};
    y += kArrow * s;
    for (int i = 0; i < inSight; ++i) {
        CharRow r;
        r.label = panel::Box{0, y, w, kLabelH * s};
        const float cy = y + kLabelH * s;
        const int n = std::min(kCharsInRow, std::max(0, charsPerLogin[size_t(L.first + i)]));
        for (int c = 0; c < n; ++c)
            r.cards.push_back(panel::Box{c * (kCardW2 + kGap2) * s, cy, kCardW2 * s, kCardH2 * s});
        r.newChar = panel::Box{n * (kCardW2 + kGap2) * s, cy, kCardW2 * s, kCardH2 * s};
        L.rows.push_back(r);
        y += rowH;
    }
    if (L.first + inSight < count) L.down = panel::Box{w / 2 - 40 * s, y, 80 * s, kArrow * s};
    return L;
}

CharHit characterHit(const CharLayout& L, float x, float y) {
    CharHit h;
    if (L.back.contains(x, y)) {
        h.back = true;
        return h;
    }
    if (L.up.w > 0 && L.up.contains(x, y)) h.up = true;
    if (L.down.w > 0 && L.down.contains(x, y)) h.down = true;
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const CharRow& r = L.rows[i];
        for (size_t c = 0; c < r.cards.size(); ++c)
            if (r.cards[c].contains(x, y)) {
                h.row = L.first + int(i);
                h.card = int(c);
                return h;
            }
        if (r.newChar.contains(x, y)) {
            h.row = L.first + int(i);
            h.newChar = true;
            return h;
        }
    }
    return h;
}

void paintCharacters(panel::Picture& out, const CharLayout& L, const std::vector<CharLogin>& logins,
                     const CharShow& sh) {
    const float s = L.s;
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const int login = L.first + int(i);
        const CharRow& r = L.rows[i];
        for (size_t c = 0; c < r.cards.size(); ++c) {
            const panel::Box& b = r.cards[c];
            const bool hot = sh.hoverRow == login && sh.hoverCard == int(c);
            panel::roundRect(out, b, 12 * s, kCard, 0.94f, hot ? kGold : kWhite, hot ? 0.9f : 0.12f, 1.0f * s);
            const panel::Picture* art = size_t(login) < logins.size() && c < logins[size_t(login)].art.size()
                                            ? logins[size_t(login)].art[c]
                                            : nullptr;
            const panel::Box pic{b.x + 1 * s, b.y + 1 * s, b.h - 2 * s, b.h - 2 * s};  // a square at its left
            if (art)
                classart::cover(out, pic, art->px, art->width, art->height, 11 * s, classart::kLeftCorners);
            else
                panel::roundRect(out, pic, 11 * s, 0x2A2416, 0.9f);
        }
        const bool hot = sh.hoverRow == login && sh.hoverCard < 0;
        panel::roundRect(out, r.newChar, 12 * s, kCard, 0.70f, kWhite, hot ? 0.45f : 0.28f, 1.5f * s);
    }
    for (const panel::Box* a : {&L.up, &L.down})
        if (a->w > 0) panel::roundRect(out, *a, 8 * s, kPill, 0.72f, kWhite, 0.14f, 1.0f * s);
}

std::vector<Words> characterWords(const CharLayout& L, const std::vector<CharLogin>& logins, const CharShow& sh,
                                  const CharTexts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, t.title, kText, 17 * s, true, Align::Center});
    out.push_back(Words{L.back, t.back, sh.hoverBack ? kGold2 : kLink, 13.5f * s, false, Align::Left});
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t login = size_t(L.first) + i;
        if (login >= logins.size()) break;
        const CharLogin& g = logins[login];
        const CharRow& r = L.rows[i];
        out.push_back(Words{r.label, g.hasPassword ? g.login : g.login + " · " + t.noPassword, kMuted, 13 * s, true,
                            Align::Left});
        for (size_t c = 0; c < r.cards.size() && c < g.chars.size(); ++c) {
            const siteapi::SiteChar& ch = g.chars[c];
            const panel::Box& b = r.cards[c];
            const float tx = b.x + b.h + 10 * s, tw = b.w - b.h - 16 * s;
            out.push_back(Words{panel::Box{tx, b.y + 14 * s, tw, 22 * s}, ch.name, kText, 15 * s, true, Align::Left});
            out.push_back(Words{panel::Box{tx, b.y + 40 * s, tw, 18 * s},
                                ch.job + (ch.level > 0 ? " · " + std::to_string(ch.level) : ""), kMuted, 12 * s, false,
                                Align::Left});
            out.push_back(Words{panel::Box{tx, b.y + 62 * s, tw, 18 * s}, capital(ch.map), kLogin, 12 * s, false,
                                Align::Left});
        }
        out.push_back(Words{r.newChar, t.newChar, sh.hoverRow == int(login) && sh.hoverCard < 0 ? kGold2 : kText,
                            14 * s, true, Align::Center});
    }
    if (L.up.w > 0) out.push_back(Words{L.up, "↑", kText, 16 * s, false, Align::Center});
    if (L.down.w > 0) out.push_back(Words{L.down, "↓", kText, 16 * s, false, Align::Center});
    return out;
}

std::vector<MenuItem> menu(const Card& c) {
    if (!c.localRow) return {MenuItem::Edit, MenuItem::GamePassword};  // 3.0.11-beta.181: the site's alone
    std::vector<MenuItem> items{MenuItem::AutoEnter, MenuItem::KeepPassword, MenuItem::Label, MenuItem::Edit,
                                MenuItem::MoveLeft,  MenuItem::MoveRight,    MenuItem::Remove};
    if (c.fromSite) items.push_back(MenuItem::GamePassword);
    return items;
}

MenuLayout menuLayout(const Layout& L, int visible, size_t items) {
    MenuLayout M;
    if (visible < 0 || visible >= int(L.cards.size())) return M;
    const panel::Box& c = L.cards[size_t(visible)];
    const float s = L.s;
    M.x = c.x + 6 * s;
    M.w = c.w - 12 * s;
    M.y = c.y + 40 * s;
    M.h = (2 * kMenuPad + float(items) * kMenuRowH) * s;
    for (size_t i = 0; i < items; ++i)
        M.rows.push_back(panel::Box{M.x + 4 * s, M.y + (kMenuPad + float(i) * kMenuRowH) * s, M.w - 8 * s,
                                    kMenuRowH * s});
    return M;
}

int menuHit(const MenuLayout& M, float x, float y) {
    for (size_t i = 0; i < M.rows.size(); ++i)
        if (M.rows[i].contains(x, y)) return int(i);
    return -1;
}

void paintMenu(panel::Picture& out, const Layout& L, const MenuLayout& M, const MenuShow& sh) {
    const float s = L.s;
    panel::roundRect(out, panel::Box{M.x, M.y, M.w, M.h}, 10 * s, kPill, 0.97f, kWhite, 0.16f, 1.0f * s);
    if (sh.hover >= 0 && sh.hover < int(M.rows.size()))
        panel::roundRect(out, M.rows[size_t(sh.hover)], 7 * s, kWhite, 0.08f);
}

std::vector<Words> menuWords(const MenuLayout& M, const std::vector<MenuItem>& items, const MenuShow& sh,
                             const MenuTexts& t, float scale) {
    std::vector<Words> out;
    const float s = scale;
    for (size_t i = 0; i < items.size() && i < M.rows.size(); ++i) {
        const panel::Box& r = M.rows[i];
        const panel::Box text{r.x + 30 * s, r.y, r.w - 36 * s, r.h};
        const panel::Box tick{r.x + 6 * s, r.y, 20 * s, r.h};
        switch (items[i]) {
            case MenuItem::AutoEnter:
                if (sh.autoEnter) out.push_back(Words{tick, "✓", kGold2, 13 * s, true, Align::Center});
                out.push_back(Words{text, t.autoEnter, kText, 13 * s, false, Align::Left});
                break;
            case MenuItem::KeepPassword:
                if (sh.keepPassword) out.push_back(Words{tick, "✓", kGold2, 13 * s, true, Align::Center});
                out.push_back(Words{text, t.keepPassword, kText, 13 * s, false, Align::Left});
                break;
            case MenuItem::Label: out.push_back(Words{text, t.label, kText, 13 * s, false, Align::Left}); break;
            case MenuItem::Edit: out.push_back(Words{text, t.edit, kText, 13 * s, false, Align::Left}); break;
            case MenuItem::MoveLeft: out.push_back(Words{text, t.moveLeft, kText, 13 * s, false, Align::Left}); break;
            case MenuItem::MoveRight: out.push_back(Words{text, t.moveRight, kText, 13 * s, false, Align::Left}); break;
            case MenuItem::Remove:
                out.push_back(Words{text, sh.removeSure ? t.removeSure : t.remove, kRed, 13 * s, sh.removeSure,
                                    Align::Left});
                break;
            case MenuItem::GamePassword:
                out.push_back(Words{text, t.gamePassword, kText, 13 * s, false, Align::Left});
                break;
        }
    }
    return out;
}

}  // namespace uagfx::loginshelf
