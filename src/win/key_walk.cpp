#include "win/key_walk.hpp"

#include <string>

#include "core/key_walk.hpp"
#include "pad/win/runtime.hpp"
#include "win/log.hpp"
#include "win/chat_window.hpp"
#include "win/game_camera.hpp"

namespace uagfx::win::keywalk {
namespace {

constexpr ULONGLONG kStopTickMs = 600;  // after the last let go: the controller still takes the keys' (zero) walk

struct State {
    bool enabled = true;
    bool space = true;       // 3.0.18-beta.209: [interface] space_action
    bool spaceDown = false;  // Space pressed as the action (its let go and its letter swallowed too)
    bool skipSpace = false;
    uint8_t camHeld = 0;     // Ctrl+arrows pressed as the camera's (one step a press)
    std::string spaceMap;    // the map Space's first action was told on
    uint8_t held = 0;
    uint8_t mode = 0;  // 3.0.18-beta.208: the walk mode's keys
    ULONGLONG letGoAt = 0;
    std::string toldMap;  // the map the first walk was told on
    int unseen = 0;       // 3.0.18-beta.215: let goes another hook took (told 20 times)
};

State& state() {
    static State s;
    return s;
}

bool modifier() { return GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0 || GetKeyState(VK_SHIFT) < 0; }

}  // namespace

// 3.0.18-beta.209: Space - the action; true: swallowed
bool spaceKey(State& s, UINT m) {
    if (m == WM_KEYUP) {
        const bool mine = s.spaceDown;
        s.spaceDown = false;
        return mine;
    }
    if (s.spaceDown) return true;  // its auto repeat: one action a press
    if (!s.space || !inGameWorld() || modifier() || !chatwin::focusInChat() || !chatwin::inputEmpty() ||
        chatwin::gameBattle())  // the game's battle mode: its Space opens the chat
        return false;
    s.spaceDown = true;
    s.skipSpace = true;
    uapad::runtime::keyAction(uapad::Action{uapad::Action::Kind::Attack, 0});
    const std::string map = readGameMap();
    if (map != s.spaceMap) {
        s.spaceMap = map;
        LOG_INFO("keys: Space acts on %s - the target, else loot beside, else the nearest monster, else the NPC near",
                 map.c_str());
    }
    return true;
}

// 3.0.18-beta.209: Ctrl+arrows - the camera, a step a press
bool cameraKey(State& s, UINT m, uint8_t dir) {
    if (m == WM_KEYUP) {
        const bool mine = (s.camHeld & dir) != 0;
        s.camHeld = uint8_t(s.camHeld & ~dir);
        return mine;
    }
    if (s.camHeld & dir) return true;
    if (!s.enabled || !inGameWorld() || !chatwin::focusInChat() || !chatwin::inputEmpty()) return false;
    s.camHeld = uint8_t(s.camHeld | dir);
    using uagfx::keywalk::Dir;
    uapad::runtime::keyCamera(dir == Dir::kLeft ? -45.0f : dir == Dir::kRight ? 45.0f : 0.0f,
                              dir == Dir::kUp ? 1 : dir == Dir::kDown ? -1 : 0);
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM) {
    State& s = state();
    if (m == WM_CHAR) {
        const bool skip = s.skipSpace && wp == ' ';
        s.skipSpace = false;
        return skip;
    }
    if (m != WM_KEYDOWN && m != WM_KEYUP) return false;
    if (m == WM_KEYDOWN && wp != VK_SPACE) s.skipSpace = false;
    if (wp == VK_SPACE) return spaceKey(s, m);
    const uint8_t dir = uagfx::keywalk::dirOfVk(unsigned(wp));
    if (!dir) return false;
    const bool ctrlOnly = GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0 && GetKeyState(VK_SHIFT) >= 0;
    if ((s.camHeld & dir) || (m == WM_KEYDOWN && ctrlOnly && !(s.held & dir))) return cameraKey(s, m, dir);
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
    g.otherField = !chatwin::focusInChat();  // the final review: a count's or a name's field keeps its arrows
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

bool keyDown(unsigned vk) { return GetKeyState(int(vk)) < 0; }

bool vector(float& x, float& y) {
    State& s = state();
    if (s.held) {  // 3.0.18-beta.215 (R-8KPDY9: the walk mode switched on took an arrow's let go, the walk went on)
        const uint8_t still = uagfx::keywalk::heldNow(s.held, keyDown);
        if (still != s.held) {
            if (s.unseen++ < 20)
                LOG_INFO("keys: an arrow's let go taken by another hook (%u) - Windows holds it no more",
                         unsigned(s.held & ~still));
            s.held = still;
            if (!s.held) s.letGoAt = GetTickCount64();
        }
    }
    const uagfx::keywalk::Vec v = uagfx::keywalk::vectorOf(uint8_t(s.held | s.mode));
    x = v.x;
    y = v.y;
    if (s.held || s.mode) return true;
    return s.letGoAt != 0 && GetTickCount64() - s.letGoAt < kStopTickMs;
}

bool walking() { return state().held != 0 || state().mode != 0; }

void setModeDirs(uint8_t dirs) {
    State& s = state();
    if (dirs == s.mode) return;
    s.mode = dirs;
    if (!s.held && !s.mode) s.letGoAt = GetTickCount64();
}

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

void setSpace(bool on) {
    State& s = state();
    if (on == s.space) return;
    s.space = on;
    LOG_INFO("keys: Space as the action %s", on ? "on" : "off");
}

}  // namespace uagfx::win::keywalk
