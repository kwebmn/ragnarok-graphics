#include "win/reference_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "core/reference_pages.hpp"
#include "core/reference_window.hpp"
#include "core/settings/strings.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"

namespace uagfx::win::referencewin {
namespace {

using settings::Text;

constexpr size_t kMostTyped = 40;

struct Entry {  // a page left for another: what «Назад» brings back
    refpages::Link at;
    std::string query, name;
    int scroll = 0;
    unsigned opened = 0;  // its long sections opened
};

struct State {
    bool enabled = false, open = false, shown = false, dirty = true, upload = true, wantPage = true;
    WPARAM skipChar = 0;  // the character of the key that opened or closed it: neither typed nor the game's
    std::string query;    // typed (ASCII: the names are English)
    refpages::Link at;    // the page: Go::None - the search's (the first screen when nothing is typed)
    unsigned opened = 0;  // the card's long sections opened (refpages::Part bits)
    std::vector<Entry> back;
    refpages::Page page;
    std::vector<refwin::Placed> rows;
    int total = 0, scroll = 0;
    refwin::Layout layout;
    refwin::Under hover;
    std::shared_ptr<const refindex::Index> index;
    std::shared_ptr<const reflive::Online> online;  // 3.0.11-beta.10: the players now (null: none fresh)
    std::string here;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, targetW = 0, targetH = 0;
    float k = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(Text t) { return settings::tr(startup().lang, t); }

void makePage(State& s) {
    const settings::Lang l = startup().lang;
    if (!s.index) {
        s.page = refpages::Page{};
        s.page.name = say(Text::RefTitle);
        refpages::Row r;
        r.style = refpages::Style::Note;
        r.left = say(Text::RefNoData);
        s.page.rows.push_back(r);
    } else if (s.at.go != refpages::Go::None) {
        refpages::Context c;
        c.here = s.here;
        int w = 0, h = 0;  // 3.0.11-beta.8: the routes start from the part of the map one is in
        if (!readPlayerCell(c.x, c.y, w, h)) c.x = c.y = -1;
        c.follow.can = minimapproto::canFollow();  // 3.0.11-beta.9: «вести ›» with the new minimap on
        minimapproto::following(c.follow.map, c.follow.walk);
        if (s.online) c.players = &s.online->maps;  // 3.0.11-beta.10
        s.page = refpages::open(*s.index, l, s.at, c, s.opened);
    } else if (!s.query.empty()) {
        s.page = refpages::found(*s.index, l, s.query);
    } else {
        s.page = refpages::home(*s.index, l, s.online ? s.online->online : 0);
    }
    s.rows = refwin::place(s.layout, s.page, s.total);
    s.scroll = refwin::clampScroll(s.scroll, s.total, s.layout.bodyH);
    s.hover = refwin::Under{};
    s.wantPage = false;
    s.dirty = true;
}

void close(State& s);

void go(State& s, const refpages::Link& to) {
    if (to.go == refpages::Go::Expand) {  // «усі N ›»: the section in full, in place
        s.opened |= 1u << unsigned(to.id);
        makePage(s);
        return;
    }
    if (to.go == refpages::Go::Follow) {  // 3.0.11-beta.9: the way on the minimap - the window gives way to it
        std::string target;
        bool walk = true;
        if (minimapproto::following(target, walk) && target == to.map && walk == (to.id == 0)) {
            minimapproto::unfollow();
            makePage(s);  // «вести ›» again
        } else {
            minimapproto::follow(to.map, to.id == 0);
            close(s);
        }
        return;
    }
    s.back.push_back(Entry{s.at, s.query, s.page.name, s.scroll, s.opened});
    s.at = to;
    s.scroll = 0;
    s.opened = 0;
    makePage(s);
}

void goBack(State& s) {
    if (s.back.empty()) return;
    const Entry e = s.back.back();
    s.back.pop_back();
    s.at = e.at;
    s.query = e.query;
    s.scroll = e.scroll;
    s.opened = e.opened;
    makePage(s);
}

void typed(State& s) {  // the query changed: its results (a card left for them comes back by «Назад»)
    if (s.at.go != refpages::Go::None) {
        s.back.push_back(Entry{s.at, "", s.page.name, s.scroll, s.opened});
        s.at = refpages::Link{};
        s.opened = 0;
    }
    s.scroll = 0;
    makePage(s);
}

void scrollBy(State& s, int pixels) {
    const int next = refwin::clampScroll(s.scroll + pixels, s.total, s.layout.bodyH);
    if (next == s.scroll) return;
    s.scroll = next;
    s.hover = refwin::Under{};
    s.dirty = true;
}

void openWindow(State& s, const refpages::Link& at) {
    s.open = true;
    s.query.clear();
    s.back.clear();
    s.at = at;
    s.scroll = 0;
    s.opened = 0;
    s.wantPage = true;
    s.dirty = true;
}

void close(State& s) {
    s.open = false;
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, false);
    s.shown = false;
}

void render(State& s) {
    const settings::Lang l = startup().lang;
    refwin::Look look{s.hover, !s.back.empty()};
    refwin::paint(s.picture, s.layout, s.page, s.rows, s.scroll, s.total, look);
    refwin::Texts t;
    t.title = say(Text::RefTitle);
    if (!s.back.empty()) t.back = settings::fmt(settings::tr(l, Text::RefBack), {s.back.back().name});
    t.query = s.query;
    t.hint = say(Text::RefSearchHint);
    if (s.index)
        t.footer = settings::fmt(settings::tr(l, Text::RefFooter),
                                 {s.index->date.substr(0, 10), s.index->commit.substr(0, 8)});
    std::vector<Words> gdi;
    for (const refwin::Words& w : refwin::words(s.layout, s.page, s.rows, s.scroll, t, look)) {
        const UINT align = w.align == refwin::Align::Right ? DT_RIGHT : w.align == refwin::Align::Center ? DT_CENTER
                                                                                                          : DT_LEFT;
        gdi.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold, align});
    }
    drawWords(s.picture, gdi);
    s.dirty = false;
    s.upload = true;
}

bool toWindow(const State& s, HWND game, POINT client, float& wx, float& wy) {
    RECT r{};
    if (!GetClientRect(game, &r) || r.right <= 0 || r.bottom <= 0 || s.targetW <= 0) return false;
    wx = float(client.x) * float(s.targetW) / float(r.right) - float(s.layout.x);  // uaRO.exe is not DPI-aware
    wy = float(client.y) * float(s.targetH) / float(r.bottom) - float(s.layout.y);
    return true;
}

bool inside(const State& s, float wx, float wy) {
    return wx >= 0 && wy >= 0 && wx < float(s.layout.w) && wy < float(s.layout.h);
}

void block(const State& s, HWND game) {
    RECT client{};
    if (!GetClientRect(game, &client) || s.targetW <= 0 || s.targetH <= 0) return;
    const refwin::Layout& L = s.layout;
    const RECT r{LONG(int64_t(L.x) * client.right / s.targetW), LONG(int64_t(L.y) * client.bottom / s.targetH),
                 LONG(int64_t(L.x + L.w) * client.right / s.targetW + 1),
                 LONG(int64_t(L.y + L.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, &r, false);
}

// A key's own character for the search (the names are English): letters, digits, the space and a few signs by the
// key's place, whatever the layout types
char letterOf(WPARAM vk, bool shift) {
    if (vk >= 'A' && vk <= 'Z') return char(shift ? vk : vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9' && !shift) return char(vk);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return char('0' + (vk - VK_NUMPAD0));
    switch (vk) {
        case VK_SPACE: return ' ';
        case VK_OEM_MINUS: return shift ? '_' : '-';
        case VK_OEM_7: return '\'';
        case VK_OEM_PERIOD: return '.';
        case VK_OEM_COMMA: return ',';
        default: return 0;
    }
}

}  // namespace

void frame(HWND game, bool enabled, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.enabled = enabled;
    if (s.open && !enabled) {
        close(s);
        LOG_INFO("reference: off ([interface] reference = 0) - the window closed");
    }
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Reference, nullptr, false);
        s.shown = false;
        return;
    }
    const std::string here = readGameMap();
    if (here != s.here) {  // 3.0.11-beta.8: «ви тут» and the routes are from the player's map
        s.here = here;
        s.wantPage = true;
    }
    if (float(k) != s.k || targetWidth != s.targetW || targetHeight != s.targetH) {
        s.k = float(k);
        s.targetW = targetWidth;
        s.targetH = targetHeight;
        s.layout = refwin::layout(targetWidth, targetHeight, std::clamp(float(k), 0.75f, 6.0f));
        s.wantPage = true;
    }
    const auto index = reference::index();
    if (index != s.index) {
        s.index = index;
        s.wantPage = true;
    }
    const auto online = reference::online();
    if (online != s.online) {
        s.online = online;
        s.wantPage = true;
    }
    if (s.wantPage) makePage(s);
    int cx = 0, cy = 0;  // the row under the game's cursor (it is drawn over the window)
    refwin::Under hover;
    if (readGameCursor(cx, cy))
        hover = refwin::under(s.layout, s.page, s.rows, s.scroll, !s.back.empty(),
                              float(cx) * float(k) - float(s.layout.x), float(cy) * float(k) - float(s.layout.y));
    if (hover.part != s.hover.part || hover.row != s.hover.row) {
        s.hover = hover;
        s.dirty = true;
    }
    if (s.dirty) render(s);
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.picture.width <= 0 || s.k <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.enabled) return false;
    const bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0, shift = GetKeyState(VK_SHIFT) < 0;
    if ((m == WM_CHAR || m == WM_SYSCHAR) && s.skipChar && wp == s.skipChar) {  // Ctrl+F's or Esc's own character
        s.skipChar = 0;
        return true;
    }
    if (m == WM_KEYDOWN && wp == 'F' && ctrl && !alt) {
        if (s.open) {
            close(s);
        } else {
            openWindow(s, refpages::Link{});
            LOG_INFO("reference: opened by Ctrl+F%s", s.index ? "" : " (no pack read yet)");
        }
        s.skipChar = 6;  // Ctrl+F's WM_CHAR
        return true;
    }
    if (!s.open) return false;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        const int line = std::max(1, s.layout.bodyH / 12);
        switch (wp) {
            case VK_ESCAPE:
                close(s);
                s.skipChar = 27;
                return true;
            case VK_LEFT:
                if (alt) goBack(s);
                return true;
            case VK_BACK:
                if (!s.query.empty()) {
                    s.query.pop_back();
                    typed(s);
                } else if (s.at.go != refpages::Go::None || !s.back.empty()) {
                    goBack(s);
                }
                s.skipChar = 8;
                return true;
            case VK_DELETE:
                if (!s.query.empty()) {
                    s.query.clear();
                    typed(s);
                }
                return true;
            case VK_RETURN:  // the first row that leads somewhere
                for (const refpages::Row& r : s.page.rows)
                    if (r.link.go != refpages::Go::None) {
                        go(s, r.link);
                        break;
                    }
                s.skipChar = 13;
                return true;
            case VK_PRIOR: scrollBy(s, -s.layout.bodyH * 9 / 10); return true;
            case VK_NEXT: scrollBy(s, s.layout.bodyH * 9 / 10); return true;
            case VK_UP: scrollBy(s, -line); return true;
            case VK_DOWN: scrollBy(s, line); return true;
            case VK_HOME: scrollBy(s, -s.total); return true;
            case VK_END: scrollBy(s, s.total); return true;
            default: break;
        }
        const char c = ctrl || alt ? 0 : letterOf(wp, shift);
        if (c && s.query.size() < kMostTyped && !(c == ' ' && (s.query.empty() || s.query.back() == ' '))) {
            s.query += c;
            typed(s);
        }
        return true;  // every key: the window's while it is open
    }
    return m == WM_KEYUP || m == WM_SYSKEYUP || m == WM_CHAR || m == WM_SYSCHAR || m == WM_DEADCHAR ||
           m == WM_SYSDEADCHAR;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.open || !s.shown) return false;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    float wx = 0, wy = 0;
    if (!toWindow(s, game, p, wx, wy) || !inside(s, wx, wy)) return false;  // outside: the game's
    switch (m) {
        case WM_MOUSEWHEEL:
            scrollBy(s, -GET_WHEEL_DELTA_WPARAM(wp) * std::max(1, s.layout.bodyH / 8) / WHEEL_DELTA);
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            const refwin::Under u = refwin::under(s.layout, s.page, s.rows, s.scroll, !s.back.empty(), wx, wy);
            if (u.part == refwin::Part::Close) close(s);
            if (u.part == refwin::Part::Back) goBack(s);
            if (u.part == refwin::Part::Row) go(s, s.page.rows[size_t(u.row)].link);
            break;
        }
        case WM_XBUTTONDOWN:
            if (GET_XBUTTON_WPARAM(wp) == XBUTTON1) goBack(s);
            result = TRUE;
            return true;
        case WM_XBUTTONUP:
            result = TRUE;
            return true;
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            break;
        default:
            return false;
    }
    result = 0;
    return true;
}

void openMap(const std::string& code) {
    State& s = state();
    if (!s.enabled || code.empty()) return;
    openWindow(s, refpages::Link{refpages::Go::Map, 0, code});
    LOG_INFO("reference: %s's card from the minimap's line", code.c_str());
}

void openMob(int id) {
    State& s = state();
    if (!s.enabled || id <= 0) return;
    openWindow(s, refpages::Link{refpages::Go::Mob, id, ""});
    LOG_INFO("reference: monster %d's card from the minimap's list", id);
}

}  // namespace uagfx::win::referencewin
