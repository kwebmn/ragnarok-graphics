#include "win/recap_window.hpp"

#include <windowsx.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/window_panel.hpp"
#include "pad/win/game_window.hpp"
#include "win/death_recap_watch.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/real_ddraw.hpp"
#include "win/uaui.hpp"
#include "win/window_input.hpp"
#include "win/words.hpp"

namespace uagfx::win::recapwin {
namespace {

using uapad::gamewindow::Blocker;
constexpr float kW = 380, kLineH = 19, kPad = 10, kTitleH = 24;

struct State {
    bool open = false, shown = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, x = 0, y = 0, w = 0, h = 0;
    panel::Box close;
    bool hoverClose = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="hpn" data-model="recap" data-style-width="w + 'px'" data-style-height="h + 'px'"
  data-style-border-radius="10 * s + 'px'" data-style-border-width="s + 'px'" data-style-background-color="ground">
<span class="title" data-style-left="10 * s + 'px'" data-style-top="10 * s + 'px'" data-style-width="w - 46 * s + 'px'"
  data-style-height="20 * s + 'px'" data-style-line-height="20 * s + 'px'" data-style-font-size="13 * s + 'px'"
  >{{title}}</span>
<span data-attr-class="closeHot ? 'ownx hot' : 'ownx'" data-style-left="w - 32 * s + 'px'"
  data-style-top="10 * s + 'px'" data-style-width="22 * s + 'px'" data-style-height="20 * s + 'px'"
  data-style-line-height="20 * s + 'px'" data-style-border-radius="6 * s + 'px'"
  data-style-font-size="12 * s + 'px'">&#x2715;</span>
<span data-for="l, i : lines" data-attr-class="i == 0 ? 'line first' : 'line'" data-style-left="10 * s + 'px'"
  data-style-top="(34 + 19 * i) * s + 'px'" data-style-width="w - 20 * s + 'px'" data-style-height="19 * s + 'px'"
  data-style-line-height="19 * s + 'px'" data-style-font-size="12 * s + 'px'">{{l}}</span>
</body></rml>)RML";

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, s = 1;
    bool closeHot = false;
    Rml::String title, ground = "#00000000";
    std::vector<Rml::String> lines;
    std::string key;
};

Ui& ui() {
    static Ui u;
    return u;
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Recap, nullptr, nullptr, false);
    s.shown = false;
    ui().doc.show(false);
}

}  // namespace

bool isOpen() { return state().open; }

void toggle(const char* why) {
    State& s = state();
    s.open = !s.open;
    LOG_INFO("recap: «Що мене вбило» %s (%s)", s.open ? "opened" : "closed", why);
}

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (!on) s.open = false;
    if (!s.open || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !uaui::ready()) return hide(s);
    std::string title;
    std::vector<std::string> lines;
    if (deathrecapwin::last(title, lines)) lines.insert(lines.begin(), title);
    else lines = {say(settings::Text::RecapNone)};
    s.k = float(k);
    s.scale = uistyle::partScale(k);
    s.targetW = targetWidth, s.targetH = targetHeight;
    s.w = int(std::lround(kW * s.scale));
    s.h = int(std::lround((2 * kPad + kTitleH + float(lines.size()) * kLineH) * s.scale));
    s.x = (targetWidth - s.w) / 2;
    s.y = int(targetHeight * 0.18f);
    s.close = panel::Box{float(s.w) - (kPad + 22) * s.scale, kPad * s.scale, 22 * s.scale, 20 * s.scale};
    s.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy))
        s.hoverClose = s.close.contains(float(cx) * float(k) - float(s.x), float(cy) * float(k) - float(s.y));
    Ui& u = ui();
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("recap", kDoc, [&](Rml::DataModelConstructor& m) {
                m.RegisterArray<std::vector<Rml::String>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("s", &u.s);
                m.Bind("closeHot", &u.closeHot);
                m.Bind("title", &u.title);
                m.Bind("ground", &u.ground);
                m.Bind("lines", &u.lines);
            }))
            return hide(s);
    }
    std::string key = std::to_string(s.w) + 'x' + std::to_string(s.h) + (s.hoverClose ? "c" : "") + '#' +
                      std::to_string(uistyle::generation());
    for (const std::string& l : lines) key += '|' + l;
    if (key != u.key) {
        u.key = key;
        u.w = float(s.w), u.h = float(s.h), u.s = s.scale, u.closeHot = s.hoverClose;
        char g[16];
        std::snprintf(g, sizeof g, "#%06x%02x", unsigned(uaui::themed(0x10141C)),
                      int(std::lround(uistyle::panel() * 255)) & 0xFF);
        u.ground = g;
        u.title = say(settings::Text::InterfaceDeathRecap);
        u.lines.assign(lines.begin(), lines.end());
        u.doc.dirtyAll();
    }
    u.doc.place(s.x, s.y);
    u.doc.show(true);
    s.shown = true;
    RECT client{};
    if (!GetClientRect(game, &client)) return;
    const RECT r = clientBox(client, targetWidth, targetHeight, s.x, s.y, s.w, s.h);
    uapad::gamewindow::setBlocked(Blocker::Recap, game, &r, false);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.x), ly = ty - float(s.y);
    if (lx < 0 || ly < 0 || lx >= float(s.w) || ly >= float(s.h)) return false;
    result = 0;
    if (m == WM_LBUTTONDOWN && s.close.contains(lx, ly)) {
        s.open = false;
        if (s.told++ < 10) LOG_INFO("recap: closed by its ✕");
    }
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (!s.open || m != WM_KEYDOWN || wp != VK_ESCAPE) return false;
    s.open = false;
    LOG_INFO("recap: closed by Esc");
    return true;
}

std::vector<padpanels::Button> padButtons() {
    State& s = state();
    if (!s.shown) return {};
    return {{panel::Box{float(s.x) + s.close.x, float(s.y) + s.close.y, s.close.w, s.close.h},
             [] { state().open = false; }}};
}

}  // namespace uagfx::win::recapwin
