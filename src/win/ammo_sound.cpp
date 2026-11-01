#include "win/ammo_sound.hpp"

#include <windows.h>

#include <cstdint>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/ui_chime.hpp"
#include "win/ammo_watch.hpp"
#include "win/log.hpp"
#include "win/sound_hook.hpp"

namespace uagfx::win::ammosound {
namespace {

struct State {
    bool on = true;
    hotbar::AmmoWarn warn;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

// The chimes made once and kept: the game's player reads them while they play
const std::vector<uint8_t>& chimeOf(chime::Kind k) {
    static const std::vector<uint8_t> low = chime::wav(chime::Kind::AmmoLow);
    static const std::vector<uint8_t> out = chime::wav(chime::Kind::AmmoOut);
    return k == chime::Kind::AmmoOut ? out : low;
}

}  // namespace

void frame(bool on) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("ammo: [interface] ammo_sound = %d", on ? 1 : 0);
    }
    const ULONGLONG now = GetTickCount64();
    const ammowatch::Ammo& a = ammowatch::read(now);
    s.warn.now(a.count, now);  // watched with the sound off too: switched on, the old fall is not told
    if (!on || s.warn.fresh < 2) return;
    const chime::Kind k = s.warn.fresh == 3 ? chime::Kind::AmmoOut : chime::Kind::AmmoLow;
    const bool played = win::playOwn(chimeOf(k).data());
    if (s.told++ < 20)
        LOG_INFO("ammo: the chime - %s (%d left) %s", s.warn.fresh == 3 ? "none left" : "10 or fewer", a.count,
                 played ? "played" : "not played: the game's sound is off");
}

}  // namespace uagfx::win::ammosound
