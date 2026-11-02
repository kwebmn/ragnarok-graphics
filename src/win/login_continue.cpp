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
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::logincontinuewin {
namespace {

namespace lco = logincontinue;

struct State {
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
    if (readGameCursor(cx, cy)) s.dropHover = logincard::dropHit(s.drop, float(cx) * s.k, float(cy) * s.k);
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

void frame(double k, int targetWidth, int targetHeight, unsigned codePage) {
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
    }
    const lco::Texts t = texts();
    s.layout = lco::layout(targetWidth, targetHeight, s.scale, t);
    s.show.hover = lco::Part::None;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy))
        s.show.hover = lco::hitTest(s.layout, float(cx) * s.k - float(s.layout.x), float(cy) * s.k - float(s.layout.y));
    s.show.changeOpen = s.dropOpen;
    const lco::Account& a = s.account;
    const std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' +
                            std::to_string(s.layout.w) + ';' + std::to_string(int(s.show.hover)) +
                            (s.dropOpen ? "o" : "") + ';' + a.name + ',' + a.job + ',' + a.map + ',' + a.login + ',' +
                            std::to_string(a.level) + (a.refused ? "r" : "") + '#' +
                            std::to_string(uistyle::generation());
    if (key != s.key) {
        s.key = key;
        if (panel::blank(s.picture, s.layout.w, s.layout.h)) {
            lco::paint(s.picture, s.layout, a, s.show);
            drawWords(s.picture, gdi(lco::words(s.layout, a, s.show, t)));
            s.upload = true;
        }
    }
    if (s.dropOpen) dropFrame(s, shelf);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.k <= 0 || s.picture.width <= 0) return;
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (s.dropOpen && s.dropPicture.width > 0)
        drawPart(record, renderer, s.dropPicture, s.dropTexture, s.dropTexW, s.dropTexH, s.dropUpload, int(s.drop.x),
                 int(s.drop.y), s.k);
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
