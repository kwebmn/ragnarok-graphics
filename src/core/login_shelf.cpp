#include "core/login_shelf.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/class_art.hpp"
#include "core/ui_style.hpp"

namespace uagfx::loginshelf {
namespace {

// At scale 1 (the concept's picture): a card 228 x 338, 20 apart, its picture 196 high; the title's pill 34 high, 18
// over the row; the buttons 42 high, 30 under it, 14 apart (210, 210, 150 wide); the arrows 36 x 56, 16 beside the row
constexpr float kCardW = 228, kCardH = 338, kGap = 20, kPicH = 196, kTitleH = 34, kTitleGap = 18, kRowGap = 30;
constexpr float kButtonH = 42, kButtonGap = 14, kOtherW = 210, kCreateW = 210, kExitW = 150;
constexpr float kArrowW = 36, kArrowH = 56, kArrowGap = 16, kPlayH = 40, kPlayIn = 14, kDotsW = 30, kDotsH = 26;
constexpr float kMenuRowH = 32, kMenuPad = 6;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;
constexpr uint32_t kMuted = 0x9AA3B5, kLogin = 0x7E879A, kRed = 0xFF5A4E, kAmber = 0xF2C46B, kPill = 0x0A0C12;
constexpr uint32_t kChosenWords = 0xF5E6BE, kKey = 0xB9A47A;

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

Layout layout(int targetW, int targetH, float scale, size_t cards, int first) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const int slots = int(cards) + 1;  // the accounts and «Додати акаунт»
    L.first = std::clamp(first, 0, std::max(0, slots - kInSight));
    const int inSight = std::min(kInSight, slots - L.first);
    const float w = (rowWidth(kInSight) + 2 * (kArrowGap + kArrowW)) * s;
    const float h = (kTitleH + kTitleGap + kCardH + kRowGap + kButtonH) * s;
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(h));
    L.x = std::max(0, targetW / 2 - L.w / 2);
    L.y = std::max(0, targetH / 2 - L.h / 2);
    L.title = panel::Box{0, 0, w, kTitleH * s};
    const float top = (kTitleH + kTitleGap) * s, row = rowWidth(inSight) * s, left = (w - row) / 2;
    for (int i = 0; i < inSight; ++i) {
        const panel::Box b{left + i * (kCardW + kGap) * s, top, kCardW * s, kCardH * s};
        if (L.first + i < int(cards))
            L.cards.push_back(b);
        else
            L.add = b;
    }
    const float arrowY = top + (kCardH - kArrowH) * s / 2;
    if (L.first > 0) L.left = panel::Box{left - (kArrowGap + kArrowW) * s, arrowY, kArrowW * s, kArrowH * s};
    if (L.first + inSight < slots) L.right = panel::Box{left + row + kArrowGap * s, arrowY, kArrowW * s, kArrowH * s};
    const float by = top + (kCardH + kRowGap) * s;
    const float buttons = (kOtherW + kCreateW + kExitW + 2 * kButtonGap) * s;
    float bx = (w - buttons) / 2;
    L.other = panel::Box{bx, by, kOtherW * s, kButtonH * s};
    bx += (kOtherW + kButtonGap) * s;
    L.create = panel::Box{bx, by, kCreateW * s, kButtonH * s};
    bx += (kCreateW + kButtonGap) * s;
    L.exit = panel::Box{bx, by, kExitW * s, kButtonH * s};
    return L;
}

panel::Box menuButton(const Layout& L, int visible) {
    if (visible < 0 || visible >= int(L.cards.size())) return {};
    const panel::Box& c = L.cards[size_t(visible)];
    return panel::Box{c.x + c.w - (10 + kDotsW) * L.s, c.y + 10 * L.s, kDotsW * L.s, kDotsH * L.s};
}

Hit hitTest(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.cards.size(); ++i) {
        if (menuButton(L, int(i)).contains(x, y)) return Hit{Part::Menu, L.first + int(i)};
        if (L.cards[i].contains(x, y)) return Hit{Part::Card, L.first + int(i)};
    }
    if (L.add.w > 0 && L.add.contains(x, y)) return Hit{Part::Add, -1};
    if (L.left.w > 0 && L.left.contains(x, y)) return Hit{Part::Left, -1};
    if (L.right.w > 0 && L.right.contains(x, y)) return Hit{Part::Right, -1};
    if (L.other.contains(x, y)) return Hit{Part::Other, -1};
    if (L.create.contains(x, y)) return Hit{Part::Create, -1};
    if (L.exit.contains(x, y)) return Hit{Part::Exit, -1};
    return {};
}

bool classicInstead(size_t accounts) { return accounts == 0; }

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
    const auto arrow = [&](const panel::Box& a, Part p) {
        if (a.w > 0) panel::roundRect(out, a, 10 * s, kPill, sh.hoverPart == p ? 0.9f : 0.72f, kWhite, 0.14f, 1.0f * s);
    };
    arrow(L.left, Part::Left);
    arrow(L.right, Part::Right);
    const auto button = [&](const panel::Box& b, Part p) {
        panel::roundRect(out, b, 9 * s, kWhite, sh.hoverPart == p ? 0.12f : 0.06f, kWhite, 0.16f, 1.0f * s);
    };
    button(L.other, Part::Other);
    button(L.create, Part::Create);
    button(L.exit, Part::Exit);
}

std::vector<Words> words(const Layout& L, const std::vector<Card>& cards, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    const float tw = panel::wordsWidth(t.title, 17 * s, true), hw = panel::wordsWidth(t.titleHint, 14 * s, false);
    const float tx = (float(L.w) - (tw + 8 * s + hw)) / 2;
    out.push_back(Words{panel::Box{tx, L.title.y, tw + 2, L.title.h}, t.title, kText, 17 * s, true, Align::Left});
    out.push_back(Words{panel::Box{tx + tw + 8 * s, L.title.y, hw + 2, L.title.h}, t.titleHint, kMuted, 14 * s, false,
                        Align::Left});
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
        const std::string line = c.busy      ? fill(t.busy, std::to_string(c.busyWindow))
                                 : c.refused ? t.refused
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
    if (L.left.w > 0) out.push_back(Words{L.left, "‹", kText, 28 * s, false, Align::Center});
    if (L.right.w > 0) out.push_back(Words{L.right, "›", kText, 28 * s, false, Align::Center});
    out.push_back(Words{L.other, t.other, kText, 15 * s, true, Align::Center});
    out.push_back(Words{L.create, t.create, kText, 15 * s, true, Align::Center});
    out.push_back(Words{L.exit, t.exit, kText, 15 * s, true, Align::Center});
    return out;
}

std::vector<MenuItem> menu(const Card& c) {
    (void)c;
    return {MenuItem::AutoEnter, MenuItem::KeepPassword, MenuItem::Label,  MenuItem::Edit,
            MenuItem::MoveLeft,  MenuItem::MoveRight,    MenuItem::Remove};
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
        }
    }
    return out;
}

}  // namespace uagfx::loginshelf
