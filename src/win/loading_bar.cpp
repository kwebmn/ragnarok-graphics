#include "win/loading_bar.hpp"

#include <cstdint>

#include "core/loading_bar.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::loadingbarwin {
namespace {

namespace lb = uagfx::loadingbar;

// uaRO.exe's: the loading window (the manager's +0x78, its class's vtable; FUN_0046b440 makes it 240 x 15 with its
// bar at +0x64), the bar (its vtable, FUN_004493d0; its per cent at +0x48 - a float 0-100 the window's message 0x11
// sets, FUN_004493b0), a window's place (+0x14 x, +0x18 y, +0x1C w, +0x20 h) and its shown flag
constexpr uint32_t kLoadingAt = 0x727ED8 + 0x78, kLoadingClass = 0x698408, kBarAt = 0x64, kBarClass = 0x69690C;
constexpr uint32_t kProgress = 0x48, kX = 0x14, kY = 0x18, kW = 0x1C, kH = 0x20, kShown = 0x38;
constexpr int kAway = -4000;

struct State {
    uisheet::Sheet sheet;
    bool on = false, moved = false, told = false, made = false;
    uint32_t window = 0;
    int oldX = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t loadingWindow() {
    const uint32_t w = read32(kLoadingAt);
    return w && read32(w) == kLoadingClass ? w : 0;
}

// The game's bar off the screen (its place kept) while ours shows
void moveAway(State& s, uint32_t w) {
    if (w != s.window) {  // another window (or none): nothing of ours on it
        s.window = w;
        s.moved = false;
    }
    if (!s.on || !w || !read32(w + kShown)) return;
    const int x = int(int32_t(read32(w + kX)));
    if (x == kAway) return;
    s.oldX = x;  // the place the game gave it
    write32(w + kX, uint32_t(kAway));
    s.moved = true;
}

}  // namespace

void beforeGameDraw() {
    State& s = state();
    moveAway(s, loadingWindow());
}

void frame(bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (!s.made) {  // over everything, the login's and the loading's screens too
        s.made = true;
        s.sheet.anywhere = true;
        s.sheet.top = true;
    }
    if (on != s.on) {
        s.on = on;
        LOG_INFO("loading bar: [interface] loading_bar = %d", on ? 1 : 0);
    }
    const uint32_t w = loadingWindow();
    moveAway(s, w);
    const bool shown = w && read32(w + kShown);
    if (w && s.moved && (!on || !shown)) {  // off, or hidden by the game: its own place back
        write32(w + kX, uint32_t(s.oldX));
        s.moved = false;
    }
    const bool show = on && shown && s.moved && k > 0 && targetWidth > 0 && targetHeight > 0;
    lb::Layout L;
    float progress = 0;
    if (show) {
        const uint32_t bar = read32(w + kBarAt);
        if (!bar || read32(bar) != kBarClass || !readGame(bar + kProgress, &progress, sizeof progress)) progress = 0;
        L = lb::layout(s.oldX, int(int32_t(read32(w + kY))), int(int32_t(read32(w + kW))),
                       int(int32_t(read32(w + kH))), k, progress);
        if (!s.told) {
            s.told = true;
            LOG_INFO("loading bar: the game's (%dx%d at %d,%d) off the screen, the new one at %.0f,%.0f %.0fx%.0f - "
                     "%s",
                     int(int32_t(read32(w + kW))), int(int32_t(read32(w + kH))), s.oldX, int(int32_t(read32(w + kY))),
                     double(L.track.x), double(L.track.y), double(L.track.w), double(L.track.h), L.percent.c_str());
        }
    }
    const int gen = show ? int(L.track.x) * 7919 + int(L.track.y) * 131 + int(L.track.w) * 17 + int(L.track.h) +
                               int(L.fill.w) * 3 + int(L.percent.size()) * 101
                         : 0;
    s.sheet.frame("loading bar", show, true, int(L.track.x), int(L.track.y), gen, [&](uisheet::Content& c) {
        c.w = L.track.w, c.h = L.track.h;
        const panel::Box track{0, 0, L.track.w, L.track.h};
        c.rect(c.under, track, L.radius, 0x10141C, 0.88f, 0xFFFFFF, 0.22f, L.edge);
        const panel::Box fill{L.fill.x - L.track.x, L.fill.y - L.track.y, L.fill.w, L.fill.h};
        c.rect(c.over, fill, fill.h / 2, 0xC9A55B, 0.95f);  // the theme's gold
        c.words.push_back(uisheet::Word{0, 0, L.track.w, L.track.h, L.px, L.track.h, L.percent,
                                        uisheet::Content::rgba(0xF4F1EA, 1), "word bold centre halo"});
    });
}

}  // namespace uagfx::win::loadingbarwin
