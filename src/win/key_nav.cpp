#include "win/key_nav.hpp"

#include "pad/core/logic/pad.hpp"
#include "win/chat_window.hpp"
#include "win/log.hpp"

namespace uagfx::win::keynav {
namespace {

struct State {
    int top = -1;           // the stack's top window last seen
    bool focus = false;     // the top window has the keys' focus (opened or clicked last)
    bool engaged = false;   // an arrow pressed: the keys drive it
    uint16_t held = 0;      // the pad bits the keys hold
    uint16_t taken = 0;     // pressed keys of ours not let go yet (their let go swallowed)
    uint16_t gap = 0;       // a held arrow's auto repeat: let go for one read, so the window steps again
    int skipChars = 0;      // Enter's, Space's, Esc's letters not given to the game
};

State& state() {
    static State s;
    return s;
}

// The pad bit a key stands for in a window; 0 none
uint16_t bitOf(WPARAM vk) {
    const bool shift = GetKeyState(VK_SHIFT) < 0;
    switch (vk) {
    case VK_UP: return uapad::kPadUp;
    case VK_DOWN: return uapad::kPadDown;
    case VK_LEFT: return uapad::kPadLeft;
    case VK_RIGHT: return uapad::kPadRight;
    case VK_RETURN: return uapad::kPadA;
    case VK_ESCAPE: return uapad::kPadB;
    case VK_SPACE: return uapad::kPadX;
    case VK_PRIOR: return uapad::kPadLB;
    case VK_NEXT: return uapad::kPadRB;
    case VK_APPS: return uapad::kPadY;
    case VK_F10: return shift ? uapad::kPadY : 0;
    default: return 0;
    }
}

bool arrow(WPARAM vk) { return vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT; }

void release(State& s, const char* why) {
    if (!s.engaged) return;
    s.engaged = false;
    s.held = 0;
    LOG_INFO("keys: the window lets the keys go (%s)", why);
}

}  // namespace

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (m == WM_CHAR) {
        if (s.skipChars <= 0 || (wp != '\r' && wp != ' ' && wp != 0x1B)) return false;
        --s.skipChars;
        return true;
    }
    if (m != WM_KEYDOWN && m != WM_KEYUP && m != WM_SYSKEYDOWN) return false;
    const uint16_t bit = bitOf(wp);
    if (!bit) return false;
    if (m == WM_KEYUP) {
        s.held = uint16_t(s.held & ~bit);
        if (!(s.taken & bit)) return false;
        s.taken = uint16_t(s.taken & ~bit);
        return true;
    }
    if (!s.focus || s.top < 0) return false;
    if (!chatwin::inputEmpty() || !chatwin::focusInChat()) {  // words in the chat line: theirs
        release(s, "words in the chat line");
        return false;
    }
    if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) return false;
    if (!s.engaged) {
        if (!arrow(wp)) return false;  // only an arrow starts it: Enter, Space, Esc stay the game's till then
        s.engaged = true;
        LOG_INFO("keys: the top window (%d) takes the arrows, Enter, Esc, Space, PgUp / PgDn", s.top);
    }
    if ((s.held & bit) && arrow(wp)) s.gap = uint16_t(s.gap | bit);  // held: a step each repeat
    s.held = uint16_t(s.held | bit);
    s.taken = uint16_t(s.taken | bit);
    if (wp == VK_RETURN || wp == VK_SPACE || wp == VK_ESCAPE) ++s.skipChars;
    return true;
}

void click(bool onWindow) {
    State& s = state();
    if (onWindow) {
        s.focus = true;
        return;
    }
    s.focus = false;
    release(s, "a click on the ground");
}

void frame(int top) {
    State& s = state();
    if (top == s.top) return;
    s.top = top;
    if (top < 0) {
        s.focus = false;
        release(s, "no window open");
        return;
    }
    s.focus = true;  // a window opened (or the one under it came up): it takes the keys' focus
}

bool engaged() {
    const State& s = state();
    return s.engaged && s.focus && s.top >= 0;
}

uint16_t buttons() {
    State& s = state();
    if (!engaged()) return 0;
    const uint16_t now = uint16_t(s.held & ~s.gap);
    s.gap = 0;
    return now;
}

void lost() {
    State& s = state();
    s.held = 0;
}

}  // namespace uagfx::win::keynav
