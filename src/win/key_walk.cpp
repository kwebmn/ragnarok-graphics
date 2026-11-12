#include "win/key_walk.hpp"

#include <string>

#include "core/key_walk.hpp"
#include "win/log.hpp"
#include "win/chat_window.hpp"
#include "win/game_camera.hpp"

namespace uagfx::win::keywalk {
namespace {

constexpr ULONGLONG kStopTickMs = 600;  // after the last let go: the controller still takes the keys' (zero) walk

struct State {
    bool enabled = true;
    uint8_t held = 0;
    ULONGLONG letGoAt = 0;
    std::string toldMap;  // the map the first walk was told on
};

State& state() {
    static State s;
    return s;
}

bool modifier() { return GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0 || GetKeyState(VK_SHIFT) < 0; }

}  // namespace

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (m != WM_KEYDOWN && m != WM_KEYUP) return false;
    const uint8_t dir = uagfx::keywalk::dirOfVk(unsigned(wp));
    if (!dir) return false;
    if (m == WM_KEYUP) {
        if (!(s.held & dir)) return false;  // pressed when it was not the walk's: the game's
        s.held = uint8_t(s.held & ~dir);
        if (!s.held) s.letGoAt = GetTickCount64();
        return true;
    }
    if (s.held & dir) return true;  // its auto repeat
    uagfx::keywalk::Gate g;
    g.enabled = s.enabled;
    const std::string map = readGameMap();
    g.inGame = !map.empty();
    g.chatEmpty = chatwin::inputEmpty();
    g.modifier = modifier();
    if (!uagfx::keywalk::arrowWalks(g)) return false;
    s.held = uint8_t(s.held | dir);
    if (map != s.toldMap) {
        s.toldMap = map;
        LOG_INFO("keys: the arrows walk on %s (the chat line empty, [interface] arrows_walk on)", map.c_str());
    }
    return true;
}

void lost() {
    State& s = state();
    if (!s.held) return;
    s.held = 0;
    s.letGoAt = GetTickCount64();
    LOG_INFO("keys: the window lost the keys - the arrows let go, the walk stops");
}

bool vector(float& x, float& y) {
    const State& s = state();
    const uagfx::keywalk::Vec v = uagfx::keywalk::vectorOf(s.held);
    x = v.x;
    y = v.y;
    if (s.held) return true;
    return s.letGoAt != 0 && GetTickCount64() - s.letGoAt < kStopTickMs;
}

bool walking() { return state().held != 0; }

void setEnabled(bool on) {
    State& s = state();
    if (on == s.enabled) return;
    s.enabled = on;
    if (!on && s.held) {
        s.held = 0;
        s.letGoAt = GetTickCount64();
    }
    LOG_INFO("keys: walking with the arrows %s", on ? "on" : "off");
}

bool enabled() { return state().enabled; }

}  // namespace uagfx::win::keywalk
