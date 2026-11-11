#include "win/login_continue.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "core/accounts.hpp"
#include "core/login_card.hpp"
#include "core/login_continue.hpp"
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
#include "core/class_art.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::logincontinuewin {
namespace {

namespace lco = logincontinue;

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.40: the card drawn by uaUI (before the world)
    int sheetGen = 0;
    bool sheetShown = false;
    std::vector<Words> words;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    unsigned codePage = 0;
    std::string chosen;  // the account picked in «Змінити акаунт» (the game's words; "" the one used last)
    int index = -1;      // the account shown: its place on the shelf
    lco::Layout layout;
    lco::Show show;
    lco::Account account;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    // «Змінити акаунт»'s list: its own picture under (or over) the link
    bool dropOpen = false;
    logincard::DropLayout drop;  // in the target's pixels
    int dropHover = -1;
    std::vector<logincard::DropRow> rows;
    std::string dropKey;
    panel::Picture dropPicture;
    uint32_t dropTexture = 0;
    int dropTexW = 0, dropTexH = 0;
    bool dropUpload = false;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

lco::Texts texts() {
    lco::Texts t;
    t.heading = say(settings::Text::ContinueHeading);
    t.play = say(settings::Text::ShelfPlay);
    t.enterKey = "Enter";
    t.loginWord = say(settings::Text::ShelfLogin);
    t.otherChar = say(settings::Text::ContinueOtherChar);
    t.change = say(settings::Text::ContinueChange);
    t.create = say(settings::Text::ShelfCreate);
    t.noChar = say(settings::Text::ShelfNoChar);
    t.refused = say(settings::Text::ShelfRefused);
    t.busy = say(settings::Text::ShelfBusy);
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

// The account shown: the one picked, else the one used last
int shownIndex(const State& s, const accounts::Shelf& shelf) {
    int best = -1;
    for (size_t i = 0; i < shelf.list.size(); ++i) {
        if (!s.chosen.empty() && accounts::loginHash(shelf.list[i].login) == accounts::loginHash(s.chosen))
            return int(i);
        if (best < 0 || shelf.list[i].usedAt > shelf.list[size_t(best)].usedAt) best = int(i);
    }
    return best;
}

void dropFrame(State& s, const accounts::Shelf& shelf) {
    s.rows.clear();
    for (const accounts::Account& a : shelf.list) {
        logincard::DropRow r;
        r.login = utf8Of(widened(a.login, s.codePage));
        if (!a.last.name.empty())
            r.line = a.last.name + " · " + a.last.job + (a.last.level > 0 ? " " + std::to_string(a.last.level) : "");
        s.rows.push_back(r);
    }
    const float x = float(s.layout.x) + s.layout.change.x;
    float y = float(s.layout.y) + s.layout.change.y + s.layout.change.h + 4 * s.scale;
    s.drop = logincard::dropAt(x, y, 300 * s.scale, s.rows.size(), s.scale);
    if (y + s.drop.h > float(s.targetH) - 90 * s.scale) {  // no room under it: over the link
        y = float(s.layout.y) + s.layout.change.y - 4 * s.scale - s.drop.h;
        s.drop = logincard::dropAt(x, std::max(0.0f, y), 300 * s.scale, s.rows.size(), s.scale);
    }
    s.dropHover = -1;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) s.dropHover = logincard::dropHit(s.drop, float(cx) * s.k, float(cy) * s.k);
    std::string key = std::to_string(int(s.drop.x)) + ',' + std::to_string(int(s.drop.y)) + ';' +
                      std::to_string(s.dropHover) + '#' + std::to_string(uistyle::generation());
    for (const logincard::DropRow& r : s.rows) key += ';' + r.login + ',' + r.line;
    if (key == s.dropKey) return;
    s.dropKey = key;
    if (!panel::blank(s.dropPicture, int(s.drop.w + 0.5f), int(s.drop.h + 0.5f))) return;
    logincard::paintDrop(s.dropPicture, s.drop, s.dropHover, s.scale);
    drawWords(s.dropPicture,
              gdi(logincard::dropWords(s.drop, s.rows, s.dropHover, say(settings::Text::LoginOtherLogin), s.scale)));
    s.dropUpload = true;
}

}  // namespace

static void frameInner(double k, int targetWidth, int targetHeight, unsigned codePage) {
    State& s = state();
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.codePage = codePage;
    const accounts::Shelf& shelf = accountsstore::shelf();
    s.index = shownIndex(s, shelf);
    s.account = lco::Account{};
    if (s.index >= 0) {
        const accounts::Account& a = shelf.list[size_t(s.index)];
        s.account.name = a.last.name;
        s.account.job = a.last.job;
        s.account.map = a.last.map;
        s.account.level = a.last.level;
        s.account.login = utf8Of(widened(a.login, codePage));
        s.account.refused = a.refused;
        for (const auto& b : gamelogins::others())  // 3.0.11-beta.178: played in another game window
            if (b.login == accounts::loginHash(a.login)) s.account.busyWindow = b.window;
        if (a.last.slot >= 0) {  // 3.0.11-beta.178: its class's portrait, else its head
            s.account.art = classpictures::forJob(a.last.jobId);
            if (!s.account.art)
                s.account.head = classpictures::head(a.last.sex, a.last.hair, a.last.hairColour, int(150 * s.scale));
        }
    }
    const lco::Texts t = texts();
    s.layout = lco::layout(targetWidth, targetHeight, s.scale, t);
    s.show.hover = lco::Part::None;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.show.hover = lco::hitTest(s.layout, float(cx) * s.k - float(s.layout.x), float(cy) * s.k - float(s.layout.y));
    s.show.changeOpen = s.dropOpen;
    const lco::Account& a = s.account;
    const std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' +
                            std::to_string(s.layout.w) + ';' + std::to_string(int(s.show.hover)) +
                            (s.dropOpen ? "o" : "") + ';' + a.name + ',' + a.job + ',' + a.map + ',' + a.login + ',' +
                            std::to_string(a.level) + (a.refused ? "r" : "") + (a.art ? "a" : a.head ? "h" : "") +
                            std::to_string(a.busyWindow) +
                            '#' +
                            std::to_string(uistyle::generation()) + (uaui::readyAnywhere() ? "u" : "");
    if (key != s.key) {
        s.key = key;
        if (uaui::readyAnywhere()) {  // 3.0.18-beta.40: uaUI's card - its words kept, no picture
            s.words = gdi(lco::words(s.layout, a, s.show, t));
            ++s.sheetGen;
        } else if (panel::blank(s.picture, s.layout.w, s.layout.h)) {
            lco::paint(s.picture, s.layout, a, s.show);
            drawWords(s.picture, gdi(lco::words(s.layout, a, s.show, t)));
            s.upload = true;
        }
    }
    if (s.dropOpen) dropFrame(s, shelf);
}

// 3.0.18-beta.40 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): «Продовжити» in RmlUi's
// markup, line by line as core logincontinue paints it - its glass, the class's portrait (or the head on its dark
// gold ground), «Грати», ▾ after «Змінити акаунт»; the words where core logincontinue places them; ▾'s list over it
static void fillSheet(State& s, uisheet::Content& c) {
    const lco::Layout& L = s.layout;
    const lco::Account& a = s.account;
    const float sc = L.s;
    constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kLink = 0x8CB8FF;
    constexpr float kTriangle = 16;  // core logincontinue's
    c.w = float(L.w), c.h = float(L.h);
    c.rect(c.under, panel::Box{0, 0, float(L.w), float(L.h)}, 14 * sc, kCard, std::max(0.94f, uistyle::window()),
           kWhite, 0.12f, 1.0f * sc);
    const panel::Box pic{1 * sc, 1 * sc, L.art.w - 1 * sc, L.art.h - 2 * sc};
    const int pw = std::max(1, int(std::ceil(pic.w))), ph = std::max(1, int(std::ceil(pic.h)));
    if (a.art) {  // the class's portrait, its left corners round
        const std::string key = "art" + std::to_string(uintptr_t(a.art)) + ':' + std::to_string(pw) + 'x' +
                                std::to_string(ph);
        const std::string src = s.sheet.made(key, pw, ph, [&](panel::Picture& p) {
            classart::cover(p, panel::Box{0, 0, float(pw), float(ph)}, a.art->px, a.art->width, a.art->height,
                            13 * sc, classart::kLeftCorners);
        });
        c.icons.push_back(uisheet::Icon{pic.x, pic.y, float(pw), float(ph), 1, src});
    } else {
        c.rect(c.under, pic, 13 * sc, 0x2A2416, 0.9f);
        c.rect(c.under, panel::Box{pic.x + pic.w - 14 * sc, pic.y, 14 * sc, pic.h}, 0, 0x2A2416, 0.9f);
        if (a.head && a.head->width > 0)
            c.icons.push_back(uisheet::Icon{std::round(pic.x + (pic.w - float(a.head->width)) / 2),
                                            std::round(pic.y + (pic.h - float(a.head->height)) / 2),
                                            float(a.head->width), float(a.head->height), 1, s.sheet.icon(a.head)});
    }
    c.rect(c.under, L.play, 9 * sc, kGold, s.show.hover == lco::Part::Play ? 0.36f : 0.26f, kGold, 0.9f, 1.0f * sc);
    {  // ▾ after «Змінити акаунт»'s words
        const uint32_t rgb = s.show.hover == lco::Part::Change || s.show.changeOpen ? kGold2 : kLink;
        const int side = std::max(4, int(std::ceil(11 * sc)));
        const std::string src = s.sheet.made("tri" + std::to_string(side) + ':' + std::to_string(rgb), side, side,
                                             [&](panel::Picture& p) {
                                                 logincard::paintTriangle(p, float(side) / 2, float(side) / 2, 9 * sc,
                                                                          rgb, 0.95f);
                                             });
        c.icons.push_back(uisheet::Icon{std::round(L.change.x + L.change.w - kTriangle * sc / 2 - float(side) / 2),
                                        std::round(L.change.y + L.change.h / 2 + 1 * sc - float(side) / 2),
                                        float(side), float(side), 1, src});
    }
    uisheet::sayGdi(c, s.words);
    if (s.dropOpen && s.drop.w > 0) {  // 3.0.18-beta.45: ▾'s list over it, as core logincard paints it
        const float x = s.drop.x - float(s.layout.x), y = s.drop.y - float(s.layout.y);
        c.rect(c.topBoxes, panel::Box{x, y, s.drop.w, s.drop.h}, 10 * sc, 0x161B26, 0.98f, kWhite, 0.14f, 1.0f * sc);
        if (s.dropHover >= 0 && s.dropHover < int(s.drop.rows.size())) {
            const panel::Box& r = s.drop.rows[size_t(s.dropHover)];
            c.rect(c.topBoxes, panel::Box{x + r.x - s.drop.x, y + r.y - s.drop.y, r.w, r.h}, 7 * sc, kGold, 0.16f);
        }
        c.sayInto(c.topWords,
                  logincard::dropWords(s.drop, s.rows, s.dropHover, say(settings::Text::LoginOtherLogin), s.scale), x,
                  y);
    }
}

void frame(double k, int targetWidth, int targetHeight, unsigned codePage) {
    frameInner(k, targetWidth, targetHeight, codePage);
    State& s = state();
    s.sheet.anywhere = true;
    const int gen = s.sheetGen ^ (s.dropOpen ? (s.dropHover + 2) * 7919 : 0);  // ▾'s list and its row lit too
    s.sheetShown = s.sheet.frame("login continue", uaui::readyAnywhere() && s.k > 0 && s.sheetGen > 0, true,
                                 s.layout.x, s.layout.y, gen, [&](uisheet::Content& c) { fillSheet(s, c); });
}

void hideSheet() { state().sheet.frame("login continue", false, true, 0, 0, 0, [](uisheet::Content&) {}); }

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    state().sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

Asked mouse(UINT m, float tx, float ty, bool& took) {
    State& s = state();
    took = false;
    const bool left = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK, press = left || m == WM_RBUTTONDOWN;
    if (s.dropOpen) {
        const int row = logincard::dropHit(s.drop, tx, ty);
        const bool over = tx >= s.drop.x && ty >= s.drop.y && tx < s.drop.x + s.drop.w && ty < s.drop.y + s.drop.h;
        if (over) {
            took = true;
            if (!left || row < 0) return {};
            s.dropOpen = false;
            s.key.clear();
            if (row == int(s.rows.size())) return Asked{Act::Other, -1};  // «+ Інший логін»
            const accounts::Shelf& shelf = accountsstore::shelf();
            if (row < int(shelf.list.size())) {
                s.chosen = shelf.list[size_t(row)].login;
                LOG_INFO("login: «Продовжити» shows the shelf's %016llx",
                         (unsigned long long)accounts::loginHash(s.chosen));
            }
            return {};
        }
        if (press) {
            s.dropOpen = false;
            s.key.clear();
        }
    }
    const float x = tx - float(s.layout.x), y = ty - float(s.layout.y);
    if (x < 0 || y < 0 || x >= float(s.layout.w) || y >= float(s.layout.h)) return {};
    took = true;
    if (!left) return {};
    switch (lco::hitTest(s.layout, x, y)) {
        case lco::Part::Play: return Asked{Act::Play, s.index};
        case lco::Part::OtherChar: return Asked{Act::OtherChar, s.index};
        case lco::Part::Change:
            s.dropOpen = !s.dropOpen;
            s.key.clear();
            s.dropKey.clear();
            return {};
        case lco::Part::Create: return Asked{Act::Create, -1};
        case lco::Part::None: return {};
    }
    return {};
}

Asked key(UINT m, WPARAM wp, bool& took) {
    State& s = state();
    took = false;
    if (s.dropOpen && m == WM_KEYDOWN && wp == VK_ESCAPE) {
        took = true;
        s.dropOpen = false;
        s.key.clear();
        return {};
    }
    if (wp == VK_RETURN || (m == WM_CHAR && wp == '\r')) {  // Enter plays (the game's own not)
        took = m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR;
        if (m == WM_KEYDOWN && s.index >= 0) return Asked{Act::Play, s.index};
        return {};
    }
    if (m == WM_CHAR && wp > 0x20 && wp != 0x7F) return Asked{Act::Other, -1};  // a login typed: the card for it
    return {};
}

void nextAccount() {
    State& s = state();
    const accounts::Shelf& shelf = accountsstore::shelf();
    if (shelf.list.empty()) return;
    const int next = (std::max(0, s.index) + 1) % int(shelf.list.size());
    s.chosen = shelf.list[size_t(next)].login;
    s.dropOpen = false;
    s.key.clear();
}

int shown() { return state().index; }

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (s.layout.w <= 0) return false;
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::logincontinuewin
