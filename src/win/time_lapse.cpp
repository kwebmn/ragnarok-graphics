#include "win/time_lapse.hpp"

#include "core/daylight.hpp"
#include "win/log.hpp"

namespace uagfx::win::timelapsewin {
namespace {

struct State {
    daylight::Lapse lapse;
    bool pressed = false;
    bool skipChar = false;
    int clock = -1;
    int target = -1;  // 3.0.22-beta.33: asked by the MCP
};

State& now() {
    static State s;
    return s;
}

}  // namespace

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = now();
    if (m == WM_CHAR && s.skipChar) {  // Ctrl+T's letter (0x14)
        s.skipChar = false;
        return wp == 0x14;
    }
    if (m == WM_KEYDOWN && wp == 'T' && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0 &&
        GetKeyState(VK_MENU) >= 0) {
        s.skipChar = true;
        if (((lp >> 30) & 1) == 0) s.pressed = true;  // held: once a press
        return true;
    }
    return false;
}

int frame(int clock, int shown, int perSecond) {
    State& s = now();
    const double t = double(GetTickCount64()) / 1000.0;
    if (clock != s.clock) {  // the «Годинник» chosen anew: its own time
        if (s.clock >= 0 && s.lapse.state() != daylight::Lapse::State::Off)
            LOG_INFO("time-lapse: off - the clock chosen anew");
        s.clock = clock;
        s.lapse.off();
    }
    if (s.target >= 0 && !s.pressed) {  // 3.0.22-beta.33: the MCP's - a press a frame until it is there
        const int st = s.lapse.state() == daylight::Lapse::State::Running ? 1
                       : s.lapse.state() == daylight::Lapse::State::Held  ? 2
                                                                           : 0;
        if (st == s.target) {
            s.target = -1;
        } else if (s.target == 0) {
            s.lapse.off();
            s.target = -1;
            LOG_INFO("time-lapse: the clock's own again (the MCP)");
        } else {
            s.pressed = true;
        }
    }
    if (s.pressed) {
        s.pressed = false;
        const int before = s.lapse.minute(t, perSecond);
        s.lapse.press(shown, t);
        const int at = s.lapse.minute(t, perSecond);
        switch (s.lapse.state()) {
            case daylight::Lapse::State::Running:
                LOG_INFO("time-lapse: Ctrl+Shift+T - the time runs from %02d:%02d, %d minutes a second", at / 60,
                         at % 60, perSecond);
                break;
            case daylight::Lapse::State::Held:
                LOG_INFO("time-lapse: Ctrl+Shift+T - stopped at %02d:%02d", at / 60, at % 60);
                break;
            case daylight::Lapse::State::Off:
                LOG_INFO("time-lapse: Ctrl+Shift+T - the clock's own again (it stood at %02d:%02d)", before / 60,
                         before % 60);
                break;
        }
    }
    return s.lapse.minute(t, perSecond);
}

void ask(int target) { now().target = target; }

int state() {
    switch (now().lapse.state()) {
        case daylight::Lapse::State::Running: return 1;
        case daylight::Lapse::State::Held: return 2;
        case daylight::Lapse::State::Off: break;
    }
    return 0;
}

}  // namespace uagfx::win::timelapsewin
