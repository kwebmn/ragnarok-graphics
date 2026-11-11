#include "win/pad_panels.hpp"

#include <algorithm>
#include <cmath>

#include "win/hunt_panel.hpp"
#include "win/log.hpp"
#include "win/mvp_panel.hpp"
#include "win/recap_window.hpp"
#include "win/tracker.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::padpanels {
namespace {

struct State {
    bool on = false;
    closedwin::Own own = closedwin::Own::Hunt;
    int at = 0;
    std::vector<Button> buttons;
};

State& state() {
    static State s;
    return s;
}

std::vector<Button> buttonsOf(closedwin::Own o) {
    switch (o) {
        case closedwin::Own::Hunt: return huntwin::padButtons();
        case closedwin::Own::Mvp: return mvpwin::padButtons();
        case closedwin::Own::Tracker: return trackerwin::padButtons();
        case closedwin::Own::Recap: return recapwin::padButtons();
        default: return {};
    }
}

void ring(const panel::Box* b) {
    static uaui::Doc doc;
    static float w = 0, h = 0;
    if (!b || !uaui::ready()) {
        doc.show(false);
        return;
    }
    if (!doc.made()) {
        doc.passive = true;
        doc.top = true;
        if (!doc.make("pad panel ring", R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="ppr" data-model="padring" data-style-width="w + 'px'" data-style-height="h + 'px'">
<div class="r" data-style-width="w + 'px'" data-style-height="h + 'px'"></div></body></rml>)RML",
                      [&](Rml::DataModelConstructor& m) {
                          m.Bind("w", &w);
                          m.Bind("h", &h);
                      }))
            return;
    }
    const float d = 3 * uaui::dp();
    if (std::fabs(b->w + 2 * d - w) > 0.5f || std::fabs(b->h + 2 * d - h) > 0.5f) {
        w = b->w + 2 * d, h = b->h + 2 * d;
        doc.dirtyAll();
    }
    doc.place(int(std::lround(b->x - d)), int(std::lround(b->y - d)));
    doc.show(true);
}

}  // namespace

bool focus(closedwin::Own o) {
    State& s = state();
    std::vector<Button> b = buttonsOf(o);
    if (b.empty()) return false;
    s.on = true;
    s.own = o;
    s.at = 0;
    s.buttons = std::move(b);
    LOG_INFO("pad: a panel's %zu buttons take the controller", s.buttons.size());
    return true;
}

bool active() { return state().on; }

void command(Command c) {
    State& s = state();
    if (!s.on) return;
    const int n = int(s.buttons.size());
    switch (c) {
        case Command::Prev: s.at = n ? (s.at + n - 1) % n : 0; break;
        case Command::Next: s.at = n ? (s.at + 1) % n : 0; break;
        case Command::Press:
            if (s.at >= 0 && s.at < n && s.buttons[size_t(s.at)].press) s.buttons[size_t(s.at)].press();
            break;
        case Command::Leave: s.on = false; break;
    }
}

void frame() {
    State& s = state();
    if (s.on) {
        s.buttons = buttonsOf(s.own);
        if (s.buttons.empty()) s.on = false;  // the panel gone (closed by its ✕, its last entry taken off)
        else s.at = std::clamp(s.at, 0, int(s.buttons.size()) - 1);
    }
    ring(s.on ? &s.buttons[size_t(s.at)].box : nullptr);
}

}  // namespace uagfx::win::padpanels
