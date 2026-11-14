#include "win/walk_mode.hpp"

#include "core/walk_mode.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_window.hpp"
#include "win/game_camera.hpp"
#include "win/key_walk.hpp"
#include "win/log.hpp"
#include "win/setting_edit.hpp"

namespace uagfx::win::walkmodewin {
namespace {

constexpr uint32_t kBarRow = 0x771458;  // the hotkey bar's row shown (0-2), as win/hotbar reads it

struct State {
    walkmode::Mode mode;
    walkmode::FileFollow file;  // [interface] walk_mode: the settings' changes, not the toggle's own writes (.214)
    bool skipChar = false;   // the toggle's letter (` ё ') not typed
    int unseen = 0;          // 3.0.18-beta.215: let goes another hook took (told 20 times)
    const Config* config = nullptr;
    std::string iniPath;
};

State& state() {
    static State s;
    return s;
}

void tellDirs(const State& s) { keywalk::setModeDirs(s.mode.on() ? s.mode.dirs() : 0); }

// 3.0.18-beta.209 (stage 3): a quick action's key - the pad controller fires it at its next tick
void doAction(const walkmode::Mode& m) {
    using walkmode::Do;
    using uapad::Action;
    switch (m.action()) {
    case Do::Attack: uapad::runtime::keyAction(Action{Action::Kind::Attack, 0}); break;
    case Do::Pickup: uapad::runtime::keyAction(Action{Action::Kind::Pickup, 0}); break;
    case Do::Talk: uapad::runtime::keyAction(Action{Action::Kind::Talk, 0}); break;
    case Do::Sit: uapad::runtime::keyAction(Action{Action::Kind::SitStand, 0}); break;
    case Do::NextTarget: uapad::runtime::keyCycleTarget(1); break;
    case Do::PrevTarget: uapad::runtime::keyCycleTarget(-1); break;
    case Do::CameraLeft: uapad::runtime::keyCamera(-45, 0); break;
    case Do::CameraRight: uapad::runtime::keyCamera(45, 0); break;
    case Do::Slot: {  // 1-9: the row the game shows (F12 turns it)
        uint8_t row = 0;
        readGame(kBarRow, &row, 1);
        uapad::runtime::keyAction(Action{Action::Kind::Slot, uint8_t((row < 3 ? row : 0) * 9 + m.slot())});
        break;
    }
    case Do::None: break;
    }
}

}  // namespace

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (m == WM_CHAR) {
        if (s.skipChar) {
            s.skipChar = false;
            return true;
        }
        // the mode walking and the chat's field in focus: no letter goes into it
        return s.mode.on() && !s.mode.typing() && inGameWorld() && chatwin::focusInChat();
    }
    if (m != WM_KEYDOWN && m != WM_KEYUP) return false;
    if (m == WM_KEYDOWN) s.skipChar = false;  // a layout whose ` gives no letter (a dead key): nothing else skipped
    if (!inGameWorld() || !chatwin::focusInChat()) return false;  // the login, a name's or a number's field: theirs
    walkmode::Key k;
    k.vk = unsigned(wp);
    k.scan = unsigned((lp >> 16) & 0xFF);
    k.extended = ((lp >> 24) & 1) != 0;
    k.down = m == WM_KEYDOWN;
    k.repeat = k.down && ((lp >> 30) & 1) != 0;  // 3.0.18-beta.215: the toggle once a press
    k.ctrl = GetKeyState(VK_CONTROL) < 0;
    k.alt = GetKeyState(VK_MENU) < 0;
    k.chatEmpty = chatwin::inputEmpty();
    const walkmode::Act act = s.mode.key(k);
    tellDirs(s);
    switch (act) {
    case walkmode::Act::Pass: return false;
    case walkmode::Act::Swallow:
        if (k.down && k.scan == walkmode::kToggleScan) s.skipChar = true;  // the toggle held: its letters not typed
        return true;
    case walkmode::Act::Toggle: {
        s.skipChar = true;
        LOG_INFO("keys: the walk mode %s (`)", s.mode.on() ? "on - W A S D walk, Enter opens the chat" : "off");
        if (s.config && !s.iniPath.empty()) {
            settingedit::set(s.iniPath, *s.config, "interface.walk_mode", s.mode.on() ? "1" : "0");
            s.file.wrote(GetTickCount64());
        }
        return true;
    }
    // the game's Enter would send the empty line; in the game's battle mode it is its own typing switch (without it the
    // game wipes the line at each key)
    case walkmode::Act::OpenChat: return !chatwin::gameBattle();
    case walkmode::Act::Send: return false;     // the game's Enter sends it
    case walkmode::Act::CancelChat: chatwin::clearLine(); return true;
    case walkmode::Act::Action: doAction(s.mode); return true;  // 3.0.18-beta.209
    }
    return false;
}

void lost() {
    State& s = state();
    s.mode.lost();
    tellDirs(s);
}

void frame(const Config& config, const std::string& iniPath) {
    State& s = state();
    s.config = &config;
    s.iniPath = iniPath;
    // 3.0.18-beta.215 (R-8KPDY9): a walking key's let go another hook took (a field of the game's took the focus)
    if (s.mode.verify(keywalk::keyDown)) {
        tellDirs(s);
        if (s.unseen++ < 20) LOG_INFO("keys: a walk mode key's let go taken by another hook - its walk stops");
    }
    // the start, or «Налаштування» switched it (3.0.18-beta.214: not the toggle's own write read back - R-CX44S5)
    if (s.file.follow(config.walkMode, GetTickCount64())) {
        if (s.mode.on() != config.walkMode) {
            s.mode.set(config.walkMode);
            tellDirs(s);
            LOG_INFO("keys: the walk mode %s by the settings", config.walkMode ? "on" : "off");
        }
    }
}

bool on() { return state().mode.on(); }

void setOn(bool on) {
    State& s = state();
    if (s.mode.on() == on) return;
    s.mode.set(on);
    tellDirs(s);
    LOG_INFO("keys: the walk mode %s by the 3D view", on ? "on" : "off");
}

bool typing() { return state().mode.typing(); }

}  // namespace uagfx::win::walkmodewin
