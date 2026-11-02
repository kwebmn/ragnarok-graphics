#include "win/login_shelf.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "core/accounts.hpp"
#include "core/login_shelf.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "win/accounts_store.hpp"
#include "win/class_pictures.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_logins.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::loginshelfwin {
namespace {

namespace ls = loginshelf;
constexpr size_t kLabelMost = 24;  // a label's letters at most

struct State {
    bool classic = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    unsigned codePage = 0;
    int first = 0;
    std::vector<ls::Card> cards;
    ls::Layout layout;
    ls::Show show;
    int menuFor = -1;  // the account whose menu is open
    ls::MenuShow menuShow;
    ls::MenuLayout menuLayout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

ls::Texts texts() {
    ls::Texts t;
    t.title = say(settings::Text::ShelfTitle);
    t.titleHint = say(settings::Text::ShelfTitleHint);
    t.play = say(settings::Text::ShelfPlay);
    t.enterKey = "Enter";
    t.loginWord = say(settings::Text::ShelfLogin);
    t.add = say(settings::Text::ShelfAdd);
    t.addHint = say(settings::Text::ShelfAddHint);
    t.other = say(settings::Text::ShelfOther);
    t.create = say(settings::Text::ShelfCreate);
    t.exit = say(settings::Text::LoginExit);
    t.refused = say(settings::Text::ShelfRefused);
    t.busy = say(settings::Text::ShelfBusy);
    t.noChar = say(settings::Text::ShelfNoChar);
    return t;
}

ls::MenuTexts menuTexts() {
    ls::MenuTexts t;
    t.autoEnter = say(settings::Text::ShelfMenuAutoEnter);
    t.keepPassword = say(settings::Text::ShelfMenuKeepPassword);
    t.label = say(settings::Text::ShelfMenuLabel);
    t.edit = say(settings::Text::ShelfMenuEdit);
    t.moveLeft = say(settings::Text::ShelfMenuLeft);
    t.moveRight = say(settings::Text::ShelfMenuRight);
    t.remove = say(settings::Text::ShelfMenuRemove);
    t.removeSure = say(settings::Text::ShelfMenuRemoveSure);
    return t;
}

std::vector<Words> gdi(const std::vector<ls::Words>& in) {
    std::vector<Words> out;
    for (const ls::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ls::Align::Center  ? DT_CENTER
                                 : w.align == ls::Align::Right ? DT_RIGHT
                                                               : DT_LEFT),
                            false});
    return out;
}

void keepInSight(State& s) {
    const int slots = int(s.cards.size()) + 1;
    if (s.show.focus < s.first) s.first = s.show.focus;
    if (s.show.focus >= s.first + ls::kInSight) s.first = s.show.focus - ls::kInSight + 1;
    s.first = std::clamp(s.first, 0, std::max(0, slots - ls::kInSight));
}

void closeMenu(State& s) {
    s.menuFor = -1;
    s.menuShow = ls::MenuShow{};
}

void commitLabel(State& s, bool keep) {
    const int i = s.show.editing;
    s.show.editing = -1;
    accounts::Shelf& shelf = accountsstore::shelf();
    if (!keep || i < 0 || i >= int(shelf.list.size())) return;
    shelf.list[size_t(i)].label = s.show.editText;
    accountsstore::save();
    LOG_INFO("login: the shelf's %016llx - its label set",
             (unsigned long long)accounts::loginHash(shelf.list[size_t(i)].login));
}

// A menu row pressed
void pick(State& s, int row) {
    accounts::Shelf& shelf = accountsstore::shelf();
    const int i = s.menuFor;
    if (i < 0 || i >= int(shelf.list.size())) return closeMenu(s);
    accounts::Account& a = shelf.list[size_t(i)];
    const std::vector<ls::MenuItem> items = ls::menu(s.cards[size_t(i)]);
    if (row < 0 || row >= int(items.size())) return;
    const unsigned long long hash = accounts::loginHash(a.login);
    switch (items[size_t(row)]) {
        case ls::MenuItem::AutoEnter:
            a.autoEnter = !a.autoEnter;
            LOG_INFO("login: the shelf's %016llx - «Одразу в гру» %s", hash, a.autoEnter ? "on" : "off");
            break;
        case ls::MenuItem::KeepPassword:
            a.keepPassword = !a.keepPassword;
            if (!a.keepPassword) {
                if (!a.password.empty()) SecureZeroMemory(a.password.data(), a.password.size());
                a.password.clear();
            }
            LOG_INFO("login: the shelf's %016llx - its password %s", hash, a.keepPassword ? "kept" : "not kept");
            break;
        case ls::MenuItem::Label:
            s.show.editing = i;
            s.show.editText = a.label;
            closeMenu(s);
            return;
        case ls::MenuItem::Edit: closeMenu(s); return;  // the caller's (the card with its login)
        case ls::MenuItem::MoveLeft:
        case ls::MenuItem::MoveRight: {
            const int by = items[size_t(row)] == ls::MenuItem::MoveLeft ? -1 : 1;
            if (accounts::move(shelf, size_t(i), by)) {
                s.show.focus = i + by;
                keepInSight(s);
            }
            closeMenu(s);
            break;
        }
        case ls::MenuItem::Remove:
            if (!s.menuShow.removeSure) {
                s.menuShow.removeSure = true;  // a second press takes it off
                s.key.clear();
                return;
            }
            accounts::remove(shelf, a.login);
            LOG_INFO("login: the shelf's %016llx taken off the shelf", hash);
            closeMenu(s);
            s.show.focus = std::max(0, std::min(s.show.focus, int(shelf.list.size()) - 1));
            break;
    }
    accountsstore::save();
    s.key.clear();
}

void render(State& s) {
    const ls::Layout& L = s.layout;
    if (!panel::blank(s.picture, L.w, L.h)) return;
    const ls::Texts t = texts();
    ls::paint(s.picture, L, s.cards, s.show, t);
    drawWords(s.picture, gdi(ls::words(L, s.cards, s.show, t)));
    if (s.menuFor >= 0 && s.menuFor < int(s.cards.size())) {
        const std::vector<ls::MenuItem> items = ls::menu(s.cards[size_t(s.menuFor)]);
        ls::paintMenu(s.picture, L, s.menuLayout, s.menuShow);
        drawWords(s.picture, gdi(ls::menuWords(s.menuLayout, items, s.menuShow, menuTexts(), L.s)));
    }
    s.upload = true;
}

}  // namespace

void classic(bool on) {
    State& s = state();
    if (s.classic == on) return;
    s.classic = on;
    closeMenu(s);
    s.show.editing = -1;
    s.key.clear();
}

bool classicChosen() { return state().classic; }

void frame(double k, int targetWidth, int targetHeight, unsigned codePage) {
    State& s = state();
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.codePage = codePage;
    const accounts::Shelf& shelf = accountsstore::shelf();
    const std::vector<uagfx::gamelogins::Busy> busy = gamelogins::others();  // 3.0.11-beta.178
    s.cards.clear();
    for (const accounts::Account& a : shelf.list) {
        ls::Card c;
        c.label = a.label;
        c.name = a.last.name;
        c.job = a.last.job;
        c.map = a.last.map;
        c.level = a.last.level;
        c.login = utf8Of(widened(a.login, codePage));
        c.refused = a.refused;
        c.hasPassword = !a.password.empty();
        for (const auto& b : busy)
            if (b.login == accounts::loginHash(a.login)) {
                c.busy = true;
                c.busyWindow = b.window;
            }
        if (a.last.slot >= 0) {  // 3.0.11-beta.178: its class's portrait, else its head
            c.art = classpictures::forJob(a.last.jobId);
            if (!c.art) c.head = classpictures::head(a.last.sex, a.last.hair, a.last.hairColour, int(120 * s.scale));
        }
        s.cards.push_back(c);
    }
    if (s.menuFor >= int(s.cards.size())) closeMenu(s);
    if (s.show.editing >= int(s.cards.size())) s.show.editing = -1;
    s.show.focus = std::clamp(s.show.focus, 0, std::max(0, int(s.cards.size()) - 1));
    keepInSight(s);
    s.layout = ls::layout(targetWidth, targetHeight, s.scale, s.cards.size(), s.first);
    s.first = s.layout.first;
    s.menuLayout = s.menuFor >= 0
                       ? ls::menuLayout(s.layout, s.menuFor - s.first, ls::menu(s.cards[size_t(s.menuFor)]).size())
                       : ls::MenuLayout{};
    if (s.menuFor >= 0 && s.menuLayout.rows.empty()) closeMenu(s);  // scrolled out of sight
    s.show.hover = -1;
    s.show.hoverPart = ls::Part::None;
    s.menuShow.hover = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const float x = float(cx) * s.k - float(s.layout.x), y = float(cy) * s.k - float(s.layout.y);
        if (s.menuFor >= 0) s.menuShow.hover = ls::menuHit(s.menuLayout, x, y);
        if (s.menuShow.hover < 0) {
            const ls::Hit h = ls::hitTest(s.layout, x, y);
            s.show.hoverPart = h.part;
            if (h.part == ls::Part::Card || h.part == ls::Part::Menu) s.show.hover = h.index;
        }
    }
    if (s.menuFor >= 0) {
        s.menuShow.autoEnter = shelf.list[size_t(s.menuFor)].autoEnter;
        s.menuShow.keepPassword = shelf.list[size_t(s.menuFor)].keepPassword;
    }
    s.show.caret = s.show.editing >= 0 && (GetTickCount64() / 500) % 2 == 0;
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) +
                      ';' + std::to_string(s.first) + ';' + std::to_string(s.show.focus) + ';' +
                      std::to_string(s.show.hover) + ',' + std::to_string(int(s.show.hoverPart)) + ';' +
                      std::to_string(s.menuFor) + ',' + std::to_string(s.menuShow.hover) +
                      (s.menuShow.removeSure ? "!" : "") + (s.menuShow.autoEnter ? "a" : "") +
                      (s.menuShow.keepPassword ? "k" : "") + ';' + std::to_string(s.show.editing) + s.show.editText +
                      (s.show.caret ? "|" : "") + '#' + std::to_string(uistyle::generation());
    for (const ls::Card& c : s.cards)
        key += ';' + c.label + ',' + c.name + ',' + c.login + ',' + std::to_string(c.level) + c.map +
               (c.refused ? "r" : "") + (c.art ? "a" : c.head ? "h" : "") + std::to_string(c.busyWindow);
    if (key != s.key) {
        s.key = key;
        render(s);
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.k <= 0 || s.picture.width <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
}

Asked mouse(UINT m, float tx, float ty, bool& took) {
    State& s = state();
    took = false;
    const float x = tx - float(s.layout.x), y = ty - float(s.layout.y);
    const bool inside = x >= 0 && y >= 0 && x < float(s.layout.w) && y < float(s.layout.h);
    const bool left = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK, right = m == WM_RBUTTONDOWN;
    if (s.menuFor >= 0) {  // the menu first: a press beside it closes it
        const int row = ls::menuHit(s.menuLayout, x, y);
        if (row >= 0) {
            took = true;
            if (left) pick(s, row);
            if (left && row < int(ls::menu(s.cards[size_t(s.menuFor)]).size()) &&
                ls::menu(s.cards[size_t(s.menuFor)])[size_t(row)] == ls::MenuItem::Edit)
                return Asked{Act::Edit, s.menuFor >= 0 ? s.menuFor : s.show.focus};
            return {};
        }
        if (left || right) closeMenu(s), s.key.clear();
    }
    if (s.show.editing >= 0 && (left || right)) commitLabel(s, true);
    if (!inside) return {};
    took = true;
    if (!left && !right) return {};
    const ls::Hit h = ls::hitTest(s.layout, x, y);
    s.key.clear();
    switch (h.part) {
        case ls::Part::Card:
            s.show.focus = h.index;
            if (right) {
                s.menuFor = h.index;
                return {};
            }
            return Asked{Act::Login, h.index};
        case ls::Part::Menu:
            s.show.focus = h.index;
            s.menuFor = s.menuFor == h.index ? -1 : h.index;
            return {};
        case ls::Part::Add: return left ? Asked{Act::Add, -1} : Asked{};
        case ls::Part::Other: return left ? Asked{Act::Other, -1} : Asked{};
        case ls::Part::Create: return left ? Asked{Act::Create, -1} : Asked{};
        case ls::Part::Exit: return left ? Asked{Act::Exit, -1} : Asked{};
        case ls::Part::Left: s.first = std::max(0, s.first - 1); return {};
        case ls::Part::Right: ++s.first; return {};
        case ls::Part::None: return {};
    }
    return {};
}

Asked key(UINT m, WPARAM wp, bool& took) {
    State& s = state();
    took = false;
    if (s.show.editing >= 0) {  // the label typed: every key is its
        took = m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR || m == WM_SYSKEYDOWN;
        if (m == WM_KEYDOWN && wp == VK_RETURN) commitLabel(s, true);
        if (m == WM_KEYDOWN && wp == VK_ESCAPE) commitLabel(s, false);
        if (m == WM_KEYDOWN && wp == VK_BACK) {
            std::string& t = s.show.editText;
            while (!t.empty() && (uint8_t(t.back()) & 0xC0) == 0x80) t.pop_back();
            if (!t.empty()) t.pop_back();
        }
        if (m == WM_CHAR && wp >= 0x20 && wp != 0x7F) {
            const std::wstring one(1, wchar_t(wp));
            const std::string add = utf8Of(one);
            size_t letters = 0;
            for (const char c : s.show.editText) letters += (uint8_t(c) & 0xC0) != 0x80;
            if (letters < kLabelMost) s.show.editText += add;
        }
        s.key.clear();
        return {};
    }
    if (s.menuFor >= 0 && m == WM_KEYDOWN && wp == VK_ESCAPE) {
        took = true;
        closeMenu(s);
        s.key.clear();
        return {};
    }
    const bool down = m == WM_KEYDOWN;
    if (wp == VK_LEFT || wp == VK_RIGHT) {
        took = m == WM_KEYDOWN || m == WM_KEYUP;
        if (down && !s.cards.empty()) {
            s.show.focus = std::clamp(s.show.focus + (wp == VK_LEFT ? -1 : 1), 0, int(s.cards.size()) - 1);
            keepInSight(s);
            s.key.clear();
        }
        return {};
    }
    if (wp == VK_RETURN || (m == WM_CHAR && wp == '\r')) {  // Enter plays the focused card (the game's own not)
        took = m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR;
        if (down && s.show.focus >= 0 && s.show.focus < int(s.cards.size())) return Asked{Act::Login, s.show.focus};
        return {};
    }
    if (m == WM_CHAR && wp > 0x20 && wp != 0x7F && s.menuFor < 0)  // a login typed: the card for it
        return Asked{Act::Other, -1};
    return {};
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (s.layout.w <= 0) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::loginshelfwin
