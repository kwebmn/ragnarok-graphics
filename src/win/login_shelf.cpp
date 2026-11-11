#include "win/login_shelf.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/accounts.hpp"
#include "core/login_shelf.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_motion.hpp"
#include "core/ui_style.hpp"
#include "win/accounts_store.hpp"
#include "win/class_pictures.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_logins.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/site_accounts.hpp"
#include "win/site_key.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::loginshelfwin {
namespace {

namespace ls = loginshelf;
constexpr size_t kLabelMost = 24;  // a label's letters at most (the search's too)

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.41: «Полиця» drawn by uaUI (before the world)
    int sheetGen = 0;
    bool sheetShown = false;
    sheet::Model model;
    bool classic = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    unsigned codePage = 0;
    int first = 0;
    std::vector<shelfmerge::Row> rows;  // 3.0.11-beta.181: this computer's and the site's
    std::vector<ls::Card> cards;        // a card a row (3.0.11-beta.189: a row the search leaves)
    // 3.0.11-beta.189 (the owner: «стрелочки, скрол чтобы мотать было интереснее ... поиск», «чтобы мышкой скролить»):
    // a card's row; the search typed; the row's shown place gliding to first, when it last moved; the view to follow
    // the focus (keys, the pad) or the focus to stay in the view (the wheel, the arrows, the bar)
    std::vector<int> cardRow;
    std::string search;
    bool searchShown = false;
    float pos = -1;
    ULONGLONG glidedAt = 0;
    bool followFocus = true;
    ls::Layout layout;
    ls::Show show;
    int menuFor = -1;  // the row whose menu is open
    int padRow = -1;   // 3.0.11-beta.179: the menu's row the pad is on
    ls::MenuShow menuShow;
    ls::MenuLayout menuLayout;
    // 3.0.11-beta.181: «Персонажі» - the site's logins' characters
    bool charsView = false;
    int charFirst = 0;
    std::vector<int> charRows;  // a «Персонажі» row's place in rows
    std::vector<ls::CharLogin> charLogins;
    ls::CharLayout charLayout;
    ls::CharShow charShow;
    int picX = 0, picY = 0, picW = 0, picH = 0;  // the picture's place (the cards' or «Персонажі»'s)
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
    t.noPassword = say(settings::Text::ShelfNoPassword);  // 3.0.11-beta.181
    t.chars = say(settings::Text::ShelfChars);
    t.newLogin = say(settings::Text::ShelfNewLogin);
    t.linkLogin = say(settings::Text::ShelfLinkLogin);
    t.searchHint = say(settings::Text::ShelfSearch);  // 3.0.11-beta.189
    t.noMatch = say(settings::Text::ShelfNoMatch);
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
    t.gamePassword = say(settings::Text::ShelfMenuGamePassword);  // 3.0.11-beta.181
    return t;
}

template <typename W>
std::vector<Words> gdi(const std::vector<W>& in) {
    std::vector<Words> out;
    for (const W& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(int(w.align) == 2 ? DT_CENTER : int(w.align) == 1 ? DT_RIGHT : DT_LEFT), false});
    return out;
}

// The view to the focus (it moved by the keys or the pad)
void keepInSight(State& s) {
    if (s.show.focus < s.first) s.first = s.show.focus;
    if (s.show.focus >= s.first + ls::kInSight) s.first = s.show.focus - ls::kInSight + 1;
    s.first = std::clamp(s.first, 0, ls::mostFirst(s.cards.size(), s.search.empty()));
    s.followFocus = true;
}

// 3.0.11-beta.189: the view moved by the wheel, the arrows or the bar - the focus stays in it (it pulled the view back
// to itself before: the arrows did nothing with the first card focused)
void moveView(State& s, int first) {
    s.first = std::clamp(first, 0, ls::mostFirst(s.cards.size(), s.search.empty()));
    s.followFocus = false;
    s.key.clear();
}

// A card's row (the search leaves some), -1 none
int rowOf(const State& s, int card) { return card >= 0 && card < int(s.cardRow.size()) ? s.cardRow[size_t(card)] : -1; }

void closeMenu(State& s) {
    s.menuFor = -1;
    s.padRow = -1;
    s.menuShow = ls::MenuShow{};
}

accounts::Account* localOf(State& s, int row) {  // the row's account on this computer's shelf, null none
    accounts::Shelf& shelf = accountsstore::shelf();
    if (row < 0 || row >= int(s.rows.size())) return nullptr;
    const int i = s.rows[size_t(row)].local;
    return i >= 0 && i < int(shelf.list.size()) ? &shelf.list[size_t(i)] : nullptr;
}

void commitLabel(State& s, bool keep) {
    const int i = s.show.editing;
    s.show.editing = -1;
    accounts::Account* a = localOf(s, rowOf(s, i));
    if (!keep || !a) return;
    a->label = s.show.editText;
    accountsstore::save();
    LOG_INFO("login: the shelf's %016llx - its label set", (unsigned long long)accounts::loginHash(a->login));
}

// A menu row pressed: what the caller does (the card with its login, a game password changed), or none
Asked pick(State& s, int menuRow) {
    const int i = s.menuFor;
    if (i < 0 || i >= int(s.cards.size())) {
        closeMenu(s);
        return {};
    }
    const std::vector<ls::MenuItem> items = ls::menu(s.cards[size_t(i)]);
    if (menuRow < 0 || menuRow >= int(items.size())) return {};
    const ls::MenuItem item = items[size_t(menuRow)];
    if (item == ls::MenuItem::Edit || item == ls::MenuItem::GamePassword) {
        closeMenu(s);
        return Asked{item == ls::MenuItem::Edit ? Act::Edit : Act::GamePassword, rowOf(s, i), {}};
    }
    accounts::Shelf& shelf = accountsstore::shelf();
    accounts::Account* a = localOf(s, rowOf(s, i));
    if (!a) return {};
    const int local = s.rows[size_t(rowOf(s, i))].local;
    const unsigned long long hash = accounts::loginHash(a->login);
    switch (item) {
        case ls::MenuItem::AutoEnter:
            a->autoEnter = !a->autoEnter;
            LOG_INFO("login: the shelf's %016llx - «Одразу в гру» %s", hash, a->autoEnter ? "on" : "off");
            break;
        case ls::MenuItem::KeepPassword:
            a->keepPassword = !a->keepPassword;
            if (!a->keepPassword) {
                if (!a->password.empty()) SecureZeroMemory(a->password.data(), a->password.size());
                a->password.clear();
            }
            LOG_INFO("login: the shelf's %016llx - its password %s", hash, a->keepPassword ? "kept" : "not kept");
            break;
        case ls::MenuItem::Label:
            s.show.editing = i;
            s.show.editText = a->label;
            closeMenu(s);
            return {};
        case ls::MenuItem::MoveLeft:
        case ls::MenuItem::MoveRight: {
            const int by = item == ls::MenuItem::MoveLeft ? -1 : 1;
            if (accounts::move(shelf, size_t(local), by)) {
                s.show.focus = std::clamp(i + by, 0, std::max(0, int(s.cards.size()) - 1));
                keepInSight(s);
            }
            closeMenu(s);
            break;
        }
        case ls::MenuItem::Remove:
            if (!s.menuShow.removeSure) {
                s.menuShow.removeSure = true;  // a second press takes it off
                s.key.clear();
                return {};
            }
            accounts::remove(shelf, a->login);
            LOG_INFO("login: the shelf's %016llx taken off the shelf", hash);
            closeMenu(s);
            s.show.focus = std::max(0, std::min(s.show.focus, int(s.cards.size()) - 2));
            break;
        default: break;
    }
    accountsstore::save();
    s.key.clear();
    return {};
}

// The row's card: this computer's last character, else the site's highest
ls::Card cardOf(const State& s, const shelfmerge::Row& r, const accounts::Shelf& shelf) {
    ls::Card c;
    c.label = r.label;
    c.login = utf8Of(widened(r.login, s.codePage));
    c.hasPassword = r.hasPassword;
    c.fromSite = r.fromSite;
    c.localRow = r.local >= 0;
    const accounts::Account* a = r.local >= 0 ? &shelf.list[size_t(r.local)] : nullptr;
    if (a) c.refused = a->refused;
    if (a && a->last.slot >= 0) {
        c.name = a->last.name;
        c.job = a->last.job;
        c.map = a->last.map;
        c.level = a->last.level;
        c.art = classpictures::forJob(a->last.jobId);  // 3.0.11-beta.178
        if (!c.art) c.head = classpictures::head(a->last.sex, a->last.hair, a->last.hairColour, int(120 * s.scale));
    } else if (!r.chars.empty()) {  // 3.0.11-beta.181: the site's
        const auto best = std::max_element(r.chars.begin(), r.chars.end(), [](const auto& x, const auto& y) {
            return x.level < y.level;
        });
        c.name = best->name;
        c.job = best->job;
        c.map = best->map;
        c.level = best->level;
        c.art = classpictures::forJob(best->jobId);
    }
    return c;
}

template <typename W>
sheet::Text textOf(const W& w) {
    return sheet::Text{w.box, w.text, w.rgb, w.px, w.bold, false,
                       w.align == ls::Align::Right    ? sheet::Align::Right
                       : w.align == ls::Align::Center ? sheet::Align::Center
                                                      : sheet::Align::Left};
}

// 3.0.18-beta.41 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): «Полиця» and «Персонажі»
// as the markup's data - core loginshelf's sheetOf (the cards cut at the row), the menu over them, the words (the
// cards' within the row)
void model(State& s) {
    sheet::Model& m = s.model;
    m = sheet::Model{};
    if (s.charsView) {
        const ls::CharLayout& L = s.charLayout;
        m.w = float(L.w), m.h = float(L.h);
        ls::CharTexts t;
        t.title = say(settings::Text::ShelfCharsTitle);
        t.back = say(settings::Text::ShelfCharsBack);
        t.newChar = say(settings::Text::ShelfNewChar);
        t.noPassword = say(settings::Text::ShelfNoPasswordShort);
        ls::sheetOfCharacters(m, L, s.charLogins, s.charShow);
        for (const auto& w : ls::characterWords(L, s.charLogins, s.charShow, t)) m.words.push_back(textOf(w));
        return;
    }
    const ls::Layout& L = s.layout;
    m.w = float(L.w), m.h = float(L.h);
    const ls::Texts t = texts();
    ls::sheetOf(m, L, s.cards, s.show, t);
    for (const ls::Words& w : ls::words(L, s.cards, s.show, t)) (w.row ? m.clippedWords : m.words).push_back(textOf(w));
    if (s.menuFor >= 0 && s.menuFor < int(s.cards.size())) {
        const std::vector<ls::MenuItem> items = ls::menu(s.cards[size_t(s.menuFor)]);
        ls::sheetOfMenu(m, L, s.menuLayout, s.menuShow);
        for (const auto& w : ls::menuWords(s.menuLayout, items, s.menuShow, menuTexts(), L.s))
            m.words.push_back(textOf(w));
    }
}

void render(State& s) {
    model(s);
    ++s.sheetGen;
}

// 3.0.11-beta.181: «Персонажі»'s frame - the site's logins with characters (and their rows' places)
void charsFrame(State& s, int targetWidth, int targetHeight) {
    s.charRows.clear();
    s.charLogins.clear();
    std::vector<int> counts;
    for (size_t i = 0; i < s.rows.size(); ++i) {
        const shelfmerge::Row& r = s.rows[i];
        if (!r.fromSite) continue;
        ls::CharLogin g;
        g.login = utf8Of(widened(r.login, s.codePage));
        g.hasPassword = r.hasPassword;
        g.chars = r.chars;
        std::sort(g.chars.begin(), g.chars.end(), [](const auto& a, const auto& b) { return a.slot < b.slot; });
        for (const siteapi::SiteChar& c : g.chars) g.art.push_back(classpictures::forJob(c.jobId));
        counts.push_back(int(g.chars.size()));
        s.charRows.push_back(int(i));
        s.charLogins.push_back(std::move(g));
    }
    s.charLayout = ls::characterRows(targetWidth, targetHeight, s.scale, counts, s.charFirst);
    s.charFirst = s.charLayout.first;
    s.charShow = ls::CharShow{};
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const ls::CharHit h = ls::characterHit(s.charLayout, float(cx) * s.k - float(s.charLayout.x),
                                               float(cy) * s.k - float(s.charLayout.y));
        s.charShow.hoverRow = h.row;
        s.charShow.hoverCard = h.newChar ? -1 : h.card;
        s.charShow.hoverBack = h.back;
    }
    s.picX = s.charLayout.x;
    s.picY = s.charLayout.y;
    s.picW = s.charLayout.w;
    s.picH = s.charLayout.h;
    std::string key = "chars;" + std::to_string(s.picX) + ',' + std::to_string(s.picY) + ',' + std::to_string(s.picW) +
                      ';' + std::to_string(s.charFirst) + ';' + std::to_string(s.charShow.hoverRow) + ',' +
                      std::to_string(s.charShow.hoverCard) + (s.charShow.hoverBack ? "b" : "") + '#' +
                      std::to_string(uistyle::generation());
    for (const ls::CharLogin& g : s.charLogins) {
        key += ';' + g.login + (g.hasPassword ? "p" : "");
        for (const siteapi::SiteChar& c : g.chars) key += ',' + c.name + std::to_string(c.level) + c.map;
    }
    if (uaui::readyAnywhere()) key += 'u';  // 3.0.18-beta.41
    if (key != s.key) {
        s.key = key;
        render(s);
    }
}

}  // namespace

void classic(bool on) {
    State& s = state();
    if (s.classic == on) return;
    s.classic = on;
    closeMenu(s);
    s.show.editing = -1;
    s.charsView = false;
    s.key.clear();
}

bool classicChosen() { return state().classic; }

const shelfmerge::Row* row(int index) {
    const State& s = state();
    return index >= 0 && index < int(s.rows.size()) ? &s.rows[size_t(index)] : nullptr;
}

static void frameInner(double k, int targetWidth, int targetHeight, unsigned codePage) {
    State& s = state();
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.codePage = codePage;
    const accounts::Shelf& shelf = accountsstore::shelf();
    const bool site = !sitekey::token().empty();
    if (site) siteaccounts::refresh(false);  // 3.0.11-beta.181: the site's logins, every 2 minutes
    s.rows = shelfmerge::merge(shelf, siteaccounts::logins());
    if (!site) s.charsView = false;
    if (s.charsView) return charsFrame(s, targetWidth, targetHeight);
    const std::vector<uagfx::gamelogins::Busy> busy = gamelogins::others();  // 3.0.11-beta.178
    // 3.0.11-beta.189: the search's field with more accounts than in sight (or words typed); the cards it leaves
    s.searchShown = int(s.rows.size()) >= ls::kInSight || !s.search.empty();
    if (!s.searchShown) s.search.clear();
    s.cards.clear();
    s.cardRow.clear();
    for (size_t i = 0; i < s.rows.size(); ++i) {
        const shelfmerge::Row& r = s.rows[i];
        ls::Card c = cardOf(s, r, shelf);
        if (!s.search.empty()) {
            std::vector<std::string> words{c.login, c.label, c.name, c.job};
            for (const siteapi::SiteChar& ch : r.chars) words.push_back(ch.name);
            if (!ls::matches(s.search, words)) continue;
        }
        for (const auto& b : busy)
            if (b.login == accounts::loginHash(r.login)) {
                c.busy = true;
                c.busyWindow = b.window;
            }
        s.cards.push_back(c);
        s.cardRow.push_back(int(i));
    }
    if (s.menuFor >= int(s.cards.size())) closeMenu(s);
    if (s.show.editing >= int(s.cards.size())) s.show.editing = -1;
    s.show.focus = std::clamp(s.show.focus, 0, std::max(0, int(s.cards.size()) - 1));
    if (s.followFocus) {
        keepInSight(s);
    } else {  // the view moved by the wheel, the arrows or the bar: the focus into it
        s.first = std::clamp(s.first, 0, ls::mostFirst(s.cards.size(), s.search.empty()));
        if (!s.cards.empty())
            s.show.focus = std::clamp(s.show.focus, s.first, std::min(int(s.cards.size()), s.first + ls::kInSight) - 1);
    }
    const ULONGLONG now = GetTickCount64();  // 3.0.11-beta.189: the row glides to its place
    const float ms = s.glidedAt ? float(std::min<ULONGLONG>(100, now - s.glidedAt)) : 0.0f;
    s.glidedAt = now;
    s.pos = motion::on() ? ls::glide(s.pos, s.first, ms) : float(s.first);
    ls::Scroll sc;
    sc.pos = s.pos;
    sc.search = s.searchShown;
    sc.add = s.search.empty();
    s.layout = ls::layout(targetWidth, targetHeight, s.scale, s.cards.size(), s.first, site, sc);
    s.first = s.layout.target;
    s.menuLayout = s.menuFor >= 0
                       ? ls::menuLayout(s.layout, s.menuFor - s.layout.first,
                                        ls::menu(s.cards[size_t(s.menuFor)]).size())
                       : ls::MenuLayout{};
    if (s.menuFor >= 0 && s.menuLayout.rows.empty()) closeMenu(s);  // scrolled out of sight
    s.show.hover = -1;
    s.show.hoverPart = ls::Part::None;
    s.menuShow.hover = -1;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const float x = float(cx) * s.k - float(s.layout.x), y = float(cy) * s.k - float(s.layout.y);
        if (s.menuFor >= 0) s.menuShow.hover = ls::menuHit(s.menuLayout, x, y);
        if (s.menuShow.hover < 0) {
            const ls::Hit h = ls::hitTest(s.layout, x, y);
            s.show.hoverPart = h.part;
            if (h.part == ls::Part::Card || h.part == ls::Part::Menu) s.show.hover = h.index;
        }
    }
    if (s.menuFor >= 0 && s.menuShow.hover < 0) s.menuShow.hover = s.padRow;  // 3.0.11-beta.179: the pad's row
    if (const accounts::Account* a = s.menuFor >= 0 ? localOf(s, rowOf(s, s.menuFor)) : nullptr) {
        s.menuShow.autoEnter = a->autoEnter;
        s.menuShow.keepPassword = a->keepPassword;
    }
    s.show.caret = (s.show.editing >= 0 || !s.search.empty()) && (GetTickCount64() / 500) % 2 == 0;
    s.show.search = s.search;  // 3.0.11-beta.189
    s.picX = s.layout.x;
    s.picY = s.layout.y;
    s.picW = s.layout.w;
    s.picH = s.layout.h;
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) +
                      ';' + std::to_string(s.first) + ';' + std::to_string(s.show.focus) + ';' +
                      std::to_string(s.show.hover) + ',' + std::to_string(int(s.show.hoverPart)) + ';' +
                      std::to_string(s.menuFor) + ',' + std::to_string(s.menuShow.hover) +
                      (s.menuShow.removeSure ? "!" : "") + (s.menuShow.autoEnter ? "a" : "") +
                      (s.menuShow.keepPassword ? "k" : "") + ';' + std::to_string(s.show.editing) + s.show.editText +
                      (s.show.caret ? "|" : "") + (site ? "S" : "") + '#' + std::to_string(uistyle::generation()) +
                      '@' + std::to_string(int(std::lround(s.pos * 1000))) + '?' + s.search;  // 3.0.11-beta.189
    for (const ls::Card& c : s.cards)
        key += ';' + c.label + ',' + c.name + ',' + c.login + ',' + std::to_string(c.level) + c.map +
               (c.refused ? "r" : "") + (c.art ? "a" : c.head ? "h" : "") + std::to_string(c.busyWindow) +
               (c.hasPassword ? "p" : "") + (c.fromSite ? "s" : "");
    if (uaui::readyAnywhere()) key += 'u';  // 3.0.18-beta.41
    if (key != s.key) {
        s.key = key;
        render(s);
    }
}

void frame(double k, int targetWidth, int targetHeight, unsigned codePage) {
    frameInner(k, targetWidth, targetHeight, codePage);
    State& s = state();
    s.sheet.anywhere = true;
    s.sheetShown = s.sheet.frame("shelf", uaui::readyAnywhere() && s.k > 0 && s.sheetGen > 0, true, s.picX, s.picY,
                                 s.sheetGen, [&](uisheet::Content& c) { uisheet::fromModel(c, s.model, s.sheet); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    state().sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

Asked mouse(UINT m, WPARAM wp, float tx, float ty, bool& took) {
    State& s = state();
    took = false;
    const bool left = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK, right = m == WM_RBUTTONDOWN;
    const int wheel = m == WM_MOUSEWHEEL ? (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1) : 0;  // 3.0.11-beta.189: up - back
    if (s.charsView) {  // 3.0.11-beta.181: «Персонажі»
        const float x = tx - float(s.charLayout.x), y = ty - float(s.charLayout.y);
        if (x < 0 || y < 0 || x >= float(s.charLayout.w) || y >= float(s.charLayout.h)) return {};
        took = true;
        if (wheel) {  // 3.0.11-beta.189: a row a notch
            s.charFirst = std::max(0, s.charFirst + wheel);
            s.key.clear();
            return {};
        }
        if (!left) return {};
        s.key.clear();
        const ls::CharHit h = ls::characterHit(s.charLayout, x, y);
        if (h.back) s.charsView = false;
        if (h.up) s.charFirst = std::max(0, s.charFirst - 1);
        if (h.down) ++s.charFirst;
        if (h.row < 0 || h.row >= int(s.charRows.size())) return {};
        const int r = s.charRows[size_t(h.row)];
        if (h.newChar) return Asked{Act::CharNew, r, {}};
        const ls::CharLogin& g = s.charLogins[size_t(h.row)];
        return h.card >= 0 && h.card < int(g.chars.size()) ? Asked{Act::CharLogin, r, g.chars[size_t(h.card)].name}
                                                           : Asked{};
    }
    const float x = tx - float(s.layout.x), y = ty - float(s.layout.y);
    const bool inside = x >= 0 && y >= 0 && x < float(s.layout.w) && y < float(s.layout.h);
    if (s.menuFor >= 0) {  // the menu first: a press beside it closes it
        const int menuRow = ls::menuHit(s.menuLayout, x, y);
        if (menuRow >= 0) {
            took = true;
            return left ? pick(s, menuRow) : Asked{};
        }
        if (left || right) closeMenu(s), s.key.clear();
    }
    if (s.show.editing >= 0 && (left || right)) commitLabel(s, true);
    if (!inside) return {};
    took = true;
    if (wheel && s.menuFor < 0) {  // 3.0.11-beta.189: a card a notch, the row gliding
        moveView(s, s.first + wheel);
        return {};
    }
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
            s.search.clear();  // 3.0.11-beta.189: after the game the whole shelf again
            return Asked{Act::Login, rowOf(s, h.index), {}};
        case ls::Part::Menu:
            s.show.focus = h.index;
            s.menuFor = s.menuFor == h.index ? -1 : h.index;
            return {};
        case ls::Part::Add: return left ? Asked{Act::Add, -1, {}} : Asked{};
        case ls::Part::Other: return left ? Asked{Act::Other, -1, {}} : Asked{};
        case ls::Part::Create: return left ? Asked{Act::Create, -1, {}} : Asked{};
        case ls::Part::Exit: return left ? Asked{Act::Exit, -1, {}} : Asked{};
        case ls::Part::Left: moveView(s, s.first - 1); return {};  // 3.0.11-beta.189: the focus no longer pulls it back
        case ls::Part::Right: moveView(s, s.first + 1); return {};
        case ls::Part::Bar: moveView(s, ls::barFirst(s.layout, x)); return {};  // 3.0.11-beta.189
        case ls::Part::SearchClear: s.search.clear(); return {};
        case ls::Part::Search: return {};
        case ls::Part::Chars:  // 3.0.11-beta.181
            if (left) s.charsView = true;
            return {};
        case ls::Part::NewLogin: return left ? Asked{Act::NewLogin, -1, {}} : Asked{};
        case ls::Part::LinkLogin: return left ? Asked{Act::LinkLogin, -1, {}} : Asked{};
        case ls::Part::None: return {};
    }
    return {};
}

Asked key(UINT m, WPARAM wp, bool& took) {
    State& s = state();
    took = false;
    if (s.charsView) {  // 3.0.11-beta.181: Esc back to the cards
        if (m == WM_KEYDOWN && wp == VK_ESCAPE) {
            took = true;
            s.charsView = false;
            s.key.clear();
        }
        return {};
    }
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
            const std::string add = typedUtf8(unsigned(wp));
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
    if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_HOME || wp == VK_END || wp == VK_PRIOR || wp == VK_NEXT) {
        took = m == WM_KEYDOWN || m == WM_KEYUP;
        if (down && !s.cards.empty()) {  // 3.0.11-beta.189: Home, End, Page Up and Down too
            const int last = int(s.cards.size()) - 1;
            const int to = wp == VK_HOME    ? 0
                           : wp == VK_END   ? last
                           : wp == VK_PRIOR ? s.show.focus - ls::kInSight
                           : wp == VK_NEXT  ? s.show.focus + ls::kInSight
                                            : s.show.focus + (wp == VK_LEFT ? -1 : 1);
            s.show.focus = std::clamp(to, 0, last);
            keepInSight(s);
            s.key.clear();
        }
        return {};
    }
    if (wp == VK_RETURN || (m == WM_CHAR && wp == '\r')) {  // Enter plays the focused card (the game's own not)
        took = m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR;
        if (down && s.show.focus >= 0 && s.show.focus < int(s.cards.size())) {
            const int row = rowOf(s, s.show.focus);
            s.search.clear();  // 3.0.11-beta.189: after the game the whole shelf again
            return Asked{Act::Login, row, {}};
        }
        return {};
    }
    if (s.searchShown && s.menuFor < 0) {  // 3.0.11-beta.189: the keys typed go into the search
        if (down && wp == VK_ESCAPE && !s.search.empty()) {
            took = true;
            s.search.clear();
            s.key.clear();
            return {};
        }
        if (wp == VK_BACK) {
            took = m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR;
            if (down) {
                while (!s.search.empty() && (uint8_t(s.search.back()) & 0xC0) == 0x80) s.search.pop_back();
                if (!s.search.empty()) s.search.pop_back();
                s.show.focus = 0;
                s.first = 0;
                s.key.clear();
            }
            return {};
        }
        if (m == WM_CHAR && wp >= 0x20 && wp != 0x7F && wp != '\b') {
            took = true;
            size_t letters = 0;
            for (const char c : s.search) letters += (uint8_t(c) & 0xC0) != 0x80;
            if (letters < kLabelMost && !(wp == ' ' && s.search.empty()))
                s.search += typedUtf8(unsigned(wp));
            s.show.focus = 0;  // the first card it leaves
            s.first = 0;
            s.followFocus = true;
            s.key.clear();
            return {};
        }
        return {};
    }
    if (m == WM_CHAR && wp > 0x20 && wp != 0x7F && s.menuFor < 0)  // a login typed: the card for it
        return Asked{Act::Other, -1, {}};
    return {};
}

Asked pad(Pad p, bool& took) {
    State& s = state();
    took = true;
    s.key.clear();
    if (s.charsView) {  // 3.0.11-beta.181: B back to the cards
        if (p == Pad::Back) s.charsView = false;
        return {};
    }
    const int cards = int(s.cards.size());
    if (s.menuFor >= 0) {  // the card's menu: its rows
        const int rows = int(ls::menu(s.cards[size_t(s.menuFor)]).size());
        switch (p) {
            case Pad::Up: s.padRow = std::max(0, s.padRow - 1); return {};
            case Pad::Down: s.padRow = std::min(rows - 1, s.padRow + 1); return {};
            case Pad::Select: return pick(s, std::max(0, s.padRow));
            case Pad::Back:
            case Pad::Menu: closeMenu(s); return {};
            default: return {};
        }
    }
    switch (p) {
        case Pad::Left:
        case Pad::Right:
            s.show.focus = ls::padMove(s.show.focus, cards, p == Pad::Left ? -1 : 1);
            keepInSight(s);
            return {};
        case Pad::Select: return cards > 0 ? Asked{Act::Login, rowOf(s, s.show.focus), {}} : Asked{Act::Add, -1, {}};
        case Pad::Other: return Asked{Act::Other, -1, {}};
        case Pad::Menu:
            if (cards > 0) {
                s.menuFor = s.show.focus;
                s.padRow = 0;
            }
            return {};
        default: took = false; return {};
    }
}

bool menuOpen() { return state().menuFor >= 0; }

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (s.picW <= 0) return false;
    x = s.picX;
    y = s.picY;
    w = s.picW;
    h = s.picH;
    return true;
}

}  // namespace uagfx::win::loginshelfwin
