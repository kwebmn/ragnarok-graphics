#include "win/key_hint.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/key_hints.hpp"
#include "core/layout.hpp"
#include "core/ui_style.hpp"
#include "win/cart_window.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/pad_hint.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"
#include "win/storage_window.hpp"
#include "win/time_lapse.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/view3d.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::keyhintwin {
namespace {

namespace kh = uagfx::keyhint;

struct State {
    uisheet::Sheet sheet;
    int gen = 0;
    bool shown = false;
    kh::Shower shower;
    kh::Seen seen;
    bool seenRead = false, wasOff = false;
    std::vector<kh::Part> parts;
    kh::Layout layout;
    std::string key;
    uint32_t casts = 0;
    int lapseWas = 0;  // 3.0.22-beta.24: Ctrl+Shift+T's state the last frame
    kh::Moment last = kh::Moment::Count;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string path() { return layout::join(gameDir(), layout::kKeyHints); }

void keep(State& s) {
    if (!writeFileAtomic(path(), kh::write(s.seen))) LOG_INFO("key hints: %s cannot be written", path().c_str());
}

void fill(State& s, uisheet::Content& c) {
    const kh::Layout& L = s.layout;
    const float sc = L.s;
    c.w = float(L.w), c.h = float(L.h);
    c.rect(c.under, L.glass, L.glass.h / 2, 0x10141C, std::max(0.88f, uistyle::panel()), 0xFFFFFF, 0.10f, 1.0f * sc);
    for (const kh::Layout::Placed& p : L.parts)
        for (const panel::Box& cap : p.caps)
            c.rect(c.under, cap, 5 * sc, kh::kCapFill, 0.95f, kh::kCapEdge, 0.28f, 1.0f * sc);
    c.say(kh::words(L, s.parts));
}

}  // namespace

void frame(HWND game, const Config& config, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.shown = false;
    if (!config.keyHints) {
        s.wasOff = true;
    } else {
        if (!s.seenRead) {
            s.seenRead = true;
            std::string text;
            if (readTextFile(path(), text)) s.seen = kh::read(text);
        }
        if (s.wasOff) {  // off and on again: every hint anew
            s.wasOff = false;
            s.seen = kh::Seen{};
            keep(s);
            LOG_INFO("key hints: on again - every hint shows anew");
        }
        kh::Now n;
        const bool front = game && GetForegroundWindow() == game;
        n.view3d = view3dwin::active();
        n.behind = view3dwin::behind();
        n.looking = view3dwin::looking();
        n.windowOpen = windowstack::top().has_value();
        n.altHeld = front && GetKeyState(VK_MENU) < 0;
        n.freeCamera = view3dwin::freeCamera();
        n.skillWait = view3dwin::skillWait();
        n.itemsShared = storagewin::shown() || cartwin::shown();
        n.flightPoints = view3dwin::flightPoints();  // 3.0.22-beta.24
        n.flying = view3dwin::flying();
        n.lapse = timelapsewin::state();
        std::array<bool, kh::kMoments> done{};
        // what the player has just done of what the lines say: Alt with a window open in the 3D view, a skill cast by
        // a click on the crosshair, a thing picked in the storage, the cart or the bag beside them
        done[size_t(kh::Moment::View3dWindow)] = n.view3d && n.windowOpen && n.altHeld;
        const uint32_t casts = view3dwin::clickCasts();
        done[size_t(kh::Moment::View3dSkill)] = casts != s.casts;
        s.casts = casts;
        // 3.0.22-beta.24: a flight flown; the time-lapse taken round to the clock's own again
        done[size_t(kh::Moment::Flight)] = n.flying;
        done[size_t(kh::Moment::TimeLapse)] = s.lapseWas == 2 && n.lapse == 0;
        s.lapseWas = n.lapse;
        done[size_t(kh::Moment::ItemPicks)] =
            n.itemsShared && storagewin::pickedCount() + cartwin::pickedCount() + itemswin::pickedCount() > 0;
        bool changed = false;
        const kh::Moment m = s.shower.frame(GetTickCount(), kh::momentsOf(n), done, s.seen, changed);
        if (changed) keep(s);
        if (m != s.last && m != kh::Moment::Count && s.told++ < 30)
            LOG_INFO("key hints: «%s» shown (%d of %d times so far)", kh::keyOf(m), s.seen.times[size_t(m)],
                     kh::kTimes);
        for (int i = 0; i < kh::kMoments; ++i)
            if (done[size_t(i)] && changed && s.seen.learned[size_t(i)] && s.told++ < 30)
                LOG_INFO("key hints: «%s» learned - not shown again", kh::keyOf(kh::Moment(i)));
        s.last = m;
        if (m != kh::Moment::Count && game && k > 0 && targetWidth > 0 && targetHeight > 0) {
            s.parts = kh::partsOf(m, n, language());
            const float scale = uistyle::partScale(k);
            // over the skill bar where the controller's line stands; over that line when it shows
            const int bottom = targetHeight - int(std::lround((padhintwin::shown() ? 150 : 104) * scale));
            s.layout = kh::layout(s.parts, targetWidth / 2, bottom, scale);
            std::string key = std::to_string(s.layout.w) + '#' + std::to_string(s.layout.y);
            for (const kh::Part& p : s.parts) key += p.keys + '|' + p.words + '|';
            key += '#' + std::to_string(uistyle::generation()) + (uaui::readyAnywhere() ? "u" : "");
            if (key != s.key) {
                s.key = key;
                ++s.gen;
            }
            s.shown = true;
        }
    }
    s.sheet.top = true;  // over the windows: a window open is one of its moments
    s.sheet.frame("key hint", s.shown, false, s.layout.x, s.layout.y, s.gen,
                  [&](uisheet::Content& c) { fill(s, c); });
}

}  // namespace uagfx::win::keyhintwin
